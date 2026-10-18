# VRM Qavatars
- Custom Avatars For Quest!
- This is a fork of [Misticenea/VRM-Qavatars-1.40.8](https://github.com/Misticenea/VRM-Qavatars-1.40.8), fixed with the help of Claude.
- Version 1.0.0, which only contains the fixes needed to make it build, can be downloaded [here](https://drive.google.com/file/d/1ZyMCo5bAPN2OxCa1rEIRV7U-OD49a1mi/view?usp=sharing). (I forgot to commit it, so there is no tag for that version.)

## Features
- Full VRM 0.0 format support
- Replay Support (No third person support for now)
- Large list of customizable options and features ( All features documented at **https://bsq-vrm.github.io/documentation/**)
- Full Body tracking/Virtual Motion Capture support
- A feature rich mirror to show off your avatar in recordings
- Expression Triggers
- Fake leg motion without foot trackers: beat synced steps, leg actions on controller inputs and feet lifting when you jump (VRIK > Leg Motion / Leg Buttons)
- Playable Performance
- And a ton more, with even more in the future

## Building
Requirements: [qpm](https://github.com/QuestPackageManager/QPM.CLI), CMake, Ninja and PowerShell 7 (`pwsh`)

```
qpm ndk resolve -d   # install the NDK and write ndkpath.txt
qpm restore          # fetch dependencies
```

| Command | Description |
|---|---|
| `qpm s build` | Build the mod into `build/` |
| `qpm s qmod` | Build and package a `.qmod` |
| `qpm s copy` | Build and copy the mod to the Quest over adb |
| `qpm s build-debug` / `qpm s qmod-debug` | Same as above with in-game posing diagnostics logging (`VRMQ_DIAG`) |
| `qpm s logcat` | Capture the mod's log from the Quest |

The mod version is taken from `info.version` in `qpm.json`.

## Credits

* [zoller27osu](https://github.com/zoller27osu), [Sc2ad](https://github.com/Sc2ad) and [jakibaki](https://github.com/jakibaki) - [beatsaber-hook](https://github.com/sc2ad/beatsaber-hook)
* [FrozenAlex](https://github.com/FrozenAlex/) for making me do this
* [HardCPP](https://github.com/hardcpp) for [ChatPlex-SDK](https://github.com/hardcpp/QuestChatPlexSDK-BS)
* [RedBrumbler](https://github.com/RedBrumbler/) for helping me debug lots of issues early on
* [Lauriethefish](https://github.com/Lauriethefish), [danrouse](https://github.com/danrouse) and [Bobby Shmurner](https://github.com/BobbyShmurner) for [this template](https://github.com/Lauriethefish/quest-mod-template)
