#!/bin/bash

# This file is run (from startup.sh) on the first reboot after an install (or update if required).

# Its function is to install the sdrplay api and then compile the sdrplay-related applications.
# The sdrplay install needs a reboot after other installation actions before it will run successfully

cd /home/pi/rpidatv/src/meteorview

# Download api if required
if [ ! -f  SDRplay_RSP_API-ARM-3.09.1.run ]; then
  wget "https://sdrplay.com/download/hardware-api-linux-32bit-unsupported/?wpdmdl=2341&refresh=6a87239f68e1f1787241375" -O SDRplay_RSP_API-ARM-3.09.1.run
  # was wget https://www.sdrplay.com/software/SDRplay_RSP_API-ARM-3.09.1.run
fi
chmod +x SDRplay_RSP_API-ARM-3.09.1.run

./sdrplay_api_install.exp

sudo systemctl disable sdrplay  # service is started only when required

# Compile meteorview
make
cp meteorview ../../bin/
cd /home/pi

# Compile sdrplayview
cd /home/pi/rpidatv/src/sdrplayview
make
cp sdrplayview ../../bin

cd /home/pi

# Delete the trigger file
rm /home/pi/rpidatv/.post-install_actions

exit