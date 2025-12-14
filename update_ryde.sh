#!/bin/bash

# Updates an existing Ryde installation on Portsdown

echo
echo "----------------------------------------"
echo "----- Updating Ryde Receiver System-----"
echo "----------------------------------------"
echo

# Stop the receiver to allow the update
sudo killall python3 >/dev/null 2>/dev/null
sleep 0.3
if pgrep -x "python3" >/dev/null
then
  sudo killall -9 python3 >/dev/null 2>/dev/null
fi

## Check which update to load
GIT_SRC_FILE=".ryde_gitsrc"
if [ -e ${GIT_SRC_FILE} ]; then
  GIT_SRC=$(</home/pi/${GIT_SRC_FILE})
else
  GIT_SRC="BritishAmateurTelevisionClub"
fi

## If previous version was Dev (davecrump), load production by default
if [ "$GIT_SRC" == "davecrump" ]; then
  GIT_SRC="BritishAmateurTelevisionClub"
fi

if [ "$1" == "-d" ]; then
  echo "Overriding to update to latest development version"
  GIT_SRC="davecrump"
fi

if [ "$GIT_SRC" == "BritishAmateurTelevisionClub" ]; then
  echo "Updating to the latest Production Ryde build";
elif [ "$GIT_SRC" == "davecrump" ]; then
  echo "Updating to the latest development Ryde build";
else
  echo "Updating to the latest ${GIT_SRC} development Ryde build";
fi

echo
echo "-----------------------------------------"
echo "----- Noting Previous Configuration -----"
echo "-----------------------------------------"
echo

cd /home/pi

PATHUBACKUP="/home/pi/user_backups"
mkdir "$PATHUBACKUP" >/dev/null 2>/dev/null  

# Note previous version number
cp -f -r /home/pi/ryde-build/installed_version.txt "$PATHUBACKUP"/prev_installed_version.txt

# Make a safe copy of the Config file in "$PATHUBACKUP" to restore at the end

cp -f -r /home/pi/ryde/config.yaml "$PATHUBACKUP"/config.yaml >/dev/null 2>/dev/null

# Capture the RC protocol in the rx.sh file:
cp -f -r /home/pi/ryde-build/rx.sh "$PATHUBACKUP"/rx.sh

# And the dvb-t config
cp -f -r /home/pi/dvbt/dvb-t_config.txt "$PATHUBACKUP"/dvb-t_config.txt >/dev/null 2>/dev/null

# Check the user's audio output to re-apply it after the update.  HDMI is default.
AUDIO_JACK=HDMI
grep -q "^        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Headphones,DEV=0 '" \
  /home/pi/ryde/rydeplayer/player.py
if [ $? -eq 0 ]; then  #  RPi Jack currently selected
  AUDIO_JACK="RPi Jack"
else
  grep -q "^        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Device,DEV=0 '" \
    /home/pi/ryde/rydeplayer/player.py
  if [ $? -eq 0 ]; then  #  USB Dongle currently selected
    AUDIO_JACK=USB
  fi
fi

# Amend the sources.list to legacy
sudo bash -c 'echo -e "deb http://legacy.raspbian.org/raspbian/ buster main contrib non-free rpi" > /etc/apt/sources.list' 

echo
echo "-------------------------------------------------"
echo "----- Updating the System Software Packages -----"
echo "-------------------------------------------------"
echo

sudo dpkg --configure -a                          # Make sure that all the packages are properly configured
sudo apt-get clean                                # Clean up the old archived packages
sudo apt-get update --allow-releaseinfo-change    # Update the package list
sudo apt-get -y dist-upgrade                      # Upgrade all the installed packages to their latest version

echo
echo "Checking for EEPROM Update"
echo

sudo rpi-eeprom-update -a                            # Update will be installed on reboot if required

# --------- Overwrite and compile all the software components -----

