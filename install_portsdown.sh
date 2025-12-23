#!/bin/bash

# Portsdown 4 Install by davecrump on 20230227

# Check current user
whoami | grep -q pi
if [ $? != 0 ]; then
  echo "Install must be performed as user pi"
  exit
fi

# Check which source needs to be loaded
GIT_SRC="BritishAmateurTelevisionClub"
GIT_SRC_FILE=".portsdown_gitsrc"

if [ "$1" == "-d" ]; then
  GIT_SRC="davecrump";
  echo
  echo "---------------------------------------------------------"
  echo "----- Installing Development version of Portsdown 4-----"
  echo "---------------------------------------------------------"
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
  echo "-------------------------------------------------------------"
  echo "----- Installing BATC Production version of Portsdown 4 -----"
  echo "-------------------------------------------------------------"
fi

# Download and install the VLC apt Preferences File 202212090
cd /home/pi
wget https://github.com/${GIT_SRC}/portsdown4/raw/master/scripts/configs/vlc
sudo cp vlc /etc/apt/preferences.d/vlc

# Amend the sources.list to legacy
sudo bash -c 'echo -e "deb http://legacy.raspbian.org/raspbian/ buster main contrib non-free rpi" > /etc/apt/sources.list' 

# Update the package manager
echo
echo "------------------------------------"
echo "----- Updating Package Manager -----"
echo "------------------------------------"
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
sudo apt-get -y dist-upgrade

echo
echo "Checking for EEPROM Update"
echo

sudo rpi-eeprom-update -a                            # Update will be installed on reboot if required

# Install the packages that we need
echo
echo "-------------------------------"
echo "----- Installing Packages -----"
echo "-------------------------------"

sudo apt-get -y install git cmake libusb-1.0-0-dev libfftw3-dev libxcb-shape0 
sudo apt-get -y install wiringpi                                         # Wiring pi depracated? 
sudo apt-get -y install libx11-dev buffer libjpeg-dev indent 
sudo apt-get -y install bc usbmount libvncserver-dev
sudo apt-get -y install ttf-dejavu-core                                  # being depracated?
sudo apt-get -y install fbi netcat imagemagick omxplayer
sudo apt-get -y install libvdpau-dev libva-dev                           # For latest ffmpeg build
sudo apt-get -y install libsqlite3-dev libi2c-dev                        # 201811300 Lime
sudo apt-get -y install sshpass  # 201905090 For Jetson Nano
sudo apt-get -y install libbsd-dev # 201910100 for raspi2raspi
sudo apt-get -y install libasound2-dev sox # 201910230 for LongMynd tone and avc2ts audio
sudo apt-get -y install libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev # Required for ffmpegsrc.cpp
sudo apt-get -y install mplayer # 202004300 Used for video monitor and LongMynd (not libpng12-dev)
sudo apt-get -y install vlc  # Latest version works for Portsdown again as of 202207130

sudo apt-get -y install autoconf libtool # for fdk aac

sudo apt-get -y install libxml2 libxml2-dev bison flex libcdk5-dev                   # for libiio
sudo apt-get -y install libaio-dev libserialport-dev libxml2-dev libavahi-client-dev # for libiio

sudo apt-get -y install nginx-light                                     # For web access
sudo apt-get -y install libfcgi-dev                                     # For web control

sudo apt-get -y install libairspy-dev                                   # For Airspy Bandviewer
sudo apt-get -y install expect                                          # For unattended installs
sudo apt-get -y install uhubctl                                         # For SDRPlay USB resets
sudo apt-get -y install libssl-dev                                      # For websockets
sudo apt-get -y install libzstd-dev                                     # For libiio 202309040
sudo apt-get -y install arp-scan                                        # For List Network Devices
sudo apt-get -y install cppcheck                                        # For HamTV Merger Client

# Install WiringPi
cd /home/pi
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build debian

# Read latest WiringPi version number and install it
vMaj=`cut -d. -f1 VERSION`
vMin=`cut -d. -f2 VERSION`
mv debian-template/wiringpi_"$vMaj"."$vMin"_armhf.deb .
sudo apt install ./wiringpi_"$vMaj"."$vMin"_armhf.deb
cd /home/pi

