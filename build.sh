#!/bin/sh
set -eu

make -j"$(nproc)" os.iso
echo "ISO build complete: os.iso"