# Sync FIT files on a Raspberry PI and update EPO data on the device

## Overview

This works as follows:

* USB charging cradle is connected to the USB port on a Raspberry PI
* When the FR920 is connected for charging, a program on the PI copies all the
  FIT files off the device onto a local folder
* Another script that downloads the EPO file (GPS quick sync data) for the
  device.
* The local folder on the PI is shared over the home network and mounted as a
  drive on the local network.
* On the laptop, I FIT files show up on the network drive and can be imported.

There is also support for downloading using an USB ANT Dongle, but the service
for that is not installed.

## Setup

### Install samba and setup network shares for PI user

  To install samba run the following:

    sudo apt-get install samba samba-common-bin smbclient

  Setup a SAMBA password for the `pi` user, (can be different than the login
  password, but I like to keep them the same)

    sudo smbpasswd -a pi

  To add exported shares for the Pi user, first create the directories to be
  shared.  FitSync is where FIT files will be downloaded, it is read-only via
  samba.  Dropbox is a folder where data can be written to, usefull to put
  files in the account.

    mkdir ~/FitSync
    mkdir ~/Dropbox

  Edit `/etc/samba/smb.conf` to enable user shares with password authenticated
  users, add the following to the end:

    [FitFiles]
            comment=FIT files downloaded from devices
            valid users = pi
            read only = yes
            browseable = yes
            path=/home/pi/FitSync

    [PiDropbox]
            comment=Dropbox for the pi user
            valid users = pi
            read only = no
            browseable = yes
            path=/home/pi/Dropbox

### Setup FitSync

#### Build and install libusb and other prerequisites

  `libudev`, `git`, `python3` and `rsync` will need to be installed first:
  
    sudo apt-get install libudev-dev git rsync python3

  Copy (via the Dropbox share) libusb-1.0.18.tar.bz2 onto the RPI, than run:

    mkdir ~/pkg
    mv ~/Dropbox/libusb-1.0.18.tar.bz2 ~/pkg/
    cd ~/pkg
    tar xjf libusb-1.0.18.tar.bz2
    cd libusb-1.0.18
    ./configure
    make
    sudo make install
    
#### Build and install FitSync

  To put the FitSync sources on the PI, make an empty repository first:
  
    mkdir ~/pkg/FitSync
    cd ~/pkg/FitSync
    git init
    
  From the other laptop, push the source to the rpi (assuming remote is
  already set up).  You will need to build and install it:
  
    git checkout master
    cd src
    make
    sudo make install

#### Setup other directories

Create the mount point directories for the devices:

    sudo mkdir -p /media/garmin
    sudo mkdir -p /media/fly6

Create symlinks so we can access device locations them from the PI home
directory:
    
    ln -s /media/fly6 ~/fly6
    ln -s /media/garmin/GARMIN/ACTIVITY ~/fr920-activities
    ln -s /media/garmin/GARMIN/NEWFILES ~/fr920-newfiles

Create symling of the device name to the devices activities (so we can browse
directly to "X:\0-ByName\fr920" and won't have to remember the device ID:
    
    mkdir ~/FitSync/0-ByName
    ### REPLACE with the real device serial number:
    ln -s ~/FitSync/3916163708/Activities ~/FitSync/0-ByName/fr920

