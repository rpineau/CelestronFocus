#!/bin/bash

mkdir -p ROOT/tmp/CelestronFocus_X2/
cp "../CelestronFocus.ui" ROOT/tmp/CelestronFocus_X2/
cp "../focuserlist CelestronFocus.txt" ROOT/tmp/CelestronFocus_X2/
cp "../build/Release/libCelestronFocus.dylib" ROOT/tmp/CelestronFocus_X2/

PACKAGE_NAME="CelestronFocus_X2.pkg"
BUNDLE_NAME="org.rti-zone.CelestronFocusX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -s "$app_id_signature" ../build/Release/libCelestronFocus.dylib
fi


if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}

else
    pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
