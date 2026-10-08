#include "worgoblin_loader.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Config.h"
#include "StringFormat.h"
#include "WorldSession.h"

#include <algorithm>
#include <string>

enum Spells
{
    BEST_DEALS_ANYWHERE = 69044,
};

// --- Two Forms ------------------------------------------------------------
//
// Spell 68996 "Two Forms" is complete in both the server's and the client's
// Spell.dbc - real text, real icon, infinite duration - but nothing put it on
// skill line 789 "Racial - Worgen", so no worgen ever learned it. The row is
// added in SQL (data/sql/db-world); the behaviour is here.
//
// In Cataclysm you pick two complete appearances at character creation, and
// the client switches between them through a dedicated aura type
// (WORGEN_ALTERED_FORM). Neither that aura type nor a second customization set
// on the character exists in a 3.3.5 client, so both are built here:
//
//   * The MODEL comes from UNIT_FIELD_DISPLAYID, but the client composes the
//     TEXTURES (skin, face, hair) from the race byte in UNIT_FIELD_BYTES_0
//     looked up in CharSections.dbc. Setting only the display gives a human
//     with worgen skin, so BOTH are set.
//   * The race byte is safe to change on a player: Unit::getRace() returns
//     m_race, not the field, and the only place the field is read is behind
//     an IsPlayer() check that has already returned. _SaveCharacter() saves
//     the race from getRace(true) and the gender from PLAYER_BYTES_3, exactly
//     because "UNIT_BYTES_0 changes with every transform effect" (the core's
//     own comment). The field is rebuilt from the database on every login.
//   * The five appearance values, on the other hand, live in
//     PLAYER_BYTES/PLAYER_BYTES_2, which ARE saved. So worgen_form_appearance
//     holds the human set, the wolf set, and the human sets a save in human
//     form may have left in `characters` (data/sql/db-characters).
//
// The aura is a WISH for human form (WantsHuman()): combat brings the wolf
// out, and the human form comes back when combat ends
// (Worgoblin.TwoForms.CombatShift). There is no "Calm the Wolf" spell in the
// 4.x spell data - only the retired 68951/68952 "zzOld Dan's Altered Form
// On/Off" - so that behaviour lives in Two Forms itself.
//
// Darkflight (68992) also switches to wolf form: its own description in
// Spell.dbc says "Activates your true form".
//
// The native display follows the form: Unit::IsInDisallowedMountForm()
// refuses mounts and taxis to a player whose UNIT_FIELD_DISPLAYID differs from
// UNIT_FIELD_NATIVEDISPLAYID. So in human form the native display is the human
// one, and the worgen model is looked up where it is needed (WorgenDisplay()).

enum WorgenTwoForms
{
    SPELL_TWO_FORMS      = 68996,
    SPELL_DARKFLIGHT     = 68992,
    RACE_WORGEN_ID       = 12,   // the free 3.3.5 race slot this module uses
    DISPLAY_HUMAN_MALE   = 49,
    DISPLAY_HUMAN_FEMALE = 50,
    // The same models as 49/50, also without CreatureDisplayInfoExtra (Redraw()).
    DISPLAY_HUMAN_MALE_REDRAW   = 907,
    DISPLAY_HUMAN_FEMALE_REDRAW = 302,
    NPC_GILNEAN_BARBER   = 9000001,
};

// The effect and sound Cataclysm plays when the form changes. Both are
// SpellVisualKit rows a stock 3.3.5 client does not have: they need the
// SpellVisualKit.dbc of MekBits' patch-Z (github.com/MekBits/azerothcore).
// Without them the client plays nothing.
enum WorgenTransformVisuals
{
    VISUAL_KIT_TO_WORGEN = 16173,   // Worgen_Combat_Transform_FX, "Worgen_Transform_Worgen"
    VISUAL_KIT_TO_HUMAN  = 28505,   // "Worgen_Transform_Human"
};

// Index into the two appearance arrays.
enum WorgenAppearanceIndex
{
    A_SKIN       = 0,
    A_FACE       = 1,
    A_HAIR       = 2,
    A_HAIRCOLOR  = 3,
    A_FACIALHAIR = 4,
    A_COUNT      = 5,
};

struct WorgenFormData : public DataMap::Base
{
    uint8 w[A_COUNT]     = { 0, 0, 0, 0, 0 };   // wolf form - the canonical one
    uint8 h[A_COUNT]     = { 0, 0, 0, 0, 0 };   // human form
    uint8 saved[A_COUNT] = { 0, 0, 0, 0, 0 };   // what the last save wrote
    bool  loaded     = false;
    bool  human      = false;   // the fields hold the human set
    bool  savedHuman = false;   // `saved` is a human set
    bool  updating   = false;   // inside UpdateForm()
    bool  redrawing  = false;   // the display may be the redraw display (Redraw())
    uint32 redrawId  = 0;       // which Redraw() the pending event belongs to
    uint32 loginTime = 0;       // GetInGameTime() at OnPlayerLogin
};

