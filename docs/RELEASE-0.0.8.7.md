# Star Fox Enhanced 0.0.8.7

This update includes the changes since 0.0.8.6. Supply your own supported retail
ROM; no ROM or private game data is included. Nintendo 3DS development remains
paused, and no 3DS package is included.

## New renderer and display options

- Added CarltonCracker's **MODERN — Experimental** renderer on desktop Windows
  and Linux. It draws the scene through the shared VR rendering path, reducing
  dependence on the per-pixel Super FX rendering layers. It remains optional;
  Software, GPU and GPU FAST are retained.
- MODERN includes world-anchored ground, smoother background movement, native
  space horizons, 3D corridors for compatible artwork, and widescreen HUD and
  communications. Optional replacement model textures, portraits, HUD sprites
  and landscape textures can be loaded from a local `textures` directory.
- Added a texture-dump tool for preparing your own replacement artwork.
- Hardened MODERN renderer switching, resource lifetime, stereo compatibility
  and complete-frame fallbacks. Its shared PCVR frame-planning path remains
  opt-in and experimental.
- Added a lightweight display-area guide to the pre-game menu and remembered
  desktop window dimensions.
- Opening HUD/touch layout customization from F1 options now edits the paused
  live game without restarting the stage. Apply and Cancel return to the same
  options session without carrying button presses into it.

## Gameplay and audio fixes

- Fixed sound-bank initialization when entering stages through Level Select.
- Restored stage-results counting, completion and bonus-credit sound feedback,
  with consistent timing across presentation frame rates.
- Fixed encounter music continuing through player death and preserved the
  replacement Player Down cue, including cockpit death paths.
- Native boss stop/fade commands now also stop or fade replacement MSU music.
  The fade follows source timing and is shared with VR rather than being a
  desktop-only effect.
- Fixed returning to the title after the ending, including restarting after
  the credits music has finished and preventing stale ending scroll state
  from leaking into subsequent screens and stages.
- Reduced Android playback buffering without changing simulation or audio
  cadence.

## Rendering fixes

- Improved alignment of banked native backgrounds and enhanced horizons across
  atlas wraps.
- Fixed enhanced-sky sampling that could select unrelated atlas sections,
  repeat panoramas within a wide view, or stretch the sky's upper edge.
- Improved distant planet continuation when looking downward, avoiding
  stretched photographic columns.
- Expanded death/revival red and white screen fills to cover widescreen and
  supersampled corners.
- Preserved ray-traced model shadows when enhanced ground replaces scenery,
  and corrected emissive-surface shadow handling.
- Restored runtime-selected sprite materials, including enemy fire trails.
- Ground-material overrides, including manually selected lava, no longer
  replace classified distant planets and starfields. Enhanced skies remain
  independently selectable.
- Stopped native star/planet colours from being extruded downward as solid
  floor blocks after an atlas wrap.
- Extended symmetric scanline-tunnel ceiling/floor diagonals into widescreen
  margins instead of stretching edge pixels into horizontal bars. The native
  256-column view is preserved; incompatible decorated tunnels keep their
  existing rendering path.
- Improved CPU layer-compositing performance while preserving material and
  coverage information.
- Kept Metal ray-pipeline preparation off the macOS UI thread, with responsive
  loading feedback and cached preparation results.

## Platform and packaging updates

- Fixed PS5 display uncovering after preview-off and loading screens.
- Added complete Vita launcher/LiveArea artwork and improved VPK metadata and
  package validation.
- Retained proper unsigned iOS IPA packaging, existing Android/Quest release
  signing, and PS5 and Steam Frame release packages.
- Updated application/package versions to 0.0.8.7, including increasing Android
  and Quest version codes and the Apple and Xbox package build versions.

## Known limits

MODERN is experimental and is not available on every platform. Some scenes,
effects and stereo combinations still use the existing renderer or differ in
appearance. PCVR, Quest, PS5 and Steam Frame still require device-specific
testing; a successful build does not guarantee compatibility or performance.

Reports of intermittent white fades, some EX ground bars, Andross entrance
stretching, native planet glitches at certain camera angles, and an EX ending
animation issue remain under investigation. The tunnel-margin fix does not
resolve every boss-room or corridor variant. Windows Defender reports against
the previous x64 download are not considered resolved by this version bump;
do not disable antivirus protection to install the game.
