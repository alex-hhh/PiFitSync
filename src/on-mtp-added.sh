#!/usr/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'
script_name=${0##**/}

# TODO: check if the vendor and product match the Garmin FR945!!!

if [ -z "$1" ]; then
    echo "$script_name: missing device path"
    exit 1
fi

device_path=/sys/$1

if ! [ -d "$device_path" ]; then
    echo "$script_name: non existent path: $device_path"
    exit 1
fi

# The device_path passed to us refers to a MTP "sub-device", the actual device
# is the parent...

actual_device_path=`dirname $device_path`

kernel_name=`basename $actual_device_path`

cd "$actual_device_path"
vendor_id=`cat idVendor`
product_id=`cat idProduct`
busnum=`cat busnum`
devnum=`cat devnum`

if [[ $vendor_id == "091e" && $product_id == "4c29" ]]; then
    echo $kernel_name > /var/run/fit-sync/mtp-fr945-kname
    echo "$busnum,$devnum" > /var/run/fit-sync/mtp-fr945-device
    /usr/bin/systemctl start mount-fr945.service
    /usr/bin/systemctl start sync-fr945.service
elif [[ $vendor_id == "091e" && $product_id == "4fdd" ]]; then
    echo $kernel_name > /var/run/fit-sync/mtp-edge540-kname
    echo "$busnum,$devnum" > /var/run/fit-sync/mtp-edge540-device
    /usr/bin/systemctl start mount-edge540.service
    /usr/bin/systemctl start sync-edge540.service
elif [[ $vendor_id == "05c6" && $product_id == "9039" ]]; then
    echo $kernel_name > /var/run/fit-sync/mtp-bolt-kname
    echo "$busnum,$devnum" > /var/run/fit-sync/mtp-bolt-device
    /usr/bin/systemctl start mount-bolt.service
    /usr/bin/systemctl start sync-bolt.service
else
    echo "$script_name: not a known device: $device_path, ignoring"
fi