static bool sCombatShift = true;

static WorgenFormData* Forms(Player* player)
{
    return player->CustomData.GetDefault<WorgenFormData>("WorgenFormData");
}

static bool IsWorgenPlayer(Unit const* unit)
{
    // getRace() reads m_race, not UNIT_FIELD_BYTES_0, so it keeps saying
    // "worgen" also while the client draws a human.
    return unit && unit->IsPlayer() && unit->getRace() == RACE_WORGEN_ID;
}

// A shapeshift form with its own model; warrior stances have none.
static bool HasModelForm(Unit* unit)
{
    for (AuraEffect const* eff : unit->GetAuraEffectsByType(SPELL_AURA_MOD_SHAPESHIFT))
        if (unit->GetModelForForm(ShapeshiftForm(eff->GetMiscValue()), eff->GetId()))
            return true;
    return false;
}

// The newest transform other than Two Forms.
static AuraEffect* OtherTransform(Unit* unit)
{
    AuraEffect* other = nullptr;
    for (AuraEffect* eff : unit->GetAuraEffectsByType(SPELL_AURA_TRANSFORM))
        if (eff->GetId() != SPELL_TWO_FORMS)
            other = eff;
    return other;
}

// Never touch the display while a form with its own model (a druid form) or
// another transform (a costume, an illusion) is on. That aura owns the display
// then, and a SetDisplayId() from here would win and leave a "bear" that looks
// like a human. Warrior stances are shapeshift forms without a model, so they
// don't count.
static bool CanChangeForm(Unit* unit)
{
    return IsWorgenPlayer(unit) && !HasModelForm(unit) && !OtherTransform(unit);
}

// getGender() reads UNIT_FIELD_BYTES_0, which SetDisplayId() sets to the
// gender of the model, so a costume changes it. PLAYER_BYTES_3 holds the
// character's.
static uint8 NativeGender(Player* player)
{
    return player->GetByteValue(PLAYER_BYTES_3, 0);
}

static uint32 HumanDisplay(Player* player)
{
    return NativeGender(player) == GENDER_MALE ? DISPLAY_HUMAN_MALE : DISPLAY_HUMAN_FEMALE;
}

static uint32 RedrawDisplay(Player* player)
{
    return NativeGender(player) == GENDER_MALE ? DISPLAY_HUMAN_MALE_REDRAW : DISPLAY_HUMAN_FEMALE_REDRAW;
}

// A display this module sets in human form.
static bool IsHumanDisplay(uint32 display)
{
    return display == DISPLAY_HUMAN_MALE || display == DISPLAY_HUMAN_FEMALE ||
           display == DISPLAY_HUMAN_MALE_REDRAW || display == DISPLAY_HUMAN_FEMALE_REDRAW;
}

// The worgen model, as Player::InitDisplayIds() picks it. Not the native
// display: that is the human one in human form.
static uint32 WorgenDisplay(Player* player)
{
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(true), player->getClass());
    if (!info)
        return player->GetNativeDisplayId();
    return NativeGender(player) == GENDER_MALE ? info->displayId_m : info->displayId_f;
}

static void ReadAppearance(Player* player, uint8* out)
{
    out[A_SKIN]       = player->GetByteValue(PLAYER_BYTES, 0);
    out[A_FACE]       = player->GetByteValue(PLAYER_BYTES, 1);
    out[A_HAIR]       = player->GetByteValue(PLAYER_BYTES, 2);
    out[A_HAIRCOLOR]  = player->GetByteValue(PLAYER_BYTES, 3);
    out[A_FACIALHAIR] = player->GetByteValue(PLAYER_BYTES_2, 0);
}

static void WriteAppearance(Player* player, uint8 const* in)
{
    player->SetByteValue(PLAYER_BYTES, 0, in[A_SKIN]);
    player->SetByteValue(PLAYER_BYTES, 1, in[A_FACE]);
    player->SetByteValue(PLAYER_BYTES, 2, in[A_HAIR]);
    player->SetByteValue(PLAYER_BYTES, 3, in[A_HAIRCOLOR]);
    player->SetByteValue(PLAYER_BYTES_2, 0, in[A_FACIALHAIR]);
}

static bool ClampHuman(Player* player, uint8* a);

static bool SameSet(uint8 const* a, uint8 const* b)
{
    return std::equal(a, a + A_COUNT, b);
}

static std::string SqlSet(uint8 const* a)
{
    if (!a)
        return "NULL,NULL,NULL,NULL,NULL";
    return Acore::StringFormat("{},{},{},{},{}", uint32(a[A_SKIN]), uint32(a[A_FACE]), uint32(a[A_HAIR]),
                               uint32(a[A_HAIRCOLOR]), uint32(a[A_FACIALHAIR]));
}

