# Belnet

[Español](readme_es.md) [Русский](readme_ru.md)

Belnet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

You can view documentation on how to get started [here](https://docs.beldex.io/products-built-on-beldex/belnet) .

A simple demo application that is belnet "aware" can be found [here](https://github.com/majestrate/belnet-aware-demos)

[![Build Status](https://ci.beldex.rocks/api/badges/beldex-coin/belnet/status.svg?ref=refs/heads/dev)](https://ci.beldex.rocks/beldex-coin/belnet)

## Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libcurl (for belnet-bootstrap)
* libunbound
* libzmq
* cppzmq
* sqlite3

### Linux

You do not have to build from source if you are on debian or ubuntu as we have apt repositories with pre-built belnet packages on `deb.beldex.io`.

You can install these using:

    $ sudo curl -L https://deb.beldex.io/pub.gpg | sudo apt-key add -
    $ echo "deb  https://deb.beldex.io/apt-repo stable main" | sudo tee /etc/apt/sources.list.d/beldex.list
    $ sudo apt update
    $ sudo apt install belnet-client


If you are not on a platform supported by the debian packages or if you want to build a dev build, this is the most "portable" way to do it:

    $ sudo apt install build-essential cmake git libcap-dev pkg-config automake libtool libuv1-dev libsodium-dev libzmq3-dev libcurl4-openssl-dev libevent-dev nettle-dev libunbound-dev libsqlite3-dev libssl-dev
    $ git clone --recursive https://github.com/Beldex-Coin/belnet
    $ cd belnet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DCMAKE_BUILD_TYPE=Release
    $ make -j$(nproc)
    
If you dont want to do a static build install the dependancies and run:

    $ cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
    $ make -j$(nproc)

install:

    $ sudo make install


supported cross targets:

* aarch64
* armhf
* mips
* mips64
* mipsel
* ppc64le

install the toolchain for `$arch` this example is `aarch64`

    $ sudo apt install g{cc,++}-aarch64-linux-gnu

build 1 or many cross targets:

    $ ./contrib/cross.sh arch_1 arch_2 ... arch_n    

    

### macOS

Belnet ~~is~~ will be available on the Apple App store. 

Source code compilation of Belnet by end users is not supported or permitted by apple on their platforms, see [this](contrib/macos/README.txt) for more information. If you find this disagreeable consider using a platform that permits compiling from source.

### Windows

You can get the latest stable windows release from https://belnet.beldex.io/ or check the releases page on github.

windows builds are cross compiled from debian/ubuntu linux

additional build requirements:

* nsis
* cpack

setup:

    $ sudo apt install build-essential cmake git pkg-config mingw-w64 nsis cpack automake libtool

building:

    $ git clone --recursive https://github.com/Beldex-Coin/belnet
    $ cd belnet
    $ ./contrib/windows.sh

### FreeBSD

build:

    $ pkg install cmake git pkgconf
    $ git clone --recursive https://github.com/Beldex-Coin/belnet
    $ cd belnet
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DBUILD_SHARED_DEPS=ON ..
    $ make

install (root):

    # make install

## Usage

### Debian / Ubuntu packages

When running from debian package the following steps are not needed as it is already ready to use.

## Running on Linux (without debs)

**DO NOT RUN AS ROOT**, run as normal user. 

set up the initial configs:

    $ belnet -g
    $ belnet-bootstrap

after you create default config, run it:

    $ belnet

This requires the binary to have the proper capabilities which is usually set by `make install` on the binary. If you have errors regarding permissions to open a new interface this can be resolved using:

    $ sudo setcap cap_net_admin,cap_net_bind_service=+eip /usr/local/bin/belnet


## Running on macOS/UNIX/BSD

**YOU HAVE TO RUN AS ROOT**, run using sudo. Elevated privileges are needed to create the virtual tunnel interface.

The macOS installer places the normal binaries (`belnet` and `belnet-bootstrap`) in `/usr/local/bin` which should be in your path, so you can easily use the binaries from your terminal. The installer also nukes your previous config and keys and sets up a fresh config and downloads the latest bootstrap seed.

to run, after you create default config:

    $ sudo belnet
