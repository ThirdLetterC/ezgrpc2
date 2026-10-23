#!/usr/bin/env bash
# Builds a TLS library that is not packaged by most distributions and
# installs it into a prefix that the ezgrpc2 build can use:
#
#   scripts/build-tls-backend.sh <libressl|boringssl|awslc|rustls> <prefix> [workdir]
#   make TLS=<backend> TLS_PREFIX=<prefix>
#
# Sources are pinned to a release and verified against a SHA-256 checksum.
# OpenSSL, wolfSSL and GnuTLS are taken from the system packages instead.
set -euo pipefail

if [ $# -lt 2 ]; then
  echo "usage: $0 <libressl|boringssl|awslc|rustls> <prefix> [workdir]" >&2
  exit 2
fi

backend=$1
prefix=$(realpath -m "$2")
workdir=$(realpath -m "${3:-$(mktemp -d)}")
jobs=$(nproc 2>/dev/null || echo 4)

case "$backend" in
  libressl)
    version=4.3.3
    url=https://cdn.openbsd.org/pub/OpenBSD/LibreSSL/libressl-$version.tar.gz
    sha256=ff97c432457f349e6ba3d416ab903bc7468f1436f0f32efe5fff808de292c7b8
    srcdir=libressl-$version
    ;;
  boringssl)
    version=0.20260929.0
    url=https://github.com/google/boringssl/archive/refs/tags/$version.tar.gz
    sha256=04da9ba0664e0a7f028e961c38d604f2cc6dac852a84ca0e51dc1fa051d4c8fe
    srcdir=boringssl-$version
    ;;
  awslc)
    version=5.10.0
    url=https://github.com/aws/aws-lc/archive/refs/tags/v$version.tar.gz
    sha256=dcac84da23dcbdd38f297f64eb3f6c419c240730f70fbb319ea93c4c54a6084c
    srcdir=aws-lc-$version
    ;;
  rustls)
    version=0.15.4
    url=https://github.com/rustls/rustls-ffi/archive/refs/tags/v$version.tar.gz
    sha256=f9d7416a0e8a3678e4192244639ab0a5cadfd12802001dc1c43491a27f12d1bc
    srcdir=rustls-ffi-$version
    ;;
  *)
    echo "unknown backend: $backend" >&2
    exit 2
    ;;
esac

mkdir -p "$workdir" "$prefix"
tarball=$workdir/$backend-$version.tar.gz

if [ ! -f "$tarball" ]; then
  curl -fsSL -o "$tarball.part" "$url"
  mv "$tarball.part" "$tarball"
fi
echo "$sha256  $tarball" | sha256sum -c -

rm -rf "${workdir:?}/$srcdir"
tar -xzf "$tarball" -C "$workdir"
src=$workdir/$srcdir

cmake_install() {
  cmake -S "$src" -B "$src/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=ON \
    "$@"
  cmake --build "$src/build" -j "$jobs"
  cmake --install "$src/build"
}

case "$backend" in
  libressl)
    cmake_install -DLIBRESSL_APPS=OFF -DLIBRESSL_TESTS=OFF
    ;;
  boringssl)
    cmake_install -DBUILD_TESTING=OFF
    ;;
  awslc)
    cmake_install -DBUILD_TESTING=OFF -DDISABLE_GO=ON -DDISABLE_PERL=ON
    ;;
  rustls)
    # the equivalent of `cargo capi install` without requiring cargo-c
    (cd "$src" && cargo build --release --locked -p rustls-ffi)
    mkdir -p "$prefix/lib" "$prefix/include"
    cp "$src/target/release/librustls_ffi.a" "$prefix/lib/librustls.a"
    cp "$src/librustls/src/rustls.h" "$prefix/include/rustls.h"
    ;;
esac

echo "installed $backend $version into $prefix"
