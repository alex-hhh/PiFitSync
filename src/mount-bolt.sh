#!/usr/bin/bash

#http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'
script_name=${0##**/}

if [ -r /var/run/fit-sync/mtp-bolt-device ]; then
    device=`cat /var/run/fit-sync/mtp-bolt-device`
    /usr/bin/jmtpfs -o allow_other -device=$device /media/bolt
else
    echo "$script_name: no MTP device to mount"
fi
