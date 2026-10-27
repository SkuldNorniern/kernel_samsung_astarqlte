#!/bin/bash
# Build Image.gz-dtb for the Galaxy A8 Star (SM-G885S).
# usage: CROSS_COMPILE=/path/to/aarch64-linux-android-4.9/bin/aarch64-linux-android- ./build.sh [defconfig]
set -e
DEFCONFIG=${1:-astarqlte_kor_skt_defconfig}
: "${CROSS_COMPILE:?set CROSS_COMPILE to the aarch64-linux-android-4.9 gcc prefix}"
export ARCH=arm64
make O=out KCFLAGS=-mno-android "$DEFCONFIG"
make O=out KCFLAGS=-mno-android -j"$(nproc)"
ls -l out/arch/arm64/boot/Image.gz-dtb