# Install libiio for Pluto SigGen (and Langstone)
cd /home/pi
git clone https://github.com/analogdevicesinc/libiio.git
cd libiio
git reset --hard b6028fdeef888ab45f7c1dd6e4ed9480ae4b55e3  # Back to Version 0.25
cmake ./
make all
sudo make install
cd /home/pi

# Install Websockets for Meteor Beacon RX server
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
cmake ./
make all
sudo make install
sudo ldconfig
cd /home/pi


# Enable USB Storage automount in Buster
echo
echo "----------------------------------"
echo "----- Enabling USB Automount -----"
echo "----------------------------------"
cd /lib/systemd/system/
if ! grep -q PrivateMounts=no systemd-udevd.service; then
  sudo sed -i -e 's/PrivateMounts=yes/PrivateMounts=no/' systemd-udevd.service
fi

cd /home/pi

# Set auto login to command line.
sudo raspi-config nonint do_boot_behaviour B2

# set the framebuffer to 32 bit depth by disabling dtoverlay=vc4-fkms-v3d
echo
echo "----------------------------------------------"
echo "---- Setting Framebuffer to 32 bit depth -----"
echo "----------------------------------------------"

sudo sed -i "/^dtoverlay=vc4-fkms-v3d/c\#dtoverlay=vc4-fkms-v3d" /boot/config.txt

# Enable camera
echo
echo "--------------------------------------------------"
echo "---- Enabling the Pi Cam in /boot/config.txt -----"
echo "--------------------------------------------------"
sudo bash -c 'echo -e "\n##Enable Pi Camera" >> /boot/config.txt'
sudo bash -c 'echo -e "\nstart_x=1\ngpu_mem=128\n" >> /boot/config.txt'


# Reduce the dhcp client timeout to speed off-network startup
echo
echo "-------------------------------------------"
echo "---- Reducing the dhcp client timeout -----"
echo "-------------------------------------------"
sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
sudo bash -c 'echo -e "timeout 5\n" >> /etc/dhcpcd.conf'
cd /home/pi/

# Download the previously selected version of Portsdown 4
echo
echo "--------------------------------------------"
echo "----- Downloading Portsdown 4 Software -----"
echo "--------------------------------------------"
wget https://github.com/${GIT_SRC}/portsdown4/archive/master.zip

# Unzip the rpidatv software and copy to the Pi
unzip -o master.zip
mv portsdown4-master rpidatv
rm master.zip
cd /home/pi


# Install LimeSuite 22.09 as at 27 Feb 23
# Commit 9c983d872e75214403b7778122e68d920d583add
echo
echo "--------------------------------------"
echo "----- Installing LimeSuite 22.09 -----"
echo "--------------------------------------"
wget https://github.com/myriadrf/LimeSuite/archive/9c983d872e75214403b7778122e68d920d583add.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-9c983d872e75214403b7778122e68d920d583add LimeSuite
rm -rf LimeSuite-9c983d872e75214403b7778122e68d920d583add
rm master.zip

# Compile LimeSuite
cd LimeSuite/
mkdir dirbuild
cd dirbuild/
cmake ../
make
sudo make install
sudo ldconfig
cd /home/pi

# Install udev rules for LimeSuite
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/pi/LimeSuite/udev-rules/install.sh
cd /home/pi	

# Record the LimeSuite Version	
echo "9c983d8" >/home/pi/LimeSuite/commit_tag.txt

# Download the LimeSDR Mini firmware/gateware versions
echo
echo "------------------------------------------------------"
echo "----- Downloading LimeSDR Mini Firmware versions -----"
echo "------------------------------------------------------"

# Current LimeSDR Mini V1 Version from LimeSuite 22.09 
mkdir -p /home/pi/.local/share/LimeSuite/images/22.09/
wget https://downloads.myriadrf.org/project/limesuite/22.09/LimeSDR-Mini_HW_1.2_r1.30.rpd -O \
               /home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_1.2_r1.30.rpd

# DVB-S/S2 Version
mkdir -p /home/pi/.local/share/LimeSuite/images/v0.3
wget https://github.com/natsfr/LimeSDR_DVBSGateware/releases/download/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd -O \
 /home/pi/.local/share/LimeSuite/images/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd

# Current LimeSDR Mini V2 Version from LimeSuite 23.11 
wget https://downloads.myriadrf.org/project/limesuite/23.11/LimeSDR-Mini_HW_2.0_r2.2.bit -O \
               /home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_2.0_r2.2.bit

# Check that it was downloaded, if not, go to source and get it
if [[ "$?" != "0" ]]; then
  rm /home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_2.0_r2.2.bit
  wget https://github.com/myriadrf/LimeSDR-Mini-v2_GW/raw/main/LimeSDR-Mini_bitstreams/lms7_trx_impl1.bit -O \
    /home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_2.0_r2.2.bit
fi

echo
echo "--------------------------------------------------------------"
echo "----- Downloading Portsdown 4 version of avc2ts Software -----"
echo "--------------------------------------------------------------"

# Download the previously selected version of avc2ts for Portsdown 4
cd /home/pi	
wget https://github.com/${GIT_SRC}/avc2ts/archive/refs/heads/portsdown4.zip

# Unzip the avc2ts software and copy to the Pi
unzip -o portsdown4.zip
mv avc2ts-portsdown4 avc2ts
rm portsdown4.zip

# Compile rpidatv gui
echo
echo "----------------------------------"
echo "----- Compiling rpidatvtouch -----"
echo "----------------------------------"
cd /home/pi/rpidatv/src/gui
make
sudo make install

# Build avc2ts and dependencies
echo
echo "--------------------------------------------"
echo "----- Building avc2ts and dependencies -----"
echo "--------------------------------------------"

# For libmpegts
cd /home/pi/avc2ts
git clone https://github.com/F5OEO/libmpegts.git
cd libmpegts
# Overwrite updated config version files 202307170
cp /home/pi/rpidatv/scripts/configs/config.guess config.guess
cp /home/pi/rpidatv/scripts/configs/config.sub config.sub
./configure
make
cd ../

# For libfdkaac
git clone https://github.com/mstorsjo/fdk-aac.git
cd fdk-aac
# Overwrite updated config version files 202307170
cp /home/pi/rpidatv/scripts/configs/config.guess config.guess
cp /home/pi/rpidatv/scripts/configs/config.sub config.sub
./autogen.sh
./configure
make && sudo make install
sudo ldconfig
cd ../

#libyuv should be used for fast picture transformation : not yet implemented
git clone https://chromium.googlesource.com/libyuv/libyuv
cd libyuv
#should patch linux.mk with -DHAVE_JPEG on CXX and CFLAGS
#seems to be link with libjpeg9-dev
make V=1 -f linux.mk
cd ../

# Make avc2ts
cd /home/pi/avc2ts
make
cp avc2ts ../rpidatv/bin/
cd /home/pi

echo
echo "-----------------------------------------------"
echo "----- Installing RTL-SDR Drivers and Apps -----"
echo "-----------------------------------------------"
cd /home/pi
wget https://github.com/keenerd/rtl-sdr/archive/master.zip
unzip master.zip
mv rtl-sdr-master rtl-sdr
rm master.zip

# Compile and install rtl-sdr
cd rtl-sdr/ && mkdir build && cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make && sudo make install && sudo ldconfig
sudo bash -c 'echo -e "\n# for RTL-SDR:\nblacklist dvb_usb_rtl28xxu\n" >> /etc/modprobe.d/blacklist.conf'
cd /home/pi

# Download, compile and install DATV Express-server
echo
echo "------------------------------------------"
echo "----- Installing DATV Express Server -----"
echo "------------------------------------------"
cd /home/pi
wget https://github.com/G4GUO/express_server/archive/master.zip
unzip master.zip
mv express_server-master express_server
rm master.zip
cd /home/pi/express_server
make
sudo make install

cd /home/pi
# Install limesdr_toolbox
echo
echo "--------------------------------------"
echo "----- Installing LimeSDR Toolbox -----"
echo "--------------------------------------"

cd /home/pi/rpidatv/src/limesdr_toolbox

# Install sub project dvb modulation
git clone https://github.com/F5OEO/libdvbmod.git
cd libdvbmod/libdvbmod
make
cd ../DvbTsToIQ/
make
cp dvb2iq /home/pi/rpidatv/bin/
cd /home/pi/rpidatv/src/limesdr_toolbox/

