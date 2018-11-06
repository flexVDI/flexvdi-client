#!/bin/bash
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