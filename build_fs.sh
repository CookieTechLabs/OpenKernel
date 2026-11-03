#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <kernel-version>" >&2
    exit 2
fi

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
version=$1
build_dir="$repo_root/build"
module_dir="$build_dir/modules/$version"
iso_boot_dir="$repo_root/iso/boot"

mkdir -p "$build_dir" "$iso_boot_dir/grub"
stage=$(mktemp -d "$build_dir/initramfs.XXXXXX")
cleanup() {
    rm -rf "$stage"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

cp -R "$repo_root/initramfs/." "$stage/"
mkdir -p "$stage/lib/modules/$version"

if [ -d "$module_dir" ]; then
    for module in "$module_dir"/*.ko; do
        [ -f "$module" ] || continue
        cp "$module" "$stage/lib/modules/$version/"
    done
fi

tar --format=ustar -C "$stage" -cf "$iso_boot_dir/initramfs.img" .