make 
cp limesdr_send /home/pi/rpidatv/bin/
cp limesdr_dump /home/pi/rpidatv/bin/
cp limesdr_stopchannel /home/pi/rpidatv/bin/
cp limesdr_forward /home/pi/rpidatv/bin/
make dvb
cp limesdr_dvb /home/pi/rpidatv/bin/
cd /home/pi

# Install Muntjac
echo
echo "------------------------------"
echo "----- Installing Muntjac -----"
echo "------------------------------"

cd /home/pi/rpidatv/src/muntjac

gcc  muntjacsdr_dvb.c  dvbs2neon.S  -mfpu=neon  -lpthread  -o  muntjacsdr_dvb
gcc  mjcalib-0v1a.c  dvbs2neon.S  -mfpu=neon  -lpthread  -o mjcalib  # only used for manual calibration

cp muntjacsdr_dvb /home/pi/rpidatv/bin/             # Executable
cp E46214B063533828.mjo /home/pi/rpidatv/bin/       # LO Suppression file

cd /home/pi

echo
echo "----------------------------------"
echo "----- Installing dvb_t_stack -----"
echo "----------------------------------"
cd /home/pi/rpidatv/src/dvb_t_stack/Release
make clean
make
cp dvb_t_stack /home/pi/rpidatv/bin/dvb_t_stack

# Install the DATV Express firmware files
cd /home/pi/rpidatv/src/dvb_t_stack
sudo cp datvexpress16.ihx /lib/firmware/datvexpress/datvexpress16.ihx
sudo cp datvexpressraw16.rbf /lib/firmware/datvexpress/datvexpressraw16.rbf
cd /home/pi

# Install LongMynd
echo
echo "--------------------------------------------"
echo "----- Installing the LongMynd Receiver -----"
echo "--------------------------------------------"
cd /home/pi
cp -r /home/pi/rpidatv/src/longmynd/ /home/pi/
cd longmynd
make

# Set up the udev rules for USB
sudo cp minitiouner.rules /etc/udev/rules.d/

cd /home/pi

# Download, compile and install the executable for hardware shutdown button
echo
echo "------------------------------------------------------------"
echo "----- Installing the hardware shutdown Button software -----"
echo "------------------------------------------------------------"

git clone https://github.com/philcrump/pi-sdn /home/pi/pi-sdn-build

# Install new version that sets swapoff
cp -f /home/pi/rpidatv/src/pi-sdn/main.c /home/pi/pi-sdn-build/main.c
cd /home/pi/pi-sdn-build
make
mv pi-sdn /home/pi/
cd /home/pi


# Reduce the dhcp client timeout to speed off-network startup (201704160)
#sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
#sudo bash -c 'echo -e "\ntimeout 5\n" >> /etc/dhcpcd.conf'

echo
echo "--------------------------"
echo "----- Preparing WiFi -----"
echo "--------------------------"

sudo rm /etc/wpa_supplicant/wpa_supplicant.conf
sudo cp /home/pi/rpidatv/scripts/configs/wpa_supplicant.conf /etc/wpa_supplicant/wpa_supplicant.conf
sudo chown root /etc/wpa_supplicant/wpa_supplicant.conf

# Disable Wifi on Initial Start
cp ~/rpidatv/scripts/configs/text.wifi_off ~/.wifi_off >/dev/null 2>/dev/null

# Compile the Signal Generator
echo
echo "------------------------------------------"
echo "----- Compiling the Signal Generator -----"
echo "------------------------------------------"
cd /home/pi/rpidatv/src/siggen
make
sudo make install
cd /home/pi

# Compile adf4351
echo
echo "----------------------------------------"
echo "----- Compiling the ADF4351 driver -----"
echo "----------------------------------------"
cd /home/pi/rpidatv/src/adf4351
make
cp adf4351 ../../bin/
cd /home/pi

# Compile Band Viewer
echo
echo "---------------------------------"
echo "----- Compiling Band Viewer -----"
echo "---------------------------------"
cd /home/pi/rpidatv/src/bandview
make
cp bandview ../../bin/
# Copy the fftw wisdom file to home so that there is no start-up delay
# This file works for both BandViewer and NF Meter
cp .fftwf_wisdom /home/pi/.fftwf_wisdom
cd /home/pi

