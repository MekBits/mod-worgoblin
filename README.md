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
between their worgen and human appearance, as in Cataclysm. No client change is
needed beyond the module's own patch.

- **Combat brings the wolf out.** The human form is a wish: entering combat
  switches to worgen form, and the human form returns when combat ends. Set
  `Worgoblin.TwoForms.CombatShift = 0` to stay human in combat.
- **Darkflight** ("Activates your true form") ends the human form.
- **Druid forms** are left alone; the form changes only outside shapeshift.
- **The form survives logout.** The aura is saved like any other, and the
  character list always shows the worgen appearance.
- **The human form has its own look.** It starts as your worgen choices (always
  valid, since the worgen ranges are a subset of the human ones). The normal
  barbershop styles hair, hair colour and facial hair of the form you are in.
  Skin and face cannot be changed at a barbershop in WotLK, so the **Gilnean
  Barber** (Shadowglen and the Stormwind barbershop) does that: stand in human
  form and talk to him.

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
- The barbershop has no script hook, so a haircut in human form is detected by
  comparing the fields with the saved human set.

The SQL (`data/sql/db-world/worgoblin_two_forms.sql`,
`data/sql/db-characters/worgen_form_appearance.sql`) is applied by the database
updater. It adds the missing `SkillLineAbility` row for skill line 789
"Racial - Worgen", binds the spell scripts, and creates and spawns the Gilnean
Barber (creature 9000001). Two Forms works only on characters that learn it, which
happens on login for every worgen.

## Credits

* mthsena for creating a repository for the [original script](https://github.com/mthsena/trinitycore_scripts/tree/master/scripts/CustomRaces) for TrinityCore.
* [Helias](https://github.com/Helias) for mentioning the script and adapting the script to AzerothCore.
* [yuan2105](https://github.com/yuanf225) for racing me to get these working and helping me out on multiple occasions.
* [Tanados](https://github.com/helldragonpz) for adapting the HD patch to work with the module.
* Trimitor#3873 for creating the HD patch.
* Various users on various Discords for helping me out on a slew of issues with almost everything. (ragestriker#8037 and Mr.MA#0957 in particular)
