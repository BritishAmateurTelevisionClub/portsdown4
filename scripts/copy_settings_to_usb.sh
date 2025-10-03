#!/bin/bash

# Version 202405291

# set -x

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
RETURN_CODE=1
MOUNT_POINT=/mnt

# Check USB drive detected

sudo bash -c ' fdisk -l | grep -q /dev/sdb1'
RETURN_CODE=$?

if [ $RETURN_CODE == 0 ]; then                       # Second drive present
  echo Drive USB or Pluto found at sdb1
  sudo bash -c ' fdisk -l | grep -q File-Stor'
  RETURN_CODE=$?
  if [ $RETURN_CODE == 0 ]; then                     # Pluto detected
    sudo bash -c ' fdisk -l | grep -q "/dev/sdb: 50 MiB"'
    RETURN_CODE=$?
    if [ $RETURN_CODE == 0 ]; then                   # Pluto on sdb1
      echo Pluto at sdb1, USB drive at sda1
      DEVICE=/dev/sda1
      sudo mount /dev/sda1 /mnt                      # So mount on sda1
    else                                               # Pluto on sda1
      echo Pluto at sda1, USB drive at sdb1
      DEVICE=/dev/sdb1
      sudo mount /dev/sdb1 /mnt                        # USB drive at sdb1, so mount it
    fi
  fi
else                                                 # Only Pluto, Single drive or no drive
  sudo bash -c ' fdisk -l | grep -q File-Stor'
  RETURN_CODE=$?
  if [ $RETURN_CODE == 0 ]; then                     # This is a Pluto, not a USB Drive
    echo USB Drive not found, but Pluto connected
    exit                                             # So do not write to it
  else
    sudo bash -c ' fdisk -l | grep -q /dev/sda1'
    RETURN_CODE=$?
    if [ $RETURN_CODE == 0 ]; then                   # USB drive at sda1, so mount it
      echo USB Drive found at sda1
      DEVICE=/dev/sda1
      sudo mount /dev/sda1 /mnt
    else
      echo USB Drive not found
      exit                                           # No USB drive, so exit
    fi
  fi
fi

echo Writing to $DEVICE

# Remove any old settings

sudo rm -rf "$MOUNT_POINT"/portsdown_settings
sudo mkdir "$MOUNT_POINT"/portsdown_settings

# portsdown_config.txt
sudo cp -f $PATHSCRIPT"/portsdown_config.txt" "$MOUNT_POINT"/portsdown_settings/portsdown_config.txt

# portsdown_presets.txt
sudo cp -f $PATHSCRIPT"/portsdown_presets.txt" "$MOUNT_POINT"/portsdown_settings/portsdown_presets.txt

# siggencal.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggencal.txt "$MOUNT_POINT"/portsdown_settings/siggencal.txt

# siggenconfig.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt "$MOUNT_POINT"/portsdown_settings/siggenconfig.txt

# rtl-fm_presets.txt
sudo cp -f $PATHSCRIPT"/rtl-fm_presets.txt" "$MOUNT_POINT"/portsdown_settings/rtl-fm_presets.txt

# portsdown_locators.txt
sudo cp -f $PATHSCRIPT"/portsdown_locators.txt" "$MOUNT_POINT"/portsdown_settings/portsdown_locators.txt

# rx_presets.txt
sudo cp -f $PATHSCRIPT"/rx_presets.txt" "$MOUNT_POINT"/portsdown_settings/rx_presets.txt

# Stream Presets
sudo cp -f $PATHSCRIPT"/stream_presets.txt" "$MOUNT_POINT"/portsdown_settings/stream_presets.txt

# Jetson Config
sudo cp -f $PATHSCRIPT"/jetson_config.txt" "$MOUNT_POINT"/portsdown_settings/jetson_config.txt

# NF Meters
sudo cp -f /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt "$MOUNT_POINT"/nf_meter_config.txt
sudo cp -f /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt "$MOUNT_POINT"/pluto_nf_meter_config.txt

# 11 files

sudo umount $DEVICE