// hs_ and hp_ are the human sets `characters` may hold until the row is next
// written: hs_ what the last save wrote, hp_ what the save calling this
// writes. No flag, so nothing depends on when the save lands: the row is queued
// before the save's own transaction, and with CharacterDatabase.WorkerThreads
// = 1 (the default) the queue runs in order, so a crash can't leave a human set
// in `characters` that the row doesn't list.
static void SaveForms(Player* player, uint8 const* saving = nullptr)
{
    WorgenFormData* d = Forms(player);
    CharacterDatabase.Execute(
        "REPLACE INTO worgen_form_appearance "
        "(guid,w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair,"
        "hs_skin,hs_face,hs_hair,hs_haircolor,hs_facialhair,"
        "hp_skin,hp_face,hp_hair,hp_haircolor,hp_facialhair) "
        "VALUES ({},{},{},{},{})",
        player->GetGUID().GetCounter(), SqlSet(d->w), SqlSet(d->h),
        SqlSet(d->savedHuman ? d->saved : nullptr), SqlSet(saving));
}

static void ReadSet(Field* f, int first, uint8* out)
{
    for (uint8 i = 0; i < A_COUNT; ++i)
        out[i] = f[first + i].Get<uint8>();
}

static void LoadForms(Player* player)
{
    WorgenFormData* d = Forms(player);

    // `characters` holds the wolf set - including barbershop and
    // character-screen changes - unless it holds a human set the row lists.
    uint8 cur[A_COUNT];
    ReadAppearance(player, cur);
    std::copy(cur, cur + A_COUNT, d->w);
    std::copy(cur, cur + A_COUNT, d->h);
    d->human = false;
    d->savedHuman = false;

    QueryResult res = CharacterDatabase.Query(
        "SELECT w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair,"
        "hs_skin,hs_face,hs_hair,hs_haircolor,hs_facialhair,"
        "hp_skin,hp_face,hp_hair,hp_haircolor,hp_facialhair "
        "FROM worgen_form_appearance WHERE guid = {}",
        player->GetGUID().GetCounter());

    if (res)
    {
        Field* f = res->Fetch();
        ReadSet(f, A_COUNT, d->h);

        for (int first : { 2 * A_COUNT, 3 * A_COUNT })
        {
            if (f[first].IsNull())
                continue;

            uint8 listed[A_COUNT];
            ReadSet(f, first, listed);
            if (!SameSet(listed, cur))
                continue;

            // The server went down after a save in human form.
            std::copy(cur, cur + A_COUNT, d->saved);
            d->savedHuman = true;
            ReadSet(f, 0, d->w);
            WriteAppearance(player, d->w);
            break;
        }
    }

    // A gender change, or wolf choices with no human equivalent.
    ClampHuman(player, d->h);

    d->loaded = true;
    SaveForms(player);
}

// Runs fn(player) after the update fields changed now have been sent, so the
// client sees both values. Map::Update() sends the fields only in an update
// with a time step (MapUpdateInterval), and players are also updated in the
// steps between, so the player's own events can run twice before anything is
// sent. The map's events advance only in the updates that send, before they
// do: the first event can still run before this change goes out, the second
// (added while events run, so not run in the same pass) one sending update
// later. A player who has left the map by then is skipped.
template<typename Fn>
static void AfterFieldsSent(Player* player, Fn fn)
{
    Map* map = player->GetMap();
    ObjectGuid const guid = player->GetGUID();
    map->Events.AddEventAtOffset([map, guid, fn]()
    {
        map->Events.AddEventAtOffset([map, guid, fn]()
        {
            if (Player* p = ObjectAccessor::GetPlayer(map, guid))
                fn(p);
        }, 1ms);
    }, 1ms);
}

// Puts the human display back after Redraw(), if nothing has replaced the
// redraw display since.
static void EndRedraw(Player* player)
{
    WorgenFormData* d = Forms(player);
    if (!d->redrawing)
        return;

    d->redrawing = false;
    if (player->GetDisplayId() == RedrawDisplay(player))
        player->SetUInt32Value(UNIT_FIELD_DISPLAYID, HumanDisplay(player));
}

// The barbershop has no script hook: WorldSession::HandleAlterAppearance ->
// Player::ChangeBarberShopStyle calls no scripts. So a haircut is detected by
// comparing. In wolf form the fields are the wolf set. In human form the
// barber chair is refused (worgoblin_barber_chair), but a haircut sent anyway
// goes through the core, which takes worgen styles (it checks getRace()); they
// are moved to valid human values here. Returns true if the human set changed.
static bool ReadBackFields(Player* player)
{
    WorgenFormData* d = Forms(player);

    if (!d->human)
    {
        ReadAppearance(player, d->w);
        return false;
    }

    uint8 cur[A_COUNT];
    ReadAppearance(player, cur);
    if (SameSet(cur, d->h))
        return false;

    std::copy(cur, cur + A_COUNT, d->h);
    if (ClampHuman(player, d->h))
        WriteAppearance(player, d->h);
    return true;
}

