When a Garmin device is plugged into a USB port, PiFitSync is supposed to
detect it and copy the FIT files to the ~/FitSync folder.  The `fit-sync-usb`
is used to copy the files, but detecting the USB insertion, mounting the drive
and starting the utility is surprisingly complex and involves a lot of files
and services.

The sections below document this process, mostly for the authors benefit when
he will have to look at this code again in a few years time.  If you know a
simpler way, the author of this package would like to know it.

# Synching Garmin MTP devices

Most newer Garmin devices appear as MTP devices and the steps below are used
to detect the device insertion into one of the Raspberry PI's USB ports and
start `fit-sync-usb`

## UDEV Rules

The UDEV rules relevant to MTP device mounting/unmounting in
`99-fit-sync.rules` are:

```
SUBSYSTEM=="usb", ACTION=="add", ATTR{interface}=="MTP", RUN+="/usr/local/libexec/fit-sync/on-mtp-added %p"
SUBSYSTEM=="usb", ACTION=="remove", RUN+="/usr/local/libexec/fit-sync/on-mtp-removed %k"
```

And they launch two scripts, `on-mtp-added` when a device is added and
`on-mtp-removed` when the device is removed.

### UDEV Rule for adding the device

An udev rule is used to detect when a MTP device from the "usb" subsusystem is
added, to call `on-mtp-added`:

```
SUBSYSTEM=="usb", ACTION=="add", ATTR{interface}=="MTP", RUN+="/usr/local/libexec/on-mtp-added %p"
```

`on-mtp-added` receives the device path (`sys` has to be prepended to it), it
will than:

* determine the parent device (this is just the parent folder in the tree).
  The UDEV rule matches against the MTP interface, but the actual device is
  the parent -- this parent device is simply the parent folder.
  
* determine the vendor id and product id from the `idVendor` and `idProduct`
  files in the parent device folder, and check them against the desired device
  for a FR945 (script will exit if it does not)
  
* determine the bus and device numbers (contents of the `busnum` and `devnum`
  files) -- this information is needed by the `jmtpfs` utility to mount the
  correct MTP device.  This information is saved into
  `/var/run/fit-sync/mtp-device`, and will be used by the device mount script,
  `mount-fr945`

* determine the kernel device name -- this is the folder name of the parent
  device.  This will be saved in `/var/run/fit-sync/mtp-kname` and will be
  used by `on-mtp-removed` script to determine if our device was removed and
  the drive needs to be unmounted.

* start the "mount-fr945.service" using `systemctl start mount-fr945.service`
  -- the `jmtpfs` utility needs to keep running while the drive is mounted and
  udev scripts neeed to run in a short amount of time so we cannot launch
  servers directly from here.
  
* start the "sync-fr945.service" using `systemctl start sync-fr945.service` --
  this will copy the FIT files into ~/FitSync to be shared over the network.
  This service has a dependency listed on the `mount-fr945.service` so,
  systemd will make sure the start is delayed until the file system is
  mounted.

### UDEV Rules for removing the device

There is no remove action against the MTP device directly, so we add a rule
against any USB device and call `on-mtp-removed` script to figure out the
rest:

```
SUBSYSTEM=="usb", ACTION=="remove", RUN+="/usr/local/libexec/on-mtp-removed %k"
```

`on-mtp-removed` receives the kernel device name for the device that was
removed, it will than:

* verify that the kernel name is the same as the one saved in
  `/var/run/fit-sync/mtp-kname`

* if it is, stop the "mount-fr945.service" using `systemctl stop
  mount-fr945.service`

### The MTP Drive Mounting/Unmounting Service

Mounting (and unmounting) the MTP device is done using a systemd service.
This is required, because the `jmtpfs` utility needs to be running while the
MTP device is mounted, so it is really a "service".  The contents of the
service file are as follows:

```
; -*- mode: conf -*-
[Unit]
Description=Mount the fr945 MTP device

[Service]
Type=forking
ExecStart=@@BINDIR@@/mount-fr945
ExecStop=/usr/bin/fusermount -u /media/fr945
Restart=no
WorkingDirectory=/
User=root
Group=root
```

In particular, the service is setup as forking (this ensures the system is
considered started only after the file system is mounted).  Also, rather than
calling `jmtpfs` directly, the `mount-fr945` shell script is used.  This
script will read the device from `/var/run/fit-sync/mtp-device` and ensures
that the correct MTP device is mounted.

The service is set up to call `fusermount` to unmount the drive when the
service is deactivated.

### The sync service

The `fit-sync-usb` utility is used to copy FIT files from the FR945 to the
`~/FitSync` folder, which is share over the network.  This is done using
another systemd service, `sync-fr945.service`.

```
; -*- mode: conf -*-

[Unit]
Description=Download FIT files from a FR945
After=fit-sync-setup.service
Requires=mount-fr945.service
After=mount-fr945.service

[Service]
Type=forking
PIDFile=/run/fit-sync/fit-sync-fr945.pid
ExecStart=@@BINDIR@@/fit-sync-usb -p /run/fit-sync/fit-sync-fr945.pid -d /media/fr945/Primary/GARMIN
Restart=no
WorkingDirectory=/media/fr945/Primary/GARMIN
User=@@USER@@
Group=@@GROUP@@
```

# Synching GARMIN USB Drive devices

Previous generation Garmin devices show up as USB drives when plugged in and
the steps below are used to mount these drives and start up `fit-sync-usb`

The whole process is controlled by UDEV rules which trigger when a new
"sd[a-z]" device shows up.  `blkid` is used to get the drive label and, if the
label is "GARMIN", the device is mounted using `/bin/mount` and the
`fit-sync-usb.service` is started.

The rules also arrange for an unmount command to be used when the device is
removed.

```
KERNEL=="sd[a-z]", GOTO="start_automount"
KERNEL=="sd[a-z][0-9]", GOTO="start_automount"
GOTO="end_automount"

LABEL="start_automount"
IMPORT{program}="/sbin/blkid -o udev -p /dev/%k"
ENV{ID_FS_LABEL}=="GARMIN", GOTO="garmin_automount"
GOTO="end_automount"

### Mount a Garmin USB device on /media/garmin and start synching files

LABEL="garmin_automount"

ACTION=="add", RUN+="/bin/mount -t vfat -osync,noexec,noatime,nodiratime,uid=ubuntu /dev/%k /media/garmin"
ACTION=="add", RUN+="/bin/systemctl start fit-sync-usb.service"
ACTION=="add", RUN+="/bin/systemctl start fit-sync-epo.service"

# Run umount when the device is removed (this will only useful if the device
# is prematurely removed.)
ACTION=="remove", RUN+="/bin/umount -l /media/garmin"

GOTO="end_automount"

LABEL="end_automount"
```

The `fit-sync-usb.service` simply launches `fit-sync-usb` -- this is needed
since programs run as part of an udev rule must complete quickly and udev ends
up killing the process otherwise.

There is also a `fit-sync-epo.service` which runs a python script to download
EPO data to the device -- this is only needed if you don't have the device
paired up with your smart phone, which already downloads that data.

# Random notes

Connect the FR945 and check `/var/log/syslog` for the device name, should be
something like,
`/sys/devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.2/1-1.2:1.0`,
check with `ls` that there are no subdirectories -- you want the deepest path.
Run the following command to print out the attributes for each level of the
tree.

    udevadm info --attribute-walk --path=$DEVPATH

Depending on how the UDEV rules are setup, they may match multiple times in
the tree (e.g. if matching only on the "idVendor" and "idProduct" attributes).
The command below can be used to determine what will be run for a specific
device -- note that this can be misleading, since you have to specify the
device, while the udev rules will run on all devices and the rule you wrote
had multiple matches...

    udevadm test $DEVPATH
    
