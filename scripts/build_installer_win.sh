#!/bin/bash

PREFIX="$1"
if file src/flexvdi-client.exe | grep -q x86-64; then
    ARCH=x86_64
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
cp "$PREFIX"/bin/usb.ids output/bin

# Copy gio TLS and GStreamer modules
cp "$PREFIX"/lib/gio/modules/libgiognutls.dll output/lib/gio/modules
cp "$PREFIX"/lib/gstreamer-1.0/libgst{app,coreelements,audioconvert,audioresample,autodetect,playback,jpeg,videofilter,videoconvert,videoscale,deinterlace,directsound}.dll output//lib/gstreamer-1.0

# Find DLLs
DLL_DIRS=( $(find "$PREFIX" $(${ARCH}-w64-mingw32-gcc -print-sysroot) -iname "*.dll" -type f -exec dirname \{\} \; 2>/dev/null | sort -u) )
for bin in $(find output -type f) ; do
    walk_dlls "$bin"
done

# strip dlls
find output -iname "*.dll" -exec ${ARCH}-w64-mingw32-strip \{\} + >& /dev/null

makensis setup.nsi
