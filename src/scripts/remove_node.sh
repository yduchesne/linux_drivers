#!/bin/bash

set -e

if [ -z "$1" ]; then
    echo "expected: $0 <module name>"
    exit 1
fi

mod_name="$1"

if ! [ -e "/dev/$mod_name" ]; then
    echo "Node /dev/$mod_name does not exist. Omitting removal of device node."
    exit 0
fi

rm -f /dev/$mod_name
echo "Removed device node: /dev/$mod_name."