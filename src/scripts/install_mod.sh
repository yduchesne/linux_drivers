#!/bin/bash

set -e

if [ -z "$1" ]; then
    echo "expected: $0 <module name>"
    exit 1
fi

mod_name="$1"

existing=$(lsmod | grep $mod_name | cat)

if [ -n "$existing" ]; then
    echo "Module $mod_name already installed. Skipping installation."
    exit 0
fi

if ! [ -f "./$mod_name.ko" ]; then
    echo "$mod_name.ko not found."
    exit 1
fi

insmod "./$mod_name.ko"
echo "Installed module: $mod_name."
