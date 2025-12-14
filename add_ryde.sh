#!/bin/bash

# Add Ryde to Portsdown 4 build

cd /home/pi

# Check which source needs to be loaded
GIT_SRC="BritishAmateurTelevisionClub"
GIT_SRC_FILE=".ryde_gitsrc"

if [ "$1" == "-d" ]; then
  GIT_SRC="davecrump";
  echo
  echo "-------------------------------------------------------------"
  echo "----- Installing Ryde development version from davecrump-----"
  echo "-------------------------------------------------------------"
  echo
elif [ "$1" == "-u" -a ! -z "$2" ]; then
  GIT_SRC="$2"
  echo
  echo "WARNING: Installing ${GIT_SRC} development version, press enter to continue or 'q' to quit."
  read -n1 -r -s key;
  if [[ $key == q ]]; then
    exit 1;
  fi
  echo "ok!";
else
  echo
  echo "-------------------------------------------"
  echo "----- Installing BATC Production Ryde -----"
  echo "-------------------------------------------"
  echo
fi

# Amend the sources.list to legacy
sudo bash -c 'echo -e "deb http://legacy.raspbian.org/raspbian/ buster main contrib non-free rpi" > /etc/apt/sources.list' 

# Update the package manager
echo
echo "------------------------------------"
echo "----- Updating Package Manager -----"
echo "------------------------------------"
echo
sudo dpkg --configure -a
sudo apt-get update --allow-releaseinfo-change

# Uninstall the apt-listchanges package to allow silent install of ca certificates (201704030)
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

# Upgrade the distribution
echo
echo "-----------------------------------"
echo "----- Performing dist-upgrade -----"
echo "-----------------------------------"
echo
sudo apt-get -y dist-upgrade

echo
echo "Checking for EEPROM Update"
echo

sudo rpi-eeprom-update -a                            # Update will be installed on reboot if required

# Install the packages that we need

echo
echo "------------------------------------------------------"
echo "----- Installing Extra Packages Required by Ryde -----"
echo "------------------------------------------------------"
echo

sudo apt-get -y install ir-keytable
sudo apt-get -y install python3-dev
sudo apt-get -y install python3-pip
sudo apt-get -y install python3-yaml
sudo apt-get -y install python3-pygame
sudo apt-get -y install python3-vlc
sudo apt-get -y install python3-evdev
sudo apt-get -y install python3-pil
sudo apt-get -y install python3-gpiozero
sudo apt-get -y install python3-urwid             # for Ryde Utils
sudo apt-get -y install python3-librtmp           # for Stream RX
sudo apt-get -y install vlc-plugin-base           # for Stream RX

pip3 install pyftdi==0.53.1                       # for Ryde Utils

# Download the previously selected version of Ryde Build
echo
echo "------------------------------------------"
echo "----- Installing Ryde Build Utilities-----"
echo "------------------------------------------"
echo

cd /home/pi
rm master.zip
wget https://github.com/${GIT_SRC}/ryde-build/archive/master.zip
unzip -o master.zip
mv ryde-build-master ryde-build
rm master.zip

# Build the LongMynd version packaged with ryde-build
#echo
#echo "--------------------------------------------"
#echo "----- Installing the LongMynd Receiver -----"
#echo "--------------------------------------------"
#echo

#cd /home/pi
#cp -r ryde-build/longmynd/ longmynd/
#cd longmynd
#make

# Set up the udev rules for USB
#sudo cp minitiouner.rules /etc/udev/rules.d/
#cd /home/pi

# Download the eclispe version of pyDispmanx
echo
echo "---------------------------------"
echo "----- Installing pyDispmanx -----"
echo "---------------------------------"
echo

cd /home/pi
wget https://github.com/eclispe/pyDispmanx/archive/master.zip
unzip -o master.zip
mv pyDispmanx-master pydispmanx
rm master.zip
cd pydispmanx
python3 setup.py build_ext --inplace

cd /home/pi

# Download the previously selected version of Rydeplayer
echo
echo "---------------------------------"
echo "----- Installing Rydeplayer -----"
echo "---------------------------------"
echo

wget https://github.com/${GIT_SRC}/rydeplayer/archive/master.zip
unzip -o master.zip
mv rydeplayer-master ryde
rm master.zip

cp /home/pi/pydispmanx/pydispmanx.cpython-37m-arm-linux-gnueabihf.so \
   /home/pi/ryde/pydispmanx.cpython-37m-arm-linux-gnueabihf.so

cd /home/pi


# Download the latest remote control definitions and images
echo
echo "----------------------------------------------"
echo "----- Downloading Remote Control Configs -----"
echo "----------------------------------------------"
echo
git clone -b definitions https://github.com/${GIT_SRC}/RydeHandsets.git RydeHandsets/definitions
git clone -b images https://github.com/${GIT_SRC}/RydeHandsets.git RydeHandsets/images

# Set up the Config for this build
cp /home/pi/ryde-build/config.yaml /home/pi/ryde/config.yaml

# Set up the operating system for Ryde
echo
echo "----------------------------------------------------"
echo "----- Setting up the Operating System for Ryde -----"
echo "----------------------------------------------------"
echo

# Set auto login to command line.
#sudo raspi-config nonint do_boot_behaviour B2

# Enable IR Control
# uncomment line 51 of /boot/config.txt dtoverlay=gpio-ir,gpio_pin=17
sudo sed -i '/#dtoverlay=gpio-ir,gpio_pin=17/c\dtoverlay=gpio-ir,gpio_pin=17' /boot/config.txt  >/dev/null 2>/dev/null

# Increase GPU memory so that it copes with 4k displays
sudo bash -c 'echo " " >> /boot/config.txt '
sudo bash -c 'echo "# Increase GPU memory for 4k displays" >> /boot/config.txt '
sudo bash -c 'echo "gpu_mem=128" >> /boot/config.txt '
sudo bash -c 'echo " " >> /boot/config.txt '

# Set the Composite Video Aspect Ratio to 4:3
#sudo bash -c 'echo -e "\n# Set the Composite Video Aspect Ratio. 1=4:3, 3=16:9" >> /boot/config.txt'
#sudo bash -c 'echo -e "sdtv_aspect=1\n" >> //boot/config.txt'

# Reduce the dhcp client timeout to speed off-network startup
#sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
#sudo bash -c 'echo -e "timeout 5\n" >> /etc/dhcpcd.conf'

# Modify .bashrc for hardware shutdown, set RPi Jack audio volume and autostart
#if grep -q Ryde /home/pi/.bashrc; then  # Second Install over a previous install
#  echo
#  echo "*********** Warning, you have installed Ryde before ************"
#  echo "****** Results are unpredictable after multiple installs *******"
#  echo "*********** Please build a new card and start again ************"
#  echo
#else  # First install
#  echo  >> ~/.bashrc
#  echo "# Autostart Ryde on Boot" >> ~/.bashrc
#  echo if test -z \"\$SSH_CLIENT\" >> ~/.bashrc 
#  echo then >> ~/.bashrc
#  echo "  # If pi-sdn is not running, check if it is required to run" >> ~/.bashrc
#  echo "  ps -cax | grep 'pi-sdn' >/dev/null 2>/dev/null" >> ~/.bashrc
#  echo "  RESULT=\"\$?\"" >> ~/.bashrc
#  echo "  if [ \"\$RESULT\" -ne 0 ]; then" >> ~/.bashrc
#  echo "    if [ -f /home/pi/.pi-sdn ]; then" >> ~/.bashrc
#  echo "      . /home/pi/.pi-sdn" >> ~/.bashrc
#  echo "    fi" >> ~/.bashrc
#  echo "  fi" >> ~/.bashrc

#  echo "  # Set RPi Audio Jack volume" >> ~/.bashrc
#  echo "  amixer set Headphone 0db >/dev/null 2>/dev/null" >> ~/.bashrc
#  echo  >> ~/.bashrc

#  echo "  # Start Ryde" >> ~/.bashrc
#  echo "  /home/pi/ryde-build/rx.sh" >> ~/.bashrc
#  echo fi >> ~/.bashrc
#  echo  >> ~/.bashrc
#fi


#echo
#echo "-----------------------------------------"
#echo "----- Building the Interim DVB-T RX -----"
#echo "-----------------------------------------"
#echo

#sudo rm -rf /home/pi/dvbt/ >/dev/null 2>/dev/null
#cp -r /home/pi/ryde-build/configs/dvbt /home/pi/dvbt
#cd /home/pi/dvbt
#make
#cd /home/pi

echo
echo "-------------------------------------"
echo "----- Installing the Ryde Utils -----"
echo "-------------------------------------"
echo

wget https://github.com/eclispe/ryde-utils/archive/master.zip
unzip -o master.zip
mv ryde-utils-master ryde-utils
rm master.zip

echo
echo "--------------------------------------"
echo "----- Configure the Menu Aliases -----"
echo "--------------------------------------"
echo

# Install the menu aliases
echo "alias ryde='/home/pi/ryde-build/rx.sh'" >> /home/pi/.bash_aliases
echo "alias dryde='/home/pi/ryde-build/debug_rx.sh'" >> /home/pi/.bash_aliases
echo "alias menuryde='/home/pi/ryde-build/menu.sh'"  >> /home/pi/.bash_aliases
echo "alias stopryde='/home/pi/ryde-build/stop.sh'"  >> /home/pi/.bash_aliases

# Synchronise the audio aoutput with Portsdown Longmynd
/home/pi/rpidatv/scripts/set_ryde_audio.sh

# Record Version Number
cd /home/pi/ryde-build/
cp latest_version.txt installed_version.txt
cd /home/pi

# Reboot
echo
echo "--------------------------------"
echo "----- Complete.  Rebooting -----"
echo "--------------------------------"
echo

sudo reboot now
exit


