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

device_tag=

if [[ $vendor_id == "091e" ]]; then     # Garmin devices
    if [[ $product_id == "4c29" ]]; then
        device_tag=fr945
    elif [[ $product_id == "4fdd" ]]; then
        device_tag=edge540
    elif [[ $product_id == "4fde" ]]; then
        device_tag=edge840
    elif [[ $product_id == "50db" ]]; then
        device_tag=fr965
    fi
elif [[ $vendor_id == "05c6" ]]; then   # Wahoo devices
     if [[ $product_id == "9039" ]]; then
         device_tag=bolt
     fi
fi

if [[ -z $device_tag ]]; then
   echo "$script_name: not a known device: $device_path, ignoring"
else
    echo $kernel_name > /var/run/fit-sync/mtp-$device_tag-kname
    echo "$busnum,$devnum" > /var/run/fit-sync/mtp-$device_tag-device
    /usr/bin/systemctl start mount-$device_tag.service
    /usr/bin/systemctl start sync-$device_tag.service
fi
