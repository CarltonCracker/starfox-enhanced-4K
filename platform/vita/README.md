# PS Vita native build

`StarFoxEnhanced.vpk` is a native VitaSDK/SDL3 build for PS Vita and
PlayStation TV. Install it with VitaShell or another VPK installer.

The game data is not included. On a PC, use `starfox_asset_builder` with a
supported clean retail ROM to create `Starfox-Assets.BIN`, then copy it to:

```
ux0:data/StarFoxEnhanced/Starfox-Assets.BIN
```

The optional music companion is discovered case-insensitively at:

```
ux0:data/StarFoxEnhanced/Starfox-MSU1.PAK
```

The same directory is used for persistent settings and Star Fox EX SRAM.
This target uses the Vita's native buttons and sticks; mobile on-screen
controls are not displayed.

The VPK is cross-compiled and its package structure is verified. Boot,
controller input, audio latency and sustained performance still need a real
Vita/PlayStation TV test. Start with 1x Render Upscale on this hardware.

The package includes an opaque bubble icon and a complete LiveArea background/
launch gate. Build validation checks those images, their template references,
the application title/version, and the SELF header. It does not establish that
the game boots or resolve a console-specific launch error. The source-only
`Portable platform builds` workflow (`source_checks_only=true`,
`source_check_os=vita-package`) tests the same packaging helper with a tiny SDK
program and the actual runtime, without uploading game data or publishing a release.

## Runtime memory

Enhanced backdrop artwork stays bundled inside the VPK, but is read lazily from
`app0:/assets/enhanced-backdrops/` rather than mapped as executable data. No
extra user copy is needed. The released 0.0.8.7 SELF mapped about 177 MiB of
code/resources before allocating the heap, which is incompatible with also
reserving VitaSDK newlib's default 128 MiB heap within a normal application.
The runtime now explicitly reserves a 96 MiB heap and a 2 MiB main-thread stack,
leaving room for display/audio/GXM allocations. Package checks reject SELF load
reservations over the project's 64 MiB executable budget and missing artwork.

See [VitaSDK's heap initialization](https://github.com/vitasdk/newlib/blob/vita/newlib/libc/sys/vita/sbrk.c)
and [main-thread configuration symbols](https://github.com/vitasdk/vita-toolchain#vita-elf-create).
These checks prevent the known memory-budget conflict; successful real-console
startup and gameplay still require confirmation.

## Building

Install [VitaSDK](https://vitasdk.org/), including `zlib` and `libvita2d`, put
its `bin` directory on `PATH`, set `VITASDK`, then run:

```sh
tools/build_vita.sh
```

The output is written to `dist/StarFoxEnhanced-vita`.