// Only through UpdateForm(), which guards against re-entry. `asked`: the core
// handed the display to Two Forms (RestoreDisplayId(), or the resend after
// login or a teleport); CanChangeForm() below rules out other owners.
// `visual`: the caller is a real switch (see UpdateForm()).
static void SetWorgenForm(Player* player, bool human, bool asked, bool visual)
{
    WorgenFormData* d = Forms(player);

    // Last chance to catch a haircut before the fields are overwritten.
    if (ReadBackFields(player))
        SaveForms(player);

    bool const switched = d->human != human;
    d->human = human;
    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, human ? uint8(RACE_HUMAN) : uint8(RACE_WORGEN_ID));
    WriteAppearance(player, human ? d->h : d->w);

    // Also while another aura owns the display: Unit::RestoreDisplayId()
    // falls back to the native display when that aura ends.
    uint32 const display = human ? HumanDisplay(player) : WorgenDisplay(player);
    player->SetNativeDisplayId(display);

    if (!CanChangeForm(player))
        return;

    // In wolf form only replace our own human display, or one handed to Two
    // Forms. Any other display (a GM's .morph) was set on purpose.
    uint32 const current = player->GetDisplayId();
    if (current != display && (human || IsHumanDisplay(current) || asked))
    {
        // SetDisplayId() resets the scale; keep scale auras (Giant Growth).
        player->SetDisplayId(display);
        player->RecalculateObjectScale();
    }

    // Not on death: dying in combat ends combat (human form back) and then
    // removes Two Forms (wolf form) in the same tick.
    if (visual && switched && player->IsAlive())
        player->SendPlaySpellVisual(human ? VISUAL_KIT_TO_HUMAN : VISUAL_KIT_TO_WORGEN);
}

// Human while Two Forms is on, unless another aura owns the display, combat
// brings the wolf out, or the player is logging out: leaving a battleground
// on logout ends combat after OnPlayerBeforeLogout and before the save, and
// the logout save must see the wolf.
static bool WantsHuman(Player* player)
{
    return player->HasAura(SPELL_TWO_FORMS) && CanChangeForm(player) &&
           !(sCombatShift && player->IsInCombat()) && !player->GetSession()->PlayerLogout();
}

static bool IsPlayerModel(Player* player, uint32 display)
{
    return display == WorgenDisplay(player) || IsHumanDisplay(display);
}

// The one place the form is decided, called from every hook that can change
// WantsHuman() and from every display change. `asked`: the core handed the
// display to Two Forms (see SetWorgenForm()). `visual`: the caller is a real
// switch - Two Forms cast or cancelled (Darkflight cancels it), combat start
// or end - so a form change plays the transformation. Not at login, logout,
// the resend after a teleport, or when another aura ends.
static void UpdateForm(Player* player, bool asked = false, bool visual = false)
{
    WorgenFormData* d = Forms(player);

    // SetDisplayId() and HandleEffect() below call OnDisplayIdChange.
    if (d->updating)
        return;
    d->updating = true;

    if (d->loaded)
        SetWorgenForm(player, WantsHuman(player), asked, visual);

    // Another transform owns the display, but RestoreDisplayId() only asks the
    // newest transform, and under a warrior stance it sets the native display
    // without asking any. Hand the display back to that transform.
    if (!HasModelForm(player))
        if (AuraEffect* other = OtherTransform(player))
            if (asked || IsPlayerModel(player, player->GetDisplayId()))
                other->HandleEffect(player, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, true);

    d->updating = false;
}

// --- valid human appearances -----------------------------------------------
//
// A skin is a CharSections row with BaseSection SKIN, variation 0 and the skin
// colour as ColorIndex. A face is BaseSection FACE with the face as
// VariationIndex AND the skin colour as ColorIndex - so a face only exists for
// certain skin colours, and a skin change must validate the face again.
//
// The core has no lookup per race and type, so sCharSectionsStore is walked.
// Skin and face only from rows with SECTION_FLAG_PLAYER - what character
// creation offers; the rest are NPC skins. Rows with SECTION_FLAG_DEATH_KNIGHT
// only for death knights. Hair style and facial hair are what
// the barbershop accepts (BarberShopStyle.dbc), hair colour any CharSections
// row for that style: some barbershop styles only have non-player rows.
//
// The wolf choices are not always valid human ones: female worgen have facial
// hair 0-11 and skin 10-11, human females facial hair 0-6 and no skin 10-11.

static bool HumanSectionExists(Player* player, CharSectionType section, uint8 type, uint8 color,
                               bool playerOnly = true)
{
    bool const dk = player->getClass() == CLASS_DEATH_KNIGHT;
    for (CharSectionsEntry const* e : sCharSectionsStore)
        if (e->RaceID == RACE_HUMAN && e->SexID == NativeGender(player) &&
            e->BaseSection == uint32(section) && e->VariationIndex == type && e->ColorIndex == color &&
            (!playerOnly || ((e->Flags & SECTION_FLAG_PLAYER) && (dk || !(e->Flags & SECTION_FLAG_DEATH_KNIGHT)))))
            return true;
    return false;
}

static bool HumanSkinExists(Player* player, uint8 skin)
{
    return HumanSectionExists(player, SECTION_TYPE_SKIN, 0, skin);
}

static bool HumanFaceExists(Player* player, uint8 face, uint8 skin)
{
    return HumanSectionExists(player, SECTION_TYPE_FACE, face, skin);
}

// BarberShopStyle type: 0 hair style, 2 facial hair.
static bool HumanBarberStyleExists(Player* player, uint32 type, uint8 id)
{
    for (BarberShopStyleEntry const* e : sBarberShopStyleStore)
        if (e->type == type && e->race == RACE_HUMAN && e->gender == NativeGender(player) && e->hair_id == id)
            return true;
    return false;
}

// Steps forward (dir=1) or backward (dir=-1) to the next valid value. The loop
// is bounded by the 256 possible indices; if there is no other valid value,
// it stays put.
template<typename Valid>
static uint8 NextValid(uint8 cur, int dir, Valid valid)
{
    for (int step = 1; step < 256; ++step)
    {
        uint8 cand = uint8(((int(cur) + dir * step) % 256 + 256) % 256);
        if (valid(cand))
            return cand;
    }
    return cur;
}

static uint8 CycleSkin(Player* player, uint8 cur, int dir)
{
    return NextValid(cur, dir, [player](uint8 v) { return HumanSkinExists(player, v); });
}

static uint8 CycleFace(Player* player, uint8 cur, uint8 skin, int dir)
{
    return NextValid(cur, dir, [player, skin](uint8 v) { return HumanFaceExists(player, v, skin); });
}

static uint8 CycleHair(Player* player, uint8 cur, int dir)
{
    return NextValid(cur, dir, [player](uint8 v) { return HumanBarberStyleExists(player, 0, v); });
}

static uint8 CycleHairColor(Player* player, uint8 cur, uint8 hair, int dir)
{
    return NextValid(cur, dir, [player, hair](uint8 v)
    {
        return HumanSectionExists(player, SECTION_TYPE_HAIR, hair, v, false);
    });
}

static uint8 CycleFacialHair(Player* player, uint8 cur, int dir)
{
    return NextValid(cur, dir, [player](uint8 v) { return HumanBarberStyleExists(player, 2, v); });
}

// Moves every invalid value to the next valid one. Returns true if anything
// changed.
static bool ClampHuman(Player* player, uint8* a)
{
    uint8 const old[A_COUNT] = { a[0], a[1], a[2], a[3], a[4] };

    if (!HumanSkinExists(player, a[A_SKIN]))
        a[A_SKIN] = CycleSkin(player, a[A_SKIN], 1);
    if (!HumanFaceExists(player, a[A_FACE], a[A_SKIN]))
        a[A_FACE] = CycleFace(player, a[A_FACE], a[A_SKIN], 1);
    if (!HumanBarberStyleExists(player, 0, a[A_HAIR]))
        a[A_HAIR] = CycleHair(player, a[A_HAIR], 1);
    if (!HumanSectionExists(player, SECTION_TYPE_HAIR, a[A_HAIR], a[A_HAIRCOLOR], false))
        a[A_HAIRCOLOR] = CycleHairColor(player, a[A_HAIRCOLOR], a[A_HAIR], 1);
    if (!HumanBarberStyleExists(player, 2, a[A_FACIALHAIR]))
        a[A_FACIALHAIR] = CycleFacialHair(player, a[A_FACIALHAIR], 1);

    return !std::equal(a, a + A_COUNT, old);
}

