#!/usr/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'
script_name=${0##**/}

kernel_name=$1

if [ -z "$kernel_name" ]; then
    echo "$script_name: missing kernel device name"
    exit 1
fi

# This script is invoked for every USB device that is removed, so we need to
# check if our device is the one that was actually removed.

for d in fr945 edge540 bolt; do
    if [ -r /var/run/fit-sync/mtp-$d-kname ]; then
        our_kernel_name=`cat /var/run/fit-sync/mtp-$d-kname`
        if [[ $kernel_name == $our_kernel_name ]]; then
            /usr/bin/systemctl stop mount-$d.service
            rm -f /var/run/fit-sync/mtp-$d-kname
            rm -f /var/run/fit-sync/mtp-$d-device
        fi
    fi
done
