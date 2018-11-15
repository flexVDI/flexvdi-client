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

BIN="$1"
CERT="$2"
PASS="$3"

usage() {
    [ -n "$1" ] && echo Error: $1
    echo Usage: $0 installer pkcs12_cert [password]
    exit 1
}

[ -f "$BIN" ] || usage "The installer $BIN does not exist"

if osslsigncode verify "$BIN" > /dev/null; then
    echo The installer is already signed
    exit 0
fi

[ -f "$CERT" ] || usage "The certificate $CERT does not exist"

if [ -z "$PASS" ]; then
    read -s -p "Certificate password: " PASS
    echo
fi

osslsigncode sign -pkcs12 "$CERT" -n "flexVDI Client" -i "https://flexvdi.com" \
    -pass "$PASS" -t http://timestamp.digicert.com -h sha1 \
    -in "$BIN" -out "${BIN/.exe/-signed.exe}"

if [ -f "${BIN/.exe/-signed.exe}" ]; then
    mv "$BIN" "${BIN/.exe/-unsigned.exe}"
    mv "${BIN/.exe/-signed.exe}" "$BIN"
fi