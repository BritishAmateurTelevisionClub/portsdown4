#!/bin/sh

# muntjac_serial.sh
# Takes 200ms to return the Muntjac serial on STDOUT

# exit if Muntjac not detected
ls -l /dev | grep -q ttyMJ0
if [ $? != 0 ]; then
  exit 
fi

# As an independent process, delay, set up the serial port and then send ###
(sleep 0.1;stty -F "/dev/ttyMJ0" "9600";echo "###" > "/dev/ttyMJ0") &

# Read the Muntjac device info and then kill cat
timeout 0.2 cat /dev/ttyMJ0 

exit
