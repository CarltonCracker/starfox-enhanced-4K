# Enhanced-background and screen-fill audit

## Scope and reproducibility

The October 16, 2026 audit separates sampling invariants, cartridge route
coverage, and actual captured views. A successful direct-entry run is **not**
proof that every branch, boss, cutscene or camera pose was visited.

- 21 Original and 43 EX playable level entries, including the available
  Black Hole, special and comet entries.
- Two bounded 8,000-tick runs per entry: stationary god mode, then eight
  horizontal/vertical input directions with pulsed laser fire. Both use the
  real SPC interpreter with synchronized output ports. No MSU audio.
- The stationary routes reached 54 background names; moving routes reached
  58. All 128 runs completed without an audit exception. The maximum fitted
  horizon residual in the moving sample was 0.762 logical pixels.
- One baseline enhanced-sky/ground, 32:9 software capture at tick 1,000 for
  every entry, followed by a 121-frame held-bank probe for every entry.
  Nine orbital views were recaptured after the final projection correction;
  the reviewed 21/43-view sheets select those newer complete frames without
  altering or duplicating their originals. These are selected visual witnesses,
  not videos covering the complete routes.
- Supplemental training-map views, authored-variant identity probes, actual
  EX 5-1 orbital entry/departure route captures, and EX menu choices 9/25/35.
  Early black startup probes are excluded as visual coverage. A borrowed
  camera can also leave an authored limb offscreen, so the visible entry/exit
  checks use the actual stage route rather than claiming forced views prove it.
- 22,375 rigid-transform checks across banks from -89 to +89 degrees,
  native/wide/ultrawide coordinates, offscreen horizons and scroll seams.
- Every one of the 38 shipped artwork files has a real-texture sampling test;
  the two retained legacy masters also run. Catalogue validation prevents
  newly shipped artwork from silently missing the sweep.

`tools/audit_background_routes.ps1` creates compact text/JSON route receipts.
Use a separate new output directory for `-Movement`. The optional `--audit`
and `--audit-moving` modes of `starfox_background_transition_check` are also
usable directly, without writing captures.

`tools/capture_background_audit.ps1 -Compact` saves one lossless PNG per
sample and removes only its own intermediate BMP. Its output is constrained
to `build/validation/active`; existing captures are not overwritten and
storage checks reserve 20 GiB free within the 1 GiB scratch limit.
`tools/make_background_audit_sheet.py --levels` arranges those PNGs for review.
No ROM/BIN files or additional embedded game executables are copied.

## Findings and corrections

1. Packed landscape skies could generate a panorama V coordinate of 2 or
   greater, accidentally selecting their moon section. All packed panorama
   paths now keep those coordinates distinct from the explicitly tagged
   celestial footprints. Unique moons remain unique.
2. The city/moon atlas could retain native sky wedges below an estimated
   horizon even when the source pixel explicitly identified sky. Source sky
   ownership now matches the other landscape paths on CPU and GPU.
3. Downward orbital views (notably the shared jade-planet backgrounds) could
   clamp to the photograph's bottom row and extrude vertical columns. The
   orbital continuation uses the existing top-down surface master instead,
   blending below the panorama into the VR renderer's conformal stereographic
   disk toward the nadir (not a radial texture fan). Its
   monotonic coordinate encoding reserves the moon tags without a lower
   panorama clamp. Sky, surface and moon occupy separate immutable atlas
   sections. It remains a distant planet, not newly generated local terrain.
4. White/red death/revival screen-fill programs request radius 197 but stop
   growing at 171: speed 18, acceleration -1. That covers the native viewport
   but misses expanded corners. Only those bytecode programs now scale their
   presentation radius to the logical canvas, including supersampled edge
   pixels. Simulation/save formats, ordinary bomb circles and native OBJ/HUD
   protection remain unchanged.
5. Clamped photographic skies now prepare their narrow zenith edge, preventing
   variable cloud/star pixels from extruding into columns above the image.
   Artwork below the first eight source rows, including the horizon, is exact.
6. A photographic panorama could repeat within an ultrawide or banked view.
   One uniform, canvas-diagonal-based cover scale keeps a view within one
   horizontal period without changing image proportions, source scroll phase
   or the authored horizon. It is independent of the current bank, avoiding
   camera-dependent zoom. Explicitly tiled space patterns and registered
   celestial subjects retain their existing mappings.

`starfox_enhanced_terrain_tests` exercises 81 packed-sky extent cases and 108
orbital-continuation cases. `STARFOX_TEST_BACKDROP_PROJECTION_ONLY=1` selects
the corresponding CPU/GPU comparison in `starfox_gpu_effects_check`, alongside
21 effect-order and 36 completed screen-fill cases. Shader changes must be
regenerated for every portable backend; the native D3D11/DXR dependencies
also include the shared sampler.

The final renderer passed the selected 43 local CTests, the full Vulkan and
D3D12 effect regressions, and the targeted Vulkan/D3D12 cases above. The
`STARFOX_TEST_D3D11_BACKDROP_PROJECTION_ONLY=1` check passed the 189 supported
native D3D11 backdrop cases; that backend retains its matching CPU fallback
for circle/fade effects. A full-size, real-artwork Vulkan gameplay capture
also confirmed the native GPU path. Linux desktop/shared-VR build and source
regressions passed in run
[38851948515](https://github.com/kandowontu2/starfox-enhanced/actions/runs/38851948515)
for renderer commit `493e1477`; subsequent changes extend only the audit/test
harness and this report.

## Remaining coverage limits

Training and some late/alternate routes are not completed by the two bounded
input patterns. In particular, the moving sweep did not visit the names
`BG_TRAINING`, `BG_1_7B`, `BG_2_5`, `BG_6_7B` or `BG_6_7C`; some may be covered
by the stationary path, be aliases of an already reached background ID, or
share tested artwork. Supplemental initialized snapshots inspect these variants,
but are not completed-route coverage. `capture_background_audit.ps1 -Background`
checks that the requested authored ID survives until capture. Use sufficient
preroll and omit `-AudioPreroll` for that existing finite diagnostic, which loads
the variant after preroll; avoid treating an uninitialized black frame as a pass.

These checks cannot certify every possible camera trajectory or hardware
backend. New stage/angle regressions should retain the actual background ID,
PPU layout, scroll/roll state, renderer and a selected witness. Nintendo 3DS
development remains paused and is not part of this audit.
