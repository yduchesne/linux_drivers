#!/bin/bash

set -e

if [ -z "$1" ]; then
    echo "expected $0 <module name>"
    exit 1
fi

mod_name="$1"

existing=$(lsmod | grep $mod_name | cat)

if [ -z "$existing" ]; then
    echo "Module $mod_name is not currently installed. Skipping uninstallation."
    exit 0
fi

rmmod "$mod_name"
echo "Uninstalled module: $mod_name."