# Compile Airspy Band Viewer
echo
echo "----------------------------------------"
echo "----- Compiling Airspy Band Viewer -----"
echo "----------------------------------------"
cd /home/pi/rpidatv/src/airspyview
make
cp airspyview ../../bin/
cd /home/pi

# Compile RTL-SDR Band Viewer
echo
echo "----------------------------------------"
echo "----- Compiling RTL-SDR Band Viewer -----"
echo "----------------------------------------"
cd /home/pi/rpidatv/src/rtlsdrview
make
cp rtlsdrview ../../bin/
cd /home/pi

# Compile Pluto Band Viewer
echo
echo "---------------------------------------"
echo "----- Compiling Pluto Band Viewer -----"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/plutoview
make
cp plutoview ../../bin/
cd /home/pi

# For Metoeorview and sdrplayview
# Download sdrplay api for install after first reboot
echo
echo "---------------------------------------"
echo "----- Downloading the sdrplay api -----"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/meteorview

# Download api
wget https://www.sdrplay.com/software/SDRplay_RSP_API-ARM-3.09.1.run
chmod +x SDRplay_RSP_API-ARM-3.09.1.run

# Create file to trigger install on next reboot
touch /home/pi/rpidatv/.post-install_actions
cd /home/pi

# Compile Power Meter
echo
echo "---------------------------------"
echo "----- Compiling Power Meter -----"
echo "---------------------------------"
cd /home/pi/rpidatv/src/power_meter
make
cp power_meter ../../bin/
cd /home/pi

# Compile Lime NF Meter
echo
echo "---------------------------------------------"
echo "----- Compiling Lime Noise Figure Meter -----"
echo "---------------------------------------------"
cd /home/pi/rpidatv/src/nf_meter
make
cp nf_meter ../../bin/
cd /home/pi

# Compile Pluto NF Meter
echo
echo "----------------------------------------------"
echo "----- Compiling Pluto Noise Figure Meter -----"
echo "----------------------------------------------"
cd /home/pi/rpidatv/src/pluto_nf_meter
make
cp pluto_nf_meter ../../bin/
cd /home/pi

# Compile Sweeper
echo
echo "---------------------------------------"
echo "----- Compiling Frequency Sweeper -----"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/sweeper
make
cp sweeper ../../bin/
cd /home/pi

# Compile DMM Display
echo
echo "---------------------------------------"
echo "-------- Compiling DMM Display --------"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/dmm
make
cp dmm ../../bin/
cd /home/pi

# Compile LimeSDR Noise Meter
echo
echo "--------------------------------------"
echo "----- Compiling Lime Noise Meter -----"
echo "--------------------------------------"
cd /home/pi/rpidatv/src/noise_meter
make
cp noise_meter ../../bin/
cd /home/pi

# Compile Pluto SDR Noise Meter
echo
echo "---------------------------------------"
echo "----- Compiling Pluto Noise Meter -----"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/pluto_noise_meter
make
cp pluto_noise_meter ../../bin/
cd /home/pi

# Compile Touchscreen Listener
echo
echo "------------------------------------------"
echo "----- Compiling Touchscreen Listener -----"
echo "------------------------------------------"
cd /home/pi/rpidatv/src/rydemon
make
cp rydemon ../../bin/
cd /home/pi

# Compile client for HamTV Merger
echo
echo "---------------------------------------------"
echo "----- Compiling Client for HamTV Merger -----"
echo "---------------------------------------------"

wget https://github.com/ARISS-UK/tsmerge-client-linuxcli/archive/refs/heads/master.zip
unzip master.zip
rm master.zip
mv tsmerge-client-linuxcli-master tsmerge
cd /home/pi/tsmerge
make cppcheck && make

# Compile OOK48 Test Source
echo
echo "---------------------------------------"
echo "----- Compiling OOK48 Test Source -----"
echo "---------------------------------------"
cd /home/pi/rpidatv/src/ook48
make
cp ook48 ../../bin/
cd /home/pi

#echo
#echo "-----------------------------------------"
#echo "----- Compiling Ancilliary programs -----"
#echo "-----------------------------------------"


