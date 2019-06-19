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

PREFIX="$1"
BUILD_TYPE="$2"
if file src/flexvdi-client.exe | grep -q x86-64; then
    ARCH=x86_64
    sed -i s/PROGRAMFILES32/PROGRAMFILES64/ setup.nsi
else
    ARCH=i686
fi
SYSTEM_DLLS="kernel32.dll user32.dll msvcrt.dll gdi32.dll msimg32.dll advapi32.dll \
    ole32.dll shell32.dll ws2_32.dll dnsapi.dll iphlpapi.dll usp10.dll dwmapi.dll \
    imm32.dll setupapi.dll winmm.dll comctl32.dll comdlg32.dll imm32.dll crypt32.dll \
    dsound.dll"


walk_dlls() {
    ${ARCH}-w64-mingw32-objdump -p "$1" | grep -i '\.dll$' | sed 's/.*\s\(\S\S*\)/\1/' | while read dll_name; do
        local dll_path=$(find "${DLL_DIRS[@]}" -maxdepth 1 -iname "$dll_name" -print -quit)
        if [ -z "$dll_path" -o ! -f "$dll_path" ]; then
            echo $SYSTEM_DLLS | grep -iq "$dll_name" || echo "$dll_name ($dll_path) not found"
        elif [ ! -f "output/bin/$dll_name" -o "output/bin/$dll_name" -ot "$dll_path" ]; then
            echo "Installing $dll_name"
            cp "$dll_path" "output/bin/$dll_name"
            walk_dlls "$dll_path"
        fi
    done
}

rm -fr output
mkdir -p output/bin output/lib/gio/modules output/lib/gstreamer-1.0
cp src/flexvdi-client.exe output/bin

# Copy gio TLS and GStreamer modules
cp "$PREFIX"/lib/gio/modules/libgiognutls.dll output/lib/gio/modules
cp "$PREFIX"/lib/gstreamer-1.0/libgst{app,coreelements,audioconvert,audioresample,autodetect,playback,jpeg,videofilter,videoconvert,videoscale,deinterlace,directsound}.dll output//lib/gstreamer-1.0

# Find DLLs
DLL_DIRS=( $(find "$PREFIX" $(${ARCH}-w64-mingw32-gcc -print-sysroot) -iname "*.dll" -type f -exec dirname \{\} \; 2>/dev/null | sort -u) )
for bin in $(find output -type f) ; do
    walk_dlls "$bin"
done

# strip dlls
if [ "$BUILD_TYPE" != "Debug" ]; then
    find output -iname "*.dll" -exec ${ARCH}-w64-mingw32-strip \{\} + >& /dev/null
fi

cp "$PREFIX"/bin/usb.ids output/bin

makensis setup.nsi
