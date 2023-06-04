#!/bin/bash
#
# Build the shit on mac
#
# You will generally need to add: -DCODESIGN_APP=... to make this work, and (unless you are a
# belnet team member) will need to pay Apple money for your own team ID and arse around with
# provisioning profiles.  See macos/README.txt.
#

set -x

if ! [ -f LICENSE.txt ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level belnet project directory"
fi

mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
      -DBUILD_STATIC_DEPS=ON \
      -DBUILD_LIBBELNET=OFF \
      -DWITH_TESTS=OFF \
      -DWITH_BOOTSTRAP=OFF \
      -DNATIVE_BUILD=OFF \
      -DWITH_LTO=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DMACOS_SYSTEM_EXTENSION=ON \
      -DCODESIGN=ON \
      -DBUILD_PACKAGE=ON \
      "$@" \
      ..
ninja -j1 package

cd ..

echo -e "Build complete, your app is here:\n"
ls -lad $(pwd)/build-mac/Belnet\ *
echo ""
