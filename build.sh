#!/bin/bash
# Build Image.gz-dtb (linux 4.19, sdm660).
# Needs: clang (Android clang-r416183b or newer) on PATH and GNU binutils:
#   sudo apt install gcc-9-aarch64-linux-gnu
# usage: ./build.sh [defconfig]
set -e
DEFCONFIG=${1:-vendor/asus/X00TD_defconfig}   # until astarqlte_defconfig exists
export ARCH=arm64
M="O=out CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-"
make $M "$DEFCONFIG"
make $M -j"$(nproc)"
ls -l out/arch/arm64/boot/Image.gz-dtb
