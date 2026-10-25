# Star Fox Enhanced 0.0.8.8

This update includes the changes since 0.0.8.7. Supply your own supported retail
ROM; no ROM or private game data is included. Nintendo 3DS development remains
paused, and no 3DS package is included.

## Backgrounds and portraits

- Fixed thin vertical seams and stepped cloud tiles while steering in Mode 2
  stages. Background columns now follow each scanline's horizontal scroll in
  both the Software and GPU renderers, including the GPU stereo sky path.
  Thanks to maiktheclaw for contributing this fix.
- Removed the dialogue-only horizontal portrait stretch in widescreen. Original
  and EX dialogue portraits now use uniform source-pixel scaling, like the
  results-screen portraits, instead of duplicating uneven columns.

## Touch controls

- Enlarged the default on-screen controls relative to the display size for new
  and reset layouts. Existing saved layouts retain their previous base sizing.
- Removed fixed pinch-size and viewport-fit limits. Controls can be made larger
  or smaller, and oversized D-pad/button groups can still be dragged and saved.

## GPU and Vita improvements

- Reworked compositor margin histograms to use small shared workgroup buffers
  instead of large private arrays. This targets excessive shader-compilation
  work while retaining the same output.
- Substantially reduced Vita executable memory use by keeping enhanced backdrop
  artwork bundled in the VPK and loading it on demand, rather than mapping every
  image at startup. No extra artwork installation is required.
- Set explicit Vita heap and main-thread stack reservations, and strengthened
  package checks for executable memory use and bundled artwork.
- Updated application and package versions across the release platforms.

## Known limits

Real-console Vita boot and sustained gameplay still need confirmation; a
successful build does not establish hardware compatibility. The separate AMD
Vulkan device-loss reports, Android/Pocket 6 audio crackling, some corridor and
background variants, and intermittent space-scene brightening remain under
investigation. These changes are not claimed to resolve every graphics report.

Antivirus detections against earlier Windows downloads remain under
investigation and are not considered resolved by this version bump. Do not
disable antivirus protection to install the game.
