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

echo Reading from $DEVICE

if [ ! -d "$MOUNT_POINT"/portsdown_settings/ ]; then
  echo "$DIRECTORY does not exist."
fi

# portsdown_config.txt
cp -f $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_config.txt $PATHSCRIPT"/portsdown_config.txt"

# portsdown_presets.txt
cp -f $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_presets.txt $PATHSCRIPT"/portsdown_presets.txt"

# siggencal.txt
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt

# siggenconfig.txt
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt

# rtl-fm_presets.txt
cp -f $PATHSCRIPT"/rtl-fm_presets.txt" $PATHSCRIPT"/rtl-fm_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/rtl-fm_presets.txt $PATHSCRIPT"/rtl-fm_presets.txt"

# portsdown_locators.txt
cp -f $PATHSCRIPT"/portsdown_locators.txt" $PATHSCRIPT"/portsdown_locators.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_locators.txt $PATHSCRIPT"/portsdown_locators.txt"

# rx_presets.txt
cp -f $PATHSCRIPT"/rx_presets.txt" $PATHSCRIPT"/rx_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/rx_presets.txt $PATHSCRIPT"/rx_presets.txt"

# Stream Presets
cp -f $PATHSCRIPT"/stream_presets.txt" $PATHSCRIPT"/stream_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/stream_presets.txt $PATHSCRIPT"/stream_presets.txt"

# Jetson Config
cp -f $PATHSCRIPT"/jetson_config.txt" $PATHSCRIPT"/jetson_config.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/jetson_config.txt $PATHSCRIPT"/jetson_config.txt"

# NF Meters
cp -f /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/nf_meter_config.txt /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt

cp -f /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/pluto_nf_meter_config.txt /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt

sudo umount $DEVICE

