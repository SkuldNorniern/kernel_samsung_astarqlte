#!/bin/bash
# Build Image.gz-dtb for the Galaxy A8 Star (SM-G885S).
# Needs GCC >= 5.1 for arm64 (linux 4.4.302): sudo apt install gcc-9-aarch64-linux-gnu
# usage: ./build.sh [defconfig]
set -e
DEFCONFIG=${1:-astarqlte_kor_skt_defconfig}
export ARCH=arm64
M="O=out CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-} CC=${CC:-aarch64-linux-gnu-gcc-9}"
make $M "$DEFCONFIG"
make $M -j"$(nproc)"
ls -l out/arch/arm64/boot/Image.gz-dtb
