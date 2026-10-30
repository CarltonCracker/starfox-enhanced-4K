# Star Fox Enhanced 0.0.8.8.2

This update includes the changes since 0.0.8.8.1. Supply your own supported
retail ROM; no ROM or private game data is included. Nintendo 3DS work remains
paused and no 3DS package is included.

## Windows packaging and security

- Removed bundled DisplayXR system installers from the game download. The
  previously bundled runtime installer accounts for the major antivirus
  detections reported against the Windows ZIP. Optional upstream download
  instructions remain available; direct Leia **SR PLATFORM** support and the
  embedded DLSS runtimes are unchanged.
- Kept the standalone asset builder in its own Windows download instead of
  duplicating it inside the game ZIP. The game still imports retail ROMs itself.
- Removed unused enhanced-background artwork from the asset builder, reducing
  its executable size substantially without changing the generated asset data.
- Added Defender checks of the Windows x64 executables and final archives
  before release uploads. This does not guarantee every antivirus vendor's
  verdict or SmartScreen reputation. Do not disable antivirus or add exclusions.

## Graphics

- Fixed the Meteor floor disappearing or wrapping back to stars near the
  lower corners of the screen.
- Corrected finite orbital backgrounds extending beyond their original image
  area during banking and in widescreen views, including planet-edge repeats
  and black padding. Corrected Original Venom-space enhanced-sky placement.
- Kept enhanced scenery active during stage-clear flyaways and results.
- Fixed Original Venom's blue/purple dry ground being mistaken for water,
  allowing enhanced ground to work without enabling ray tracing.
- Corrected results and ending drawing order so later objects do not
  incorrectly paint over percentage lettering.

## Controls and updates

- Added **CONTROLLER REMAP → KEYBOARD → KEYBOARD PLAYER**. Select **PLAYER 2**
  for controller-P1/keyboard-P2 co-op; the assignment is saved. Default
  keyboard/first-controller sharing remains unchanged, as do EX's native
  player-count and multitap options.
- Preserved full five-part version handling for the in-game updater, including
  detecting 0.0.8.8.2 from 0.0.8.8.1. Portable Windows x64/x86 and Linux x64
  packages retain confirmed download, verification and restart-to-install.
  Other platforms retain their existing manual update methods.
- Restored the direct optional MSU-1 music-pack download link in the README.
- Added a desktop build option to load enhanced artwork from companion files
  instead of embedding it. Normal releases continue to embed the artwork.

## Compatibility notes

Settings, saves and existing compatible `Starfox-Assets.BIN` data remain usable.
Android and Quest retain their existing release-signing key; no signing-key
change is introduced. Desktop source/build directories do not support in-place
auto-update; use an extracted release installation.

Older installations may still contain optional DisplayXR installer files from
earlier packages. They are not needed for the game or direct SR PLATFORM mode;
do not run or whitelist a file blocked by security software.

Device-specific AMD/Android GPU hangs, remaining Retroid audio reports and
hardware-specific Vita/PS5/macOS compatibility reports are not all confirmed
resolved. Build success alone is not hardware acceptance.