# Compile and install the executable for GPIO-switched transmission (201710080)
cd /home/pi/rpidatv/src/keyedtx
make
mv keyedtx /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission with touch (202003020)
cd /home/pi/rpidatv/src/keyedtxtouch
make
mv keyedtxtouch /home/pi/rpidatv/bin/
cd /home/pi


# Compile the Attenuator Driver (201801060)
cd /home/pi/rpidatv/src/atten
make
cp /home/pi/rpidatv/src/atten/set_attenuator /home/pi/rpidatv/bin/set_attenuator
cd /home/pi

# Compile the wav2lime utility (202301140)
cd /home/pi/rpidatv/src/wav2lime
gcc -o wav2lime wav2lime.c -lLimeSuite
cp /home/pi/rpidatv/src/wav2lime/wav2lime /home/pi/rpidatv/bin/wav2lime
cd /home/pi

echo
echo "------------------------------------------"
echo "----- Setting up for captured images -----"
echo "------------------------------------------"

# Amend /etc/fstab to create a tmpfs drive at ~/tmp for multiple images (201708150)
sudo sed -i '4itmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0' /etc/fstab

# Create a ~/snaps folder for captured images (201708150)
mkdir /home/pi/snaps

# Set the image index number to 0 (201708150)
echo "0" > /home/pi/snaps/snap_index.txt


# Download and compile the components for Screen Capture
wget https://github.com/AndrewFromMelbourne/raspi2png/archive/master.zip
unzip master.zip
mv raspi2png-master raspi2png
rm master.zip
cd raspi2png
make
sudo make install
cd /home/pi

echo
echo "--------------------------------------"
echo "----- Configure the Menu Aliases -----"
echo "--------------------------------------"

# Install the menu aliases
echo "alias menu='/home/pi/rpidatv/scripts/menu.sh menu'" >> /home/pi/.bash_aliases
echo "alias gui='/home/pi/rpidatv/scripts/utils/guir.sh'"  >> /home/pi/.bash_aliases
echo "alias ugui='/home/pi/rpidatv/scripts/utils/uguir.sh'"  >> /home/pi/.bash_aliases
echo "alias udvbt='/home/pi/rpidatv/scripts/utils/udvbt.sh'"  >> /home/pi/.bash_aliases
echo "alias stop='/home/pi/rpidatv/scripts/utils/stop.sh'"  >> /home/pi/.bash_aliases

# Modify .bashrc to run startup script on ssh logon
#cd /home/pi
#sed -i 's|/home/pi/Langstone/run|source /home/pi/rpidatv/scripts/startup.sh|' .bashrc

echo if test -z \"\$SSH_CLIENT\" >> ~/.bashrc 
echo then >> ~/.bashrc
echo "source /home/pi/rpidatv/scripts/startup.sh" >> ~/.bashrc
echo fi >> ~/.bashrc

#Configure the boot parameters

if !(grep lcd_rotate /boot/config.txt) then
  sudo sh -c "echo lcd_rotate=2 >> /boot/config.txt"
fi
if !(grep disable_splash /boot/config.txt) then
  sudo sh -c "echo disable_splash=1 >> /boot/config.txt"
fi
if !(grep global_cursor_default /boot/cmdline.txt) then
  sudo sed -i '1s,$, vt.global_cursor_default=0,' /boot/cmdline.txt
fi

# Configure the nginx web server
cp -r /home/pi/rpidatv/scripts/configs/webroot /home/pi/webroot
sudo cp /home/pi/rpidatv/scripts/configs/nginx.conf /etc/nginx/nginx.conf

# Create a directory for IQ files 202403250
mkdir /home/pi/iqfiles

# Record Version Number
cd /home/pi/rpidatv/scripts/
cp latest_version.txt installed_version.txt
cd /home/pi

# Save git source used
echo "${GIT_SRC}" > /home/pi/${GIT_SRC_FILE}

echo
echo "SD Card Serial:"
cat /sys/block/mmcblk0/device/cid

# Reboot
echo
echo "--------------------------------"
echo "----- Complete.  Rebooting -----"
echo "--------------------------------"
sleep 1

sudo reboot now
exit


