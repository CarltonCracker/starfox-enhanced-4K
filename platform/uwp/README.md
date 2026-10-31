# Xbox UWP Developer Mode build

This is an x64 UWP package for Xbox One and Xbox Series consoles running in
Developer Mode. Upload the `.appx`, the included x64 package in `Dependencies`,
and the `.cer` through Xbox Device Portal. It is not a retail Xbox package and
cannot run outside Developer Mode.

Current alpha package: **0.0.5.1** (check all four version numbers in Device
Portal). It includes the fixes tested in the 0.0.3 revisions below. Revision
2 fixed accidental desktop C++ runtime imports and the audio startup deadlock;
the Series S tester now confirms that it boots. Revision 3 enables the WinRT
controller backend, which was disabled even though it is the only native
controller backend in this UWP build. Console controller input still requires
tester verification. Revision 4 retains that controller fix and includes the
shared hit-list fixes (scheduler cadence, reticle identity, tunnel scanlines,
background wrapping and the EX Venom handoff).

Back up the app's LocalState files before uninstalling (uninstall removes
internal app data). Remove an older `StarFoxEnhanced.Dev` deployment before installing a rebuilt
development package, then upload the current `.appx`, `.cer`, and x64 VCLibs
dependency together. This avoids retaining an older package with the same
public identity or an obsolete development signing certificate.

The app uses its UWP `LocalState` directory as internal writable storage. Put
`Starfox-Assets.BIN` directly in the `LocalState` root. `Starfox-MSU1.PAK` is
optional and may be put in the same directory. Both names are discovered
without regard to letter case. If the asset pack is missing, launch the app
once and leave its provisioning screen open, upload the file through Device
Portal's File Explorer, and the game will continue automatically. Settings,
controller/keyboard choices, HUD layouts, and any rebuilt asset companion are
saved below LocalState by the app.

If the console returns to Home during launch, retrieve
`StarFoxEnhanced-startup.log` from the LocalState root and include it with the
bug report, along with the Xbox model and installed four-part package version.
It records entry into WinMain, activation, SDL initialization, asset loading,
audio initialization and the first presented frame. If this file is absent,
say so: a loader failure can occur before any game code or logging runs. In
that case include Device Portal's launch error/crash report if available.

If the game boots but the controller is unresponsive, include this same log.
Revision 3 records the WGI setting, detected joysticks/gamepads, device names,
hotplug changes, and the first received controller input. Try the controller
both connected at launch and reconnected while the menu is open.

The included `.cer` contains only the public development certificate. If the
console asks for a certificate while deploying, upload that file with the
package. The private signing key is generated during the build and is never
placed in the distribution folder.

Build on Windows with Visual Studio 2022's Universal Windows Platform C++
tools and the Windows 11 SDK installed:

```powershell
.\tools\build_xbox_uwp.ps1
```

The build validates the executable's imports against the bundled VCLibs
framework before packaging. Desktop C++ runtime imports are rejected, even if
the executable happens to launch successfully on a Windows PC.

## MODERN renderer on Xbox (untested on console)

MODERN, with its DYNAMIC LIGHTING ray tracing, runs on D3D12 through SDL's GPU
renderer. Every other renderer keeps using Direct3D 11. The game switches to
D3D12 only while RENDERER is MODERN. If the console cannot create a D3D12
device, the game logs why and keeps drawing with Direct3D 11.

To test, set the package to Game mode, which gives it the full GPU. In Dev
Home, highlight Star Fox Enhanced, press the Menu button, choose View details
and set App type to Game. Then:

1. Choose RENDERER: MODERN in the pre-game menu, then start a stage.
2. Get `StarFoxEnhanced-startup.log` from LocalState. It must contain
   `SDL GPU device created: driver=direct3d12 adapter=...` and
   `MODERN frame presented`. A line starting `SDL GPU (D3D12) unavailable`
   or `MODERN fallback` gives the reason it did not run.
3. For timings, set VSYNC OFF and PRESENTATION FPS to its highest value.
   Play Corneria past the launch tunnel for at least 30 seconds each with
   DYNAMIC LIGHTING OFF, ON with SHADOW SOFTNESS HARD, and ON with SHADOW
   SOFTNESS HIGH. While MODERN draws, the log gets a `MODERN timing:` line
   every 10 seconds: fps, average and worst frame time, output size and the
   lighting settings in force. Report the Xbox model with the log.
