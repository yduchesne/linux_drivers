#!/bin/bash

set -e

if [ -z "$1" ]; then
    echo "expected: $0 <module name>"
    exit 1
fi

mod_name="$1"

existing=$(lsmod | grep "$mod_name" | cat)

if [ -z "$existing" ]; then
    echo "Module $mod_name not installed. Cannot invoke mknod for it."
    exit 1
fi

if [ -e "/dev/$mod_name" ]; then
    echo "A device node already exists for module $mod_name. Aborting node creation attempt."
    exit 0
fi

major=$(cat /proc/devices | grep "$mod_name" | awk '{$1=$1};1' | cut -d ' ' -f 1)

mknod /dev/$mod_name c $major 0

echo "Created device node: /dev/$mod_name."


