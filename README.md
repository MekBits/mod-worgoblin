# Worgoblin Module

[![core-build](https://github.com/MekBits/mod-worgoblin/actions/workflows/core-build.yml/badge.svg)](https://github.com/MekBits/mod-worgoblin/actions/workflows/core-build.yml)

This is a module for [AzerothCore](http://www.azerothcore.org) that adds worgen, goblins, and numerous features related to their playability.

This is a fork of [heyitsbench/mod-worgoblin](https://github.com/heyitsbench/mod-worgoblin), which is no longer maintained. It adds [Two Forms](#two-forms) for worgen; everything else is the original module.

## Important Notes

This module makes *heavy* use of client patching and modification. Because of this, it is recommended to use a [clean, unmodified enUS WoW client](https://www.chromiecraft.com/downloads) as issues have been reported due to use of other clients, such as the one offered from Warmane. The HD patch included on the ChromieCraft website has been known to cause issues with this patch. It is for that reason that it's recommended for you to use the HD patch adapted to this module in particular, which can be found [here](https://github.com/benjymansy123/mod-worgoblin/releases/tag/hd-patch) with instructions on how to install it.

## How to Install

### 1) Add the `mod-worgoblin` folder to your AzerothCore source's modules directory.

This can be done by cloning the repository through git or by downloading the module as a ZIP. If you choose the latter, make sure that the folder name of the module is exactly `mod-worgoblin`.

### 2) Replace the DBC files in your AzerothCore Data directory with the ones provided in [DBFilesClient](https://github.com/heyitsbench/mod-worgoblin/tree/master/data/patch/DBFilesClient).

Upon downloading the repo, you'll have the patch-contents file available to you. Copy all the contents of the DBFilesClient folder to your AzerothCore Data directory. Feel free to make a backup of the DBCs you'll be replacing, as backups never hurt.

### 3) Compile and install AzerothCore.

### 4) Move the [patch](https://github.com/heyitsbench/mod-worgoblin/tree/master/data/patch) folder to your Data folder in your WoW client and rename the folder to `patch-A.MPQ`.

### 5) Remove signature checks from your WoW executable.
I recommend Windows users to use [this patcher](https://www.wowmodding.net/files/file/283-wow-335-patcher-custom-item-fix/) created by kebabstorm to patch your Wow.exe file. I recommend macOS users to download this [pre-patched .app file](https://github.com/benjymansy123/custom-race-ac-12_6_21/releases/download/sig-check/WoW.app.zip), since no easy patcher exists for macOS.

Because WoW uses signature checks so as to not allow Interface files to be modified, we need to remove those checks in order to be able to use custom races. This is accomplished using the patcher included in the repo. Move the patcher into your WoW client directory and run it. Again, feel free to make a backup of your original executable.

And with that, you are all done!

## Two Forms

Worgen learn **Two Forms** (spell 68996) with their other racials and can switch
between their worgen and human appearance, as in Cataclysm. The module's own
patch is all the client needs, except for the transformation effect and sound:
the server sends SpellVisualKit 16173 (to worgen) and 28505 (to human), which a
3.3.5 `SpellVisualKit.dbc` does not have. Without those rows the switch is
silent.

- **Combat brings the wolf out.** The human form is a wish: entering combat
  switches to worgen form, and the human form returns when combat ends. Set
  `Worgoblin.TwoForms.CombatShift = 0` to stay human in combat.
- **Darkflight** ("Activates your true form") ends the human form.
- **Druid forms** are left alone; the form changes only outside shapeshift.
- **The form survives logout.** The aura is saved like any other, and the
  character list always shows the worgen appearance.
- **The human form has its own look.** It starts as your worgen choices (always
  valid, since the worgen ranges are a subset of the human ones). The barber
  chair styles the worgen form only. The **Gilnean Barber** (Shadowglen and the
  Stormwind barbershop) styles the human form: skin, face, hair, hair colour
  and facial hair, free and shown as you choose. Stand in human form and talk
  to him.
- **A worgen druid cannot talk in human form.** The client checks languages
  against the race/class pairs in its `CharBaseInfo.dbc`, for the race it
  draws: human, and there is no human druid. It refuses every chat line with
  "You can't speak that language", GM commands too. Other worgen classes exist
  as humans and are not affected. The fix is client-side (see below).

How it works in 3.3.5, where the client has neither Cataclysm's aura type nor a
second appearance per character:

- The client takes the **model** from `UNIT_FIELD_DISPLAYID`, but composes skin,
  face and hair from the **race byte** in `UNIT_FIELD_BYTES_0` looked up in
  `CharSections.dbc`. Both are set: human display 49/50 (not the illusion
  creatures 20707/20708, whose display hides your gear) and race byte 1.
- The race byte is safe to change: `Unit::getRace()` returns `m_race`, and
  `_SaveCharacter()` saves the race from `getRace(true)` and the gender from
  `PLAYER_BYTES_3`.
- The appearance bytes in `PLAYER_BYTES`/`PLAYER_BYTES_2` **are** saved, so the
  table `worgen_form_appearance` (characters database) holds both sets. The
  worgen set is written back in `OnPlayerBeforeLogout`, before `SaveToDB()`.
  If the server dies while someone is in human form, the next login puts it
  right.
- In human form the **native display** (`UNIT_FIELD_NATIVEDISPLAYID`) is the
  human one too. The server's `Unit::IsInDisallowedMountForm()` refuses mounts
  and flight paths to a player whose display differs from the native one.
- The **barber chair cannot style the human form.** The client builds its
  barbershop style lists once per login, for the race in the first packet that
  creates your character, which is worgen: the human form is set after it. In
  the chair it looks your current style up for the race you have then. In
  human form that finds nothing and the client crashes (`ERROR #132` in
  `GetBarberShopTotalCost`). So the module refuses the chair in human form. A
  login to a character that is still in the world sends the form it is in, so
  after one the chair waits for a normal login.
- The client reads skin, face and hair when it builds the model, which happens
  when the display changes. A change to the appearance fields alone does not
  show the new face. So the Gilnean Barber sets display 907 (male) or 302
  (female), the same human models, for one server tick and then 49/50 again.
- Anything else that changes the fields in human form is detected by comparing
  them with the saved human set.
- A worgen druid in human form needs a `CharBaseInfo.dbc` row for race 1, class
  11 in the client. That row also offers Human Druid at character creation,
  which the server refuses unless `playercreateinfo` has it, so the character
  creation screen must hide it.

The SQL (`data/sql/db-world/worgoblin_two_forms.sql`,
`data/sql/db-characters/worgen_form_appearance.sql`) is applied by the database
updater. It adds the missing `SkillLineAbility` row for skill line 789
"Racial - Worgen", binds the spell scripts, and creates and spawns the Gilnean
Barber (creature 9000001). Two Forms works only on characters that learn it, which
happens on login for every worgen.

## Quests and items open to every race

`worgoblin.sql` adds worgen wherever humans may take a quest or use an item,
but skips masks it reads as "all races", among them 2047 (races 1-11, without
worgen). Its goblin update also turns the ten-race mask 1791 into 2047 first.
`data/sql/db-world/worgoblin_worgen_masks.sql` gives worgen every mask that
names all ten original races, which opens the Netherwing chain, To Skettis! and
a few other quests and items.

## Credits

* mthsena for creating a repository for the [original script](https://github.com/mthsena/trinitycore_scripts/tree/master/scripts/CustomRaces) for TrinityCore.
* [Helias](https://github.com/Helias) for mentioning the script and adapting the script to AzerothCore.
* [yuan2105](https://github.com/yuanf225) for racing me to get these working and helping me out on multiple occasions.
* [Tanados](https://github.com/helldragonpz) for adapting the HD patch to work with the module.
* Trimitor#3873 for creating the HD patch.
* [Maayv](https://github.com/Maayv/mod-worgoblin) for the worgen footprint timing and Cataclysm's goblin combat sounds.
* Various users on various Discords for helping me out on a slew of issues with almost everything. (ragestriker#8037 and Mr.MA#0957 in particular)