# Download the previously selected version of Ryde Build
echo
echo "----------------------------------------"
echo "----- Updating Ryde Build Utilities-----"
echo "----------------------------------------"
echo
rm -rf /home/pi/ryde-build
wget https://github.com/${GIT_SRC}/ryde-build/archive/master.zip
unzip -o master.zip
mv ryde-build-master ryde-build
rm master.zip
cd /home/pi

# Build the LongMynd version packaged with ryde-build
#echo
#echo "------------------------------------------"
#echo "----- Updating the LongMynd Receiver -----"
#echo "------------------------------------------"
#echo
#rm -rf /home/pi/longmynd
#cp -r ryde-build/longmynd/ longmynd/
#cd longmynd
#make

# Download and compile pyDispmanx
echo
echo "--------------------------------"
echo "----- Updating pyDispmanx -----"
echo "--------------------------------"
echo

wget https://github.com/eclispe/pyDispmanx/archive/master.zip
unzip -o master.zip
rm -rf pydispmanx
mv pyDispmanx-master pydispmanx
rm master.zip
cd pydispmanx
python3 setup.py build_ext --inplace

cd /home/pi

# Download the previously selected version of Ryde Player
echo
echo "--------------------------------"
echo "----- Updating Ryde Player -----"
echo "--------------------------------"
echo
rm -rf /home/pi/ryde
wget https://github.com/${GIT_SRC}/rydeplayer/archive/master.zip
unzip -o master.zip
mv rydeplayer-master ryde
rm master.zip
cd /home/pi/ryde

cp /home/pi/pydispmanx/pydispmanx.cpython-37m-arm-linux-gnueabihf.so pydispmanx.cpython-37m-arm-linux-gnueabihf.so

cd /home/pi

# Download and overwrite the latest remote control definitions and images
echo
echo "----------------------------------------------"
echo "----- Downloading Remote Control Configs -----"
echo "----------------------------------------------"
echo

rm -rf /home/pi/RydeHandsets/definitions
rm -rf /home/pi/RydeHandsets/images

git clone -b definitions https://github.com/${GIT_SRC}/RydeHandsets.git RydeHandsets/definitions
git clone -b images https://github.com/${GIT_SRC}/RydeHandsets.git RydeHandsets/images

cd /home/pi

echo
echo "-----------------------------------"
echo "----- Updating the Ryde Utils -----"
echo "-----------------------------------"
echo

wget https://github.com/eclispe/ryde-utils/archive/master.zip
unzip -o master.zip
rm -rf ryde-utils
mv ryde-utils-master ryde-utils
rm master.zip

echo
echo "---------------------------------------------"
echo "----- Restoring the User's Config Files -----"
echo "---------------------------------------------"


grep -q "CombiTunerExpress" "$PATHUBACKUP"/config.yaml
if [ $? == 0 ]; then # User's config file is latest version, so simply copy back
  cp -f -r "$PATHUBACKUP"/config.yaml /home/pi/ryde/config.yaml >/dev/null 2>/dev/null

# Restore the user's original audio output (Default is HDMI)
case "$AUDIO_JACK" in
  "RPi Jack")
    sed -i "/--alsa-audio-device/c\        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Headphones,DEV=0 '" \
      /home/pi/ryde/rydeplayer/player.py
  ;;
  "USB")
    sed -i "/--alsa-audio-device/c\        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Device,DEV=0 '" \
      /home/pi/ryde/rydeplayer/player.py
  ;;
esac

# Add new alias if needed for the update
if ! grep -q "alias dryde" /home/pi/.bash_aliases; then
  echo "alias dryde='/home/pi/ryde-build/debug_rx.sh'" >> /home/pi/.bash_aliases
fi


# Record the version numbers

cp /home/pi/ryde-build/latest_version.txt /home/pi/ryde-build/installed_version.txt
cp -f -r "$PATHUBACKUP"/prev_installed_version.txt /home/pi/ryde-build/prev_installed_version.txt
rm -rf "$PATHUBACKUP"

echo
echo "-------------------------------"
echo "----- Rebooting Portsdown -----"
echo "-------------------------------"
echo

sudo shutdown -r now

exit




