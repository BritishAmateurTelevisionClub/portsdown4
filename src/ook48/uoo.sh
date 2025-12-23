#! /bin/bash

# Compile LimeSDR bandview and copy to rpidatv/bin

# set -x

# Kill the current process if it is running
sudo killall ook48 >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/ook48
touch ook48.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cp ook48 /home/pi/rpidatv/bin/ook48
cd /home/pi

/home/pi/rpidatv/bin/ook48
exit
