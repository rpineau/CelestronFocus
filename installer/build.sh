#!/bin/bash

mkdir -p ROOT/tmp/CelestronFocus_X2/
cp "../CelestronFocus.ui" ROOT/tmp/CelestronFocus_X2/
cp "../focuserlist CelestronFocus.txt" ROOT/tmp/CelestronFocus_X2/
cp "../build/Release/libCelestronFocus.dylib" ROOT/tmp/CelestronFocus_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.CelestronFocus_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 CelestronFocus_X2.pkg
pkgutil --check-signature ./CelestronFocus_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.CelestronFocus_X2 --scripts Scripts --version 1.0 CelestronFocus_X2.pkg
fi

rm -rf ROOT
