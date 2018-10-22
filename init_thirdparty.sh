#/bin/sh

git submodule init
git submodule update --recursive

cd third-party/RemoteController
git submodule init
git submodule update --recursive
cd ../../

cd third-party/libarchive

/bin/sh build/autogen.sh || exit 1
./configure --prefix=$PWD/output --without-nettle --without-lzma \
--without-iconv --enable-shared=no || exit 1

cd ../../
