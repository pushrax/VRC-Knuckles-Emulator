# VRC-Knuckles-Emulator

Natural gestures with Knuckles (aka Index Controllers) in VRChat.

<img src="https://raw.githubusercontent.com/pushrax/VRC-Knuckles-Emulator/master/VRC-Knuckles-Emulator/icon.png" width="100px"> 

This works by intercepting calls VRChat makes to OpenVR to get controller data, remapping the inputs based on the gesture calculated in a separate app. No modification to the game itself was necessary.

```
+----------+      +--------+      +----------+
|          |      |        |      |          |
|  VRChat  +----->+  Hook  +----->+  OpenVR  |
|          |      |        |      |          |
+----------+      +--+-----+      +----+-----+
                     ^                 |
                     |                 |
                  +--+-----------+     |
                  |              |     |
                  |  Helper app  +<----+
                  |              |
                  +--------------+
```

This is completely unsupported, no warranty, etc. The tuning isn't perfect, but if you're a dev (and you probably are if you currently have these controllers), it should be easy to build yourself and adjust it. Look for the `InferGesture` function.

### Instructions

- If you've modified the SteamVR input bindings for VRChat, switch to the defaults, at least for the grip and trigger
- [Download](https://github.com/pushrax/VRC-Knuckles-Emulator/releases/latest/download/VRC-Knuckles-Emulator-1.zip) and unzip
- Run VRC-Knuckles-Emulator.exe and open game, in either order

The gestures should be pretty intuitive and require no explanation, however to avoid making menus difficult to use, the "fist" gesture is controlled entirely via the trigger, not via the grip.

### Building

There are no external dependencies. The project is set up on VS2015 (v140 toolchain). If you want to build on VS2017, just change `<PlatformToolset>v140</PlatformToolset>` to `v150` in the vcxproj files.
