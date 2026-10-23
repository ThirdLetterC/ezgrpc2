#!/usr/bin/env bash
# Builds flatcc, the FlatBuffers compiler and runtime for C, and installs it
# into a prefix that the ezgrpc2 build can use:
#
#   scripts/build-flatcc.sh <prefix> [workdir]
#   cmake -S . -B build-flatcc -DEZGRPC2_FLATCC=ON -DEZGRPC2_FLATCC_PREFIX=<prefix>
#
# The source is pinned to a release and verified against a SHA-256 checksum.
# The prefix receives bin/flatcc, include/flatcc and lib/libflatccrt.a.
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 <prefix> [workdir]" >&2
  exit 2
fi

prefix=$(realpath -m "$1")
workdir=$(realpath -m "${2:-$(mktemp -d)}")
jobs=$(nproc 2>/dev/null || echo 4)

version=0.6.4
url=https://github.com/dvidelabs/flatcc/archive/refs/tags/v$version.tar.gz
sha256=463c1cbc95777a01e103a6c961325f28e8876cb44a195b5c0f8d1117ae019580
srcdir=flatcc-$version

mkdir -p "$workdir" "$prefix"
tarball=$workdir/flatcc-$version.tar.gz

if [ ! -f "$tarball" ]; then
  curl -fsSL -o "$tarball.part" "$url"
  mv "$tarball.part" "$tarball"
fi
echo "$sha256  $tarball" | sha256sum -c -

rm -rf "${workdir:?}/$srcdir"
tar -xzf "$tarball" -C "$workdir"
src=$workdir/$srcdir

# the runtime is linked into libezgrpc2.so as well, so it must be PIC
cmake -S "$src" -B "$src/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DFLATCC_INSTALL=ON \
  -DFLATCC_TEST=OFF \
  -DFLATCC_CXX_TEST=OFF
cmake --build "$src/build" -j "$jobs"
cmake --install "$src/build"

echo "installed flatcc $version into $prefix"
