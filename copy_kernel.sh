#!/bin/sh
set -eu

mkdir -p iso/boot
cp build/kernel.bin iso/boot/kernel.bin