#!/bin/sh

builddir=$(mktemp -d -p . build.XXXXXXXX)
cd $builddir
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=/usr
cd -
checkinstall --backup=no --fstrans --install=no --default --maintainer "Cyril Margorin <comargo@gmail.com>" --deldoc --deldesc --pkgname meterDigitizer-mqtt --pkgversion 0.1 cmake --build $builddir --target install
rm -rf $builddir
