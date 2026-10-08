---
title: Application icons
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Application icons

The Sagan compiler owns the canonical native application-icon assets under
`assets/application/`. A windowed consumer must query the compiler that emits
its C++ and must not copy these files into its repository:

```bash
windows_resource="$(sagan --application-icon windows)"
linux_icon="$(sagan --application-icon linux)"
macos_icon="$(sagan --application-icon macos)"
```

The command validates that the requested installed or build-tree resource is
present. An unknown target names the accepted targets; a missing asset asks the
user to rebuild or reinstall Sagan with application assets. This makes an
incomplete toolchain a packaging error instead of silently producing an
unbranded application.

`SAGAN_TOOLCHAIN_ROOT` is a test and isolated-staging override for the normal
executable-relative toolchain root. If set, it must name a layout containing
the documented `assets/application/` tree; it does not permit a downstream
repository to replace the canonical artwork.

## Windows executable

Pass the returned COFF object to the native linker. It contains the multi-size
ICO resource used by Explorer, the taskbar, and the window switcher:

```bash
g++ program.cpp "$windows_resource" -o program.exe
```

The Windows portable archive and installer stage the resource object and its
canonical ICO. Sagan's own generated-program paths link the same object.

## Linux desktop application

Install the returned PNG under the application's staging root and reference
the installed icon name from its desktop entry:

```bash
install -Dm644 "$linux_icon" \
  AppDir/usr/share/icons/hicolor/512x512/apps/sagan.png
install -Dm644 packaging/sagan.desktop \
  AppDir/usr/share/applications/sagan.desktop
```

The desktop entry uses `Icon=sagan`. A development launch that is not installed
may use an absolute icon path in its generated desktop entry. The renderer must
also set its native window class/application identifier consistently so the
desktop can associate the live window with that entry.

## macOS application bundle

Copy the returned ICNS file into the bundle and reference it by filename from
`Info.plist`:

```bash
mkdir -p Sagan.app/Contents/MacOS Sagan.app/Contents/Resources
cp program Sagan.app/Contents/MacOS/Sagan
cp "$macos_icon" Sagan.app/Contents/Resources/sagan.icns
```

The bundle's `CFBundleExecutable` is `Sagan` and `CFBundleIconFile` is
`sagan.icns`. Launch the bundle rather than the loose Mach-O executable when
verifying Finder, Dock, and application-switcher behavior.

`scripts/application_icon_test.py` validates the committed ICO, PNG, and ICNS
formats on every host. Windows resource inspection additionally verifies the
icon embedded in a generated executable. Downstream repositories test their
own desktop entry or bundle assembly because those names and bundle identifiers
belong to the application, not the compiler.
