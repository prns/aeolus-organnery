# Standalone Linux AppImage

This packaging target builds Aeolus as a relocatable x86_64 AppImage. It bundles
the application, its dynamically loaded X11/text UI plugins, their non-base
shared-library dependencies, and the Debian `stops` instrument-data package.

## Build

On Ubuntu 22.04 (or a compatible Debian/Ubuntu system), install the packages
listed in `.github/workflows/appimage.yml`, then run:

```sh
bash packaging/appimage/build.sh
```

The resulting `Aeolus-x86_64.AppImage` can be moved and run without installing
Aeolus or its build dependencies. Make it executable if needed:

```sh
chmod +x Aeolus-x86_64.AppImage
./Aeolus-x86_64.AppImage
```

The AppImage includes the instrument definitions. Audio hardware, ALSA/JACK
services, MIDI devices, and a working X11 desktop remain host-system services;
the AppImage does not install or start those services. JACK is the default
backend. Use Aeolus' `-A` option for ALSA.

A GitHub Actions build is available from **Actions → Build standalone Linux
AppImage**. It also runs automatically when the packaging or application
sources change on the `standalone-appimage` branch.
