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

for kname_file in /var/run/fit-sync/mtp-*-kname; do
    if [ -r $kname_file ]; then
        our_kernel_name=`cat $kname_file`
        if [[ $kernel_name == $our_kernel_name ]]; then
            d=`echo $kname_file | sed 's/.*-\(.*\)-.*/\1/'`
            /usr/bin/systemctl stop mount-$d.service
            rm -f /var/run/fit-sync/mtp-$d-kname
            rm -f /var/run/fit-sync/mtp-$d-device
        fi
    fi
done