class worgoblin : public PlayerScript
{
public:
    worgoblin() : PlayerScript("worgoblin") { }

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("Announce.enable", true))
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the Worgoblin module.");

        if (!IsWorgenPlayer(player))
            return;

        LoadForms(player);
        Forms(player)->loginTime = player->GetInGameTime();

        // The aura was re-applied during _LoadAuras, BEFORE this hook, when
        // the appearance was not loaded yet. The create packet has gone out
        // with the worgen race (worgoblin_barber_chair).
        UpdateForm(player);
    }

    // Called from WorldSession::LogoutPlayer before SaveToDB(), with
    // PlayerLogout() already true: the wolf set goes back into the fields, so
    // `characters` - and the character list - keeps the canonical form.
    void OnPlayerBeforeLogout(Player* player) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player);
    }

    // After the logout save. A row written now no longer lists the human set
    // an earlier save wrote, so a later character-screen change can't match it.
    void OnPlayerLogout(Player* player) override
    {
        if (IsWorgenPlayer(player) && Forms(player)->loaded)
            SaveForms(player);
    }

    void OnPlayerSave(Player* player) override
    {
        if (!IsWorgenPlayer(player))
            return;

        WorgenFormData* d = Forms(player);
        if (!d->loaded)
            return;

        ReadBackFields(player);

        // Exactly what _SaveCharacter() writes right after this hook.
        uint8 cur[A_COUNT];
        ReadAppearance(player, cur);
        SaveForms(player, d->human ? cur : nullptr);

        std::copy(cur, cur + A_COUNT, d->saved);
        d->savedHuman = d->human;
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        trans->Append("DELETE FROM worgen_form_appearance WHERE guid = {}", guid);
    }

    void OnPlayerGetReputationPriceDiscount(Player const* player, FactionTemplateEntry const* factionTemplate, float& discount) override
    {
        if (!factionTemplate || !factionTemplate->faction)
            return;

        if (player->HasSpell(BEST_DEALS_ANYWHERE))
            discount *= 0.8;
    }

    // Redraw() restores the display from the map it was started on; a player
    // who changed map before that never gets the event.
    void OnPlayerMapChanged(Player* player) override
    {
        if (IsWorgenPlayer(player) && Forms(player)->loaded)
            EndRedraw(player);
    }

    void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player, false, true);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player, false, true);
    }
};

class spell_rocket_barrage : public SpellScript
{
    PrepareSpellScript(spell_rocket_barrage);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        int32 basePoints = 0 + caster->GetLevel() * 2;
        basePoints += caster->SpellBaseDamageBonusDone(GetSpellInfo()->GetSchoolMask()) * 0.429; //BM=0.429 here, don't ask me how.
        basePoints += caster->GetTotalAttackPowerValue(caster->getClass() != CLASS_HUNTER ? BASE_ATTACK : RANGED_ATTACK) * 0.25; // 0.25=BonusCoefficient, hardcoding it here
        SetEffectValue(basePoints);
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_rocket_barrage::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_worgen_two_forms : public SpellScript
{
    PrepareSpellScript(spell_worgen_two_forms);

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!IsWorgenPlayer(caster))
            return SPELL_FAILED_DONT_REPORT;

        // The spell already has SPELL_ATTR0_CANT_USED_IN_COMBAT; this is belt
        // and braces, so a GM-granted cast cannot get around it.
        if (caster->IsInCombat())
            return SPELL_FAILED_AFFECTING_COMBAT;

        // Toggle. Without it a second cast would just refresh the aura, and
        // the player could never get back to wolf form on their own.
        if (caster->HasAura(SPELL_TWO_FORMS))
        {
            caster->RemoveAurasDueToSpell(SPELL_TWO_FORMS);
            return SPELL_FAILED_DONT_REPORT;
        }

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_worgen_two_forms::CheckCast);
    }
};

class spell_worgen_two_forms_aura : public AuraScript
{
    PrepareAuraScript(spell_worgen_two_forms_aura);

    // Two things here are not decoration.
    //
    // 1. PreventDefaultAction() in BOTH directions. Otherwise
    //    SPELL_AURA_TRANSFORM sets the display to the illusion creature 20708
    //    (Human Female Illusion) - female for everyone, and a creature display
    //    hides the player's gear.
    // 2. Registration on AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK and not only
    //    REAL. Unit::RestoreDisplayId() calls HandleEffect() again with
    //    AURA_EFFECT_HANDLE_SEND_FOR_CLIENT every time another transform or
    //    shapeshift aura falls off. Without the mask the core would take the
    //    display back behind our back and apply the illusion anyway.
    //
    // The aura survives logout (Aura::CanBeSaved() says yes: not passive, not
    // channeled, self-cast, infinite duration), so the form is remembered.
    //
    // REAL is the cast and the cancel; the aura loaded at login is REAL too,
    // but UpdateForm() does nothing before OnPlayerLogin.
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (IsWorgenPlayer(target))
            UpdateForm(target, mode == AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, (mode & AURA_EFFECT_HANDLE_REAL) != 0);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (IsWorgenPlayer(target))
            UpdateForm(target, false, (mode & AURA_EFFECT_HANDLE_REAL) != 0);
    }

    void Register() override
    {
        OnEffectApply  += AuraEffectApplyFn(spell_worgen_two_forms_aura::HandleApply,
                                            EFFECT_0, SPELL_AURA_TRANSFORM,
                                            AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
        OnEffectRemove += AuraEffectRemoveFn(spell_worgen_two_forms_aura::HandleRemove,
                                             EFFECT_0, SPELL_AURA_TRANSFORM,
                                             AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
    }
};

// Darkflight describes itself as "Activates your true form", so it ends the
// human form. The same behaviour as in Cataclysm.
class spell_worgen_darkflight : public SpellScript
{
    PrepareSpellScript(spell_worgen_darkflight);

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (IsWorgenPlayer(caster))
            caster->RemoveAurasDueToSpell(SPELL_TWO_FORMS);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_worgen_darkflight::HandleAfterCast);
    }
};

// --- Gilnean Barber -------------------------------------------------------
//
// Styles the human form: skin and face, which no barbershop changes in WotLK,
// and hair, hair colour and facial hair, which the barber chair cannot do in
// human form (worgoblin_barber_chair). Everything is applied live, so the menu
// itself is the preview - which is why you have to be in human form to use it.

// The client reads the appearance fields when it builds the model, which it
// does when the display changes. A change to the fields alone recolours the
// model it has, with the old face. So the display shows an identical human
// model for one map update and then the human display again: both changes build
// the model with the new fields.
static void Redraw(Player* player)
{
    WorgenFormData* d = Forms(player);

    uint32 const current = player->GetDisplayId();
    if (current == RedrawDisplay(player))
    {
        EndRedraw(player);   // switching back builds the model too
        return;
    }

    // A costume or a druid form owns the display; the model is built again
    // when it ends.
    if (current != HumanDisplay(player))
        return;

    // The field only: SetDisplayId() calls OnDisplayIdChange, and UpdateForm()
    // would put the human display back before anything is sent.
    player->SetUInt32Value(UNIT_FIELD_DISPLAYID, RedrawDisplay(player));
    d->redrawing = true;
    uint32 const id = ++d->redrawId;
    AfterFieldsSent(player, [id](Player* p)
    {
        if (Forms(p)->redrawId == id)
            EndRedraw(p);
    });
}

// Next and previous of each value are consecutive.
enum GilneanBarberActions
{
    GB_SKIN_NEXT = GOSSIP_ACTION_INFO_DEF + 1,
    GB_SKIN_PREV,
    GB_FACE_NEXT,
    GB_FACE_PREV,
    GB_HAIR_NEXT,
    GB_HAIR_PREV,
    GB_HAIRCOLOR_NEXT,
    GB_HAIRCOLOR_PREV,
    GB_FACIALHAIR_NEXT,
    GB_FACIALHAIR_PREV,
    GB_RESET,
    GB_DONE,
};

class npc_gilnean_barber : public CreatureScript
{
public:
    npc_gilnean_barber() : CreatureScript("npc_gilnean_barber") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!IsWorgenPlayer(player))
        {
            creature->Whisper("I only cut Gilnean hair, I'm afraid.", LANG_UNIVERSAL, player);
            CloseGossipMenuFor(player);
            return true;
        }

        WorgenFormData* d = Forms(player);
        if (!d->loaded || !d->human)
        {
            creature->Whisper("Take your human form first, so I can see what I am doing. Use Two Forms.",
                              LANG_UNIVERSAL, player);
            CloseGossipMenuFor(player);
            return true;
        }

        BuildMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        WorgenFormData* d = Forms(player);
        if (!IsWorgenPlayer(player) || !d->loaded || !d->human)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        uint8 const old[A_COUNT] = { d->h[0], d->h[1], d->h[2], d->h[3], d->h[4] };
        int const dir = (action - GB_SKIN_NEXT) % 2 == 0 ? 1 : -1;

        switch (action)
        {
            case GB_SKIN_NEXT:
            case GB_SKIN_PREV:
                d->h[A_SKIN] = CycleSkin(player, d->h[A_SKIN], dir);
                break;
            case GB_FACE_NEXT:
            case GB_FACE_PREV:
                d->h[A_FACE] = CycleFace(player, d->h[A_FACE], d->h[A_SKIN], dir);
                break;
            case GB_HAIR_NEXT:
            case GB_HAIR_PREV:
                d->h[A_HAIR] = CycleHair(player, d->h[A_HAIR], dir);
                break;
            case GB_HAIRCOLOR_NEXT:
            case GB_HAIRCOLOR_PREV:
                d->h[A_HAIRCOLOR] = CycleHairColor(player, d->h[A_HAIRCOLOR], d->h[A_HAIR], dir);
                break;
            case GB_FACIALHAIR_NEXT:
            case GB_FACIALHAIR_PREV:
                d->h[A_FACIALHAIR] = CycleFacialHair(player, d->h[A_FACIALHAIR], dir);
                break;
            case GB_RESET:
                std::copy(d->w, d->w + A_COUNT, d->h);
                break;
            case GB_DONE:
            default:
                SaveForms(player);
                creature->Whisper("Dressed to walk among humans again.", LANG_UNIVERSAL, player);
                CloseGossipMenuFor(player);
                return true;
        }

        // Faces are tied to the skin colour and hair colours to the style, so
        // a new skin or style may need the nearest face or colour that exists.
        ClampHuman(player, d->h);

        if (!SameSet(old, d->h))
        {
            WriteAppearance(player, d->h);   // live preview
            Redraw(player);
            SaveForms(player);
        }
        BuildMenu(player, creature);
        return true;
    }

private:
    static void AddPair(Player* player, std::string const& what, uint8 now, uint32 next)
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next " + what + " (now: " + std::to_string(uint32(now)) + ")",
            GOSSIP_SENDER_MAIN, next);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous " + what, GOSSIP_SENDER_MAIN, next + 1);
    }

    static void BuildMenu(Player* player, Creature* creature)
    {
        WorgenFormData* d = Forms(player);

        ClearGossipMenuFor(player);
        AddPair(player, "skin tone", d->h[A_SKIN], GB_SKIN_NEXT);
        AddPair(player, "face", d->h[A_FACE], GB_FACE_NEXT);
        AddPair(player, "hair style", d->h[A_HAIR], GB_HAIR_NEXT);
        AddPair(player, "hair colour", d->h[A_HAIRCOLOR], GB_HAIRCOLOR_NEXT);
        AddPair(player, NativeGender(player) == GENDER_MALE ? "facial hair" : "piercings", d->h[A_FACIALHAIR],
                GB_FACIALHAIR_NEXT);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Reset to my wolf choices",
            GOSSIP_SENDER_MAIN, GB_RESET);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "That looks right",
            GOSSIP_SENDER_MAIN, GB_DONE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
};

// --- Barber chair ------------------------------------------------------------
//
// The client builds its barbershop style lists once per login, for the race
// byte in the create packet of its own player, and looks the current styles up
// in them for the race byte it has when the chair opens. When the two differ
// the lookup finds nothing, and GetBarberShopTotalCost crashes the client. A
// login sends the worgen race (OnPlayerLogin sets the human form after the
// create packet), so the chair is only for the wolf form. Logging in to a
// character still in the world sends the form it is in and calls no script;
// GetInGameTime() changes then, and the chair waits for a real login.
class worgoblin_barber_chair : public AllGameObjectScript
{
public:
    worgoblin_barber_chair() : AllGameObjectScript("worgoblin_barber_chair") { }

    bool CanGameObjectGossipHello(Player* player, GameObject* go) override
    {
        if (go->GetGoType() != GAMEOBJECT_TYPE_BARBER_CHAIR || !IsWorgenPlayer(player))
            return false;

        WorgenFormData* d = Forms(player);
        if (d->human)
            ChatHandler(player->GetSession()).SendNotification(
                "The barber chair is for your worgen form. The Gilnean Barber styles your human form.");
        else if (!d->loaded || player->GetInGameTime() != d->loginTime)
            ChatHandler(player->GetSession()).SendNotification("Log out and back in to use the barber chair.");
        else
            return false;

        return true;
    }
};

class worgoblin_config : public WorldScript
{
public:
    worgoblin_config() : WorldScript("worgoblin_config", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sCombatShift = sConfigMgr->GetOption<bool>("Worgoblin.TwoForms.CombatShift", true);
    }
};

// Any display change can leave the fields and the display out of step with
// WantsHuman(): a costume or a polymorph ending under a warrior stance (then
// Unit::RestoreDisplayId() sets the native display), or ending in combat, or a
// druid form or costume starting in human form.
class worgoblin_display : public UnitScript
{
public:
    worgoblin_display() : UnitScript("worgoblin_display", true, { UNITHOOK_ON_DISPLAYID_CHANGE }) { }

    void OnDisplayIdChange(Unit* unit, uint32 displayId) override
    {
        if (!IsWorgenPlayer(unit))
            return;

        Player* player = unit->ToPlayer();
        UpdateForm(player);

        // Player::InitDisplayIds() (.modify gender) sets the native display to
        // the worgen model right after this display. In human form, set the
        // human one again once it has.
        if (displayId == WorgenDisplay(player) && Forms(player)->human)
            player->m_Events.AddEventAtOffset([player]() { UpdateForm(player); }, 1ms);
    }
};

void Add_Worgoblin()
{
    new worgoblin();
    new worgoblin_config();
    new worgoblin_display();
    new worgoblin_barber_chair();
    new npc_gilnean_barber();
    RegisterSpellScript(spell_rocket_barrage);
    RegisterSpellAndAuraScriptPair(spell_worgen_two_forms, spell_worgen_two_forms_aura);
    RegisterSpellScript(spell_worgen_darkflight);
}
