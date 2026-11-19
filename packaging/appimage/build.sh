#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
APPDIR="$ROOT/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib/aeolus" "$APPDIR/usr/share/aeolus"

# Build the application and its GUI plugins.
# LIBDIR is retained as a fallback; AppRun supplies the relocatable path.
python3 - <<'PY'
from pathlib import Path
p = Path("source/main.cc")
s = p.read_text()
old = '''    if (t_opt) sprintf (s, "%s/aeolus_txt.so", LIBDIR);
    else       sprintf (s, "%s/aeolus_x11.so", LIBDIR);'''
new = '''    const char *libdir = getenv ("AEOLUS_LIBDIR");
    if (! libdir || ! *libdir) libdir = LIBDIR;
    if (t_opt) sprintf (s, "%s/aeolus_txt.so", libdir);
    else       sprintf (s, "%s/aeolus_x11.so", libdir);'''
if old not in s:
    raise SystemExit("Could not apply relocatable plugin-path patch to source/main.cc")
p.write_text(s.replace(old, new, 1))
PY

make -C source clean
make -C source -j"$(nproc)" LIBDIR=/usr/lib/aeolus
install -Dm755 source/aeolus "$APPDIR/usr/bin/aeolus"
install -Dm755 source/aeolus_x11.so "$APPDIR/usr/lib/aeolus/aeolus_x11.so"
install -Dm755 source/aeolus_txt.so "$APPDIR/usr/lib/aeolus/aeolus_txt.so"

# Aeolus' organ definitions are distributed separately from its source.
# Bundle Debian's architecture-independent stops package so the default
# instrument works without a system-wide Aeolus installation.
mkdir -p "$ROOT/.packaging/stops"
(
  cd "$ROOT/.packaging/stops"
  apt-get download stops
  DEB="$(find . -maxdepth 1 -name 'stops_*.deb' -print -quit)"
  test -n "$DEB"
  dpkg-deb -x "$DEB" "$APPDIR"
)

install -Dm755 packaging/appimage/AppRun "$APPDIR/AppRun"
install -Dm644 packaging/appimage/aeolus.desktop "$APPDIR/aeolus.desktop"
install -Dm644 packaging/appimage/aeolus.svg "$APPDIR/aeolus.svg"

# linuxdeploy copies the non-base shared libraries needed by the executable
# and both dynamically-loaded UI plugins into the AppDir.
LINUXDEPLOY="${LINUXDEPLOY:-$ROOT/linuxdeploy-x86_64.AppImage}"
if [[ ! -x "$LINUXDEPLOY" ]]; then
  curl -L --fail --retry 3 \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    -o "$LINUXDEPLOY"
  chmod +x "$LINUXDEPLOY"
fi
"$LINUXDEPLOY" --appimage-extract-and-run \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/aeolus" \
  --library "$APPDIR/usr/lib/aeolus/aeolus_x11.so" \
  --library "$APPDIR/usr/lib/aeolus/aeolus_txt.so" \
  --desktop-file "$APPDIR/aeolus.desktop" \
  --icon-file "$APPDIR/aeolus.svg" \
  --output appimage

