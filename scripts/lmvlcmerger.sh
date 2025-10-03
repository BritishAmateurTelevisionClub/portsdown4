#!/bin/bash

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
RCONFIGFILE="/home/pi/rpidatv/scripts/longmynd_config.txt"
MCONFIGFILE="/home/pi/rpidatv/scripts/merger_config.txt"

############ FUNCTION TO READ CONFIG FILE #############################

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}

########################################################################

cd /home/pi

# Read from receiver and merger config files
FREQ_KHZ=$(get_config_var freq $MCONFIGFILE)
INPUT_SEL=$(get_config_var socket $MCONFIGFILE)
LNBVOLTS=$(get_config_var lnbvolts $MCONFIGFILE)

AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)
VLCVOLUME=$(get_config_var vlcvolume $PCONFIGFILE)

TIMEOUT_CMD="-r 5000 "
SYMBOLRATEK=2000

# Send audio to the correct port
if [ "$AUDIO_OUT" == "rpi" ]; then
  # Check for latest Buster update
  aplay -l | grep -q 'bcm2835 Headphones'
  if [ $? == 0 ]; then
    AUDIO_DEVICE="hw:CARD=Headphones,DEV=0"
  else
    AUDIO_DEVICE="hw:CARD=ALSA,DEV=0"
  fi
else
  AUDIO_DEVICE="hw:CARD=Device,DEV=0"
fi
if [ "$AUDIO_OUT" == "hdmi" ]; then
  # Overide for HDMI
  AUDIO_DEVICE="hw:CARD=b1,DEV=0"
fi

# Select the correct tuner input
INPUT_CMD=" "
if [ "$INPUT_SEL" == "b" ]; then
  INPUT_CMD="-w"
fi

# Set the LNB Volts
VOLTS_CMD=" "
if [ "$LNBVOLTS" == "h" ]; then
  VOLTS_CMD="-p h"
fi
if [ "$LNBVOLTS" == "v" ]; then
  VOLTS_CMD="-p v"
fi

# Error check volume
if [ "$VLCVOLUME" -lt 0 ]; then
  VLCVOLUME=0
fi
if [ "$VLCVOLUME" -gt 512 ]; then
  VLCVOLUME=512
fi

# Create dummy marquee overlay file
echo " " >/home/pi/tmp/vlc_overlay.txt

sudo killall longmynd >/dev/null 2>/dev/null
sudo killall vlc >/dev/null 2>/dev/null
sudo killall nc >/dev/null 2>/dev/null

# Play a very short dummy file if this is a first start for VLC since boot
# This makes sure the RX works on first selection after boot
if [[ ! -f /home/pi/tmp/vlcprimed ]]; then
  cvlc -I rc --rc-host 127.0.0.1:1111 -f --codec ffmpeg --video-title-timeout=100 \
    --width 800 --height 480 \
    --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
    --gain 3 --alsa-audio-device $AUDIO_DEVICE \
    /home/pi/rpidatv/video/blank.ts vlc:quit >/dev/null 2>/dev/null &
  sleep 1
  touch /home/pi/tmp/vlcprimed
  echo shutdown | nc 127.0.0.1 1111
fi


# Pipe the status output from UDP broadcast to the status fifo
nc -k -l -u 9003 > longmynd_status_fifo &

# Set the VLC UDP Control
UDPIP=127.0.0.1
UDPPORT=1234

# Start LongMynd
/home/pi/longmynd/longmynd -i 127.255.255.255 9002 -I 127.255.255.255 9003 \
  $VOLTS_CMD $TIMEOUT_CMD $INPUT_CMD $FREQ_KHZ $SYMBOLRATEK >/dev/null 2>/dev/null &

# Start VLC
cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
  --width 800 --height 480 \
  --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
  --gain 3 --alsa-audio-device $AUDIO_DEVICE \
  udp://@:9002 >/dev/null 2>/dev/null &

sleep 1

# Set the start-up volume
printf "volume "$VLCVOLUME"\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

# Stop VLC playing and move it to the next track.  This is required for PicoTuner
# as it sends the TS data in 64 byte bursts, not 510 byte bursts like MiniTiouner
# The -i 1 parameter to put a second between commands is required to ensure reset at low SRs
nc -i 1 127.0.0.1 1111 >/dev/null 2>/dev/null << EOF
stop
next
logout
EOF


exit



