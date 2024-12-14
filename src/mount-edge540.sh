#!/usr/bin/bash

#http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'
script_name=${0##**/}

if [ -r /var/run/fit-sync/mtp-edge540-device ]; then
    device=`cat /var/run/fit-sync/mtp-edge540-device`
    /usr/bin/jmtpfs -o allow_other -device=$device /media/edge540
else
    echo "$script_name: no MTP device to mount"
fi
