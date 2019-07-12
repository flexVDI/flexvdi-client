#!/bin/bash

# Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

# This file is part of flexVDI Client.

# flexVDI Client is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# flexVDI Client is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.

export PREFIX="$1"
export PKG_NAME="$2"
export PKG_CONFIG_PATH="$PREFIX"/lib/pkgconfig:"$PREFIX"/share/pkgconfig

set -e

export SRCDIR=`dirname "$0"`/..

rm -fr icons.iconset flexvdi-client.app "${PKG_NAME}.tmp.dmg" "${PKG_NAME}.dmg"
ICONDIR=icons.iconset
mkdir $ICONDIR
trap "rm -fr $ICONDIR" EXIT
ORIGICON=${SRCDIR}/resources/images/icon.png

# Multi-resolution icons
for SIZE in 16 32 64 128 256; do
    sips -z $SIZE $SIZE $ORIGICON --out $ICONDIR/icon_${SIZE}x${SIZE}.png
done
iconutil -c icns -o flexvdi-client.icns $ICONDIR

python2.7 -c "import bundler.main; bundler.main.main(['${PKG_NAME}.bundle'])"

# Sign app bundle
IDENTITY="Developer ID Application: Flexible Software Solutions S.L."
codesign -s "$IDENTITY" -fv flexvdi-client.app/Contents/MacOS/{flexvdi-client-bin,gst-plugin-scanner}
find flexvdi-client.app/Contents/Resources/lib -type f | xargs codesign -s "$IDENTITY" -fv
codesign -s "$IDENTITY" -fv flexvdi-client.app

# Create the DMG image
hdiutil create -size 45m -fs HFS+ -volname "${PKG_NAME}" "${PKG_NAME}.tmp.dmg"
DEVS=$(hdiutil attach "${PKG_NAME}.tmp.dmg" | cut -f 1)
DEV=$(echo $DEVS | cut -f 1 -d ' ')
MOUNTPOINT=/Volumes/"${PKG_NAME}"
mkdir $MOUNTPOINT/.background
cp -a flexvdi-client.app $MOUNTPOINT
cp $SRCDIR/resources/images/dmg_background.png $MOUNTPOINT/.background

echo '
   tell application "Finder"
     tell disk "'${PKG_NAME}'"
           open
           set current view of container window to icon view
           set toolbar visible of container window to false
           set statusbar visible of container window to false
           set the bounds of container window to {200, 100, 840, 580}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 88
           set background picture of theViewOptions to file ".background:dmg_background.png"
           make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
           set position of item "flexvdi-client.app" of container window to {200, 300}
           set position of item "Applications" of container window to {440, 300}
           update without registering applications
           delay 5
           close
     end tell
   end tell
' | osascript
 
cp flexvdi-client.icns $MOUNTPOINT/.VolumeIcon.icns
SetFile -a C $MOUNTPOINT
chmod -Rf go-w $MOUNTPOINT
sync
sync

# Unmount the disk image
hdiutil detach $DEV
hdiutil convert "${PKG_NAME}.tmp.dmg" -format UDZO -o "${PKG_NAME}.dmg"

codesign -s "$IDENTITY" -fv "${PKG_NAME}.dmg"
