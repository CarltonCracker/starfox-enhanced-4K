# Star Fox Enhanced 0.0.8.8.1

This update includes the changes since 0.0.8.8. Supply your own supported retail
ROM; no ROM or private game data is included. Nintendo 3DS development remains
paused, and no 3DS package is included.

## In-game updates

- Added Options → Check for updates, showing both the installed and latest
  available version. Checks and downloads run without blocking the menu.
- Added confirmed download, verification and restart-to-install for portable
  Windows x64/x86 and Linux x64 desktop installations, including extracted
  Steam Deck desktop packages. Settings, saves and user-provided assets are
  preserved; failed replacements roll back changed application files.
- macOS supports version checks and opening the release page. Mobile, console
  and VR packages still use their normal manual installation methods. This
  first release containing the updater must itself be installed normally.
- Added numeric hotfix-version support so 0.0.8.8.1 is displayed and compared
  correctly. See [update support and limitations](https://github.com/kandowontu2/starfox-enhanced/blob/main/docs/UPDATES.md).

## Graphics and performance

- Corrected corridor backdrop extensions and closed-tunnel ceilings, including
  the unwanted top bar and moving-atlas edge seams. Authored background margins
  are used before extending an image beyond its original coverage.
- Restored the default removal behavior of allocated map objects, preventing
  earlier space-scene objects from persisting into enclosed areas.
- Reduced software-renderer CPU work by sharing persistent presentation workers
  across background rendering and upscaled brightness/contrast processing.
  Native-resolution output and serial rendering retain their existing behavior.
- Rendered independent music and sound-effect stems concurrently, joining them
  within the same simulation tick to preserve game/audio timing.
- Hardened worker error handling so all outstanding work is joined before an
  error is returned to the calling thread.

## Audio and mobile

- Restored the correct instrument-bank setup when launching Original stages
  through the pre-game LEVEL SELECT menu, fixing incorrect weapon sounds.
- Increased Android startup audio headroom and let the audio device select its
  callback size to target crackling reported on handheld devices. Source audio
  cadence is unchanged; affected-device confirmation is still needed.
- Added missing iPhone/iPad launcher artwork and strengthened IPA icon/package
  validation. The iOS download remains an unsigned IPA for sideloading.
- Updated package identifiers and build numbers for this hotfix. Android and
  Quest retain the existing release-signing key.

## Known limits

This release does not claim to resolve every open report. AMD Vulkan
device-loss/GPU hangs, Android audio latency and remaining device-specific
crackling, intermittent EX graphics/space-scene brightening, and some background
variants remain under investigation. Vita, PS5 and macOS fixes still need the
relevant hardware confirmation; successful builds alone do not establish full
hardware compatibility.

Antivirus detections against earlier Windows downloads remain under review.
Do not disable antivirus protection to install the game.
