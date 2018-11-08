#!/bin/bash

PREFIX="$1"
PKG_NAME="$2"
BIN=src/flexvdi-client
export PKG_CONFIG_PATH="$PREFIX"/lib/pkgconfig:"$PREFIX"/share/pkgconfig

set -e

SRCDIR=`dirname "$0"`/..
TMPDIR=`mktemp -d`
trap "rm -fr $TMPDIR" EXIT

cat > $TMPDIR/flexvdi-client.desktop << EOF
[Desktop Entry]
Name=flexVDI client
Exec=flexvdi-client
Icon=icon
EOF
cp "$SRCDIR"/resources/images/icon.png $TMPDIR

copy_with_deps() {
    cp "$@"
    ldd "${@:1:$#-1}" | grep "=>" | sed 's;.* => \(/.*\) (.*;\1;' | grep "$PREFIX" | sort -u | xargs -r cp -t $TMPDIR/lib
}

mkdir -p $TMPDIR/bin $TMPDIR/lib/gstreamer-1.0 $TMPDIR/lib/gio $TMPDIR/share/fonts
copy_with_deps "$BIN" $TMPDIR/bin/flexvdi-client
ldd "$BIN" | sed 's;.* => \(/.*\) (.*;\1;' | grep -e libudev | xargs -r cp -t $TMPDIR/lib
copy_with_deps $(pkg-config gstreamer-1.0 --variable pluginsdir)/libgst{app,coreelements,audioconvert,audioresample,autodetect,playback,jpeg,videofilter,videoconvert,videoscale,deinterlace,alsa,pulseaudio}.so "$TMPDIR"/lib/gstreamer-1.0
copy_with_deps $(pkg-config gstreamer-1.0 --variable prefix)/libexec/gstreamer-1.0/gst-plugin-scanner "$TMPDIR"/bin
copy_with_deps $(pkg-config gio-2.0 --variable giomoduledir)/libgiognutls.so "$TMPDIR"/lib/gio
cp -a "$PREFIX"/lib/gtk-3.0 "$TMPDIR"/lib
find $TMPDIR/{bin,lib} -type f -exec chmod 755 \{\} + -exec strip -s \{\} + &> /dev/null | true

cp -a "$PREFIX"/share/glib-2.0/schemas $TMPDIR/share
cp "$PREFIX"/bin/usb.ids $TMPDIR
cp /usr/share/fonts/truetype/lato/Lato-Reg*.ttf $TMPDIR/share/fonts
cat > $TMPDIR/share/fonts/fonts.conf <<\EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<!-- /etc/fonts/fonts.conf file to configure system font access -->
<fontconfig>
    <dir>share/fonts</dir>
    <cachedir prefix="xdg">fontconfig</cachedir>
</fontconfig>
EOF

cat > $TMPDIR/AppRun <<\EOF
#!/bin/sh
HERE=$(dirname $(readlink -f "${0}"))
if which pax11publish > /dev/null; then
    eval $(pax11publish -i)
fi
export LD_LIBRARY_PATH="${HERE}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="${HERE}/share${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
export GSETTINGS_SCHEMA_DIR=${HERE}/share/schemas${GSETTINGS_SCHEMA_DIR:+:$GSETTINGS_SCHEMA_DIR}
export GST_PLUGIN_SYSTEM_PATH="${HERE}"/lib/gstreamer-1.0
export GST_PLUGIN_SCANNER="${HERE}"/bin/gst-plugin-scanner
export LIBVA_DRIVERS_PATH="${HERE}"/lib
export GIO_MODULE_DIR="${HERE}"/lib/gio
export GTK_PATH="${HERE}"/lib/gtk-3.0
export FONTCONFIG_PATH="${HERE}"/share/fonts

# Currently we change into the APPDIR directory, this only because of gdk-pixbuf
# and pango cache files which need to specify relative paths.
cd "${HERE}"

# Check dependencies are met, to avoid cryptic error messages
ERRORS=`find -type f -executable | xargs ldd | grep "not found" | sort -u`
if [ -n "$ERRORS" ]; then
    echo "*************************************************"
    echo "WARNING!!! There are missing dependencies:"
    echo "*************************************************"
    echo "$ERRORS"
fi

"${HERE}"/bin/flexvdi-client "$@"
EOF
chmod 755 $TMPDIR/AppRun

appimagetool -n $TMPDIR ./${PKG_NAME}.AppImage || {
    echo appimagetool not found, creating just a tar.gz archive
    tar czf ./${PKG_NAME}.appimage.tar.gz -C $TMPDIR .
}