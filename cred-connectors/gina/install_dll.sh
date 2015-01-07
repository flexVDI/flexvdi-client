#!/bin/bash

VM_IMG="$1"
DLL="$2"
GUEST_DLL=flexVDIGina.dll

if ! test -f "$VM_IMG" -o -f "$DLL"; then
    echo Usage: install_dll.sh VM_IMAGE gina.dll
    exit 1
fi

guestfish --rw --add "$VM_IMG" -i upload "$DLL" /WINDOWS/system32/"$GUEST_DLL"

