#!/bin/bash

# set -x # Uncomment for testing

# Version 202108160

############# SET GLOBAL VARIABLES ####################

PATHRPI="/home/pi/rpidatv/bin"
PATHSCRIPT="/home/pi/rpidatv/scripts"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
JCONFIGFILE="/home/pi/rpidatv/scripts/jetson_config.txt"
PATH_LIME_CAL="/home/pi/rpidatv/scripts/limecalfreq.txt"

############# MAKE SURE THAT WE KNOW WHERE WE ARE ##################

cd /home/pi

############# DEFINE THE FUNCTIONS USED BELOW ##################

source /home/pi/rpidatv/scripts/a_functions.sh

# ########################## KILL ALL PROCESSES BEFORE STARTING  ################

sudo killall -9 ffmpeg >/dev/null 2>/dev/null
sudo killall rpidatv >/dev/null 2>/dev/null
sudo killall -9 avc2ts >/dev/null 2>/dev/null
# Kill netcat that night have been started for Express Server
sudo killall netcat >/dev/null 2>/dev/null
sudo killall -9 netcat >/dev/null 2>/dev/null

############ READ FROM portsdown_config.txt and Set PARAMETERS #######################

MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
TSVIDEOFILE=$(get_config_var tsvideofile $PCONFIGFILE)
PATERNFILE=$(get_config_var paternfile $PCONFIGFILE)
UDPOUTADDR=$(get_config_var udpoutaddr $PCONFIGFILE)
UDPOUTPORT=$(get_config_var udpoutport $PCONFIGFILE)
UDPINPORT=$(get_config_var udpinport $PCONFIGFILE)
CALL=$(get_config_var call $PCONFIGFILE)
CHANNEL="Portsdown_4"
FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)
STREAM_URL=$(get_config_var streamurl $PCONFIGFILE)
STREAM_KEY=$(get_config_var streamkey $PCONFIGFILE)
OUTPUT_STREAM="-f flv $STREAM_URL/$STREAM_KEY"
MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)
GAIN=$(get_config_var rfpower $PCONFIGFILE)
PIDVIDEO=$(get_config_var pidvideo $PCONFIGFILE)
PIDAUDIO=$(get_config_var pidaudio $PCONFIGFILE)
PIDPMT=$(get_config_var pidpmt $PCONFIGFILE)
PIDSTART=$(get_config_var pidstart $PCONFIGFILE)
SERVICEID=$(get_config_var serviceid $PCONFIGFILE)
LOCATOR=$(get_config_var locator $PCONFIGFILE)
FORMAT=$(get_config_var format $PCONFIGFILE)
PICAM=$(get_config_var picam $PCONFIGFILE)

PILOT=$(get_config_var pilots $PCONFIGFILE)
FRAME=$(get_config_var frames $PCONFIGFILE)

PIN_I=$(get_config_var gpio_i $PCONFIGFILE)
PIN_Q=$(get_config_var gpio_q $PCONFIGFILE)

ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
ANALOGCAMINPUT=$(get_config_var analogcaminput $PCONFIGFILE)
ANALOGCAMSTANDARD=$(get_config_var analogcamstandard $PCONFIGFILE)

AUDIO_PREF=$(get_config_var audio $PCONFIGFILE)
CAPTIONON=$(get_config_var caption $PCONFIGFILE)
OPSTD=$(get_config_var outputstandard $PCONFIGFILE)
DISPLAY=$(get_config_var display $PCONFIGFILE)

BAND_LABEL=$(get_config_var labelofband $PCONFIGFILE)
NUMBERS=$(get_config_var numbers $PCONFIGFILE)

MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)

LKVUDP=$(get_config_var lkvudp $JCONFIGFILE)
LKVPORT=$(get_config_var lkvport $JCONFIGFILE)
PLUTOIP=$(get_config_var plutoip $PCONFIGFILE)

OUTPUT_IP=""
LIMETYPE=""
REQ_UPSAMPLE=$(get_config_var upsample $PCONFIGFILE)

# If a Fushicai EasyCap, adjust the contrast to prevent white crushing
# Default is 464 (scale 0 - 1023) which crushes whites
lsusb | grep -q '1b71:3002'
if [ $? == 0 ]; then   ## Fushuicai USBTV007
  ECCONTRAST="contrast=380"
else
  ECCONTRAST=" "
fi

# Check Pi Cam orientation
# Set for avc2ts and ffmpeg modes
if [ "$PICAM" == "normal" ]; then
  H264ORIENTATION=""
else
  H264ORIENTATION="-u "
fi

let SYMBOLRATE=SYMBOLRATEK*1000

# Set the FEC
FEC=$(get_config_var fec $PCONFIGFILE)
case "$FEC" in
  "7" )
    FECNUM="7"
    FECDEN="8"
    PFEC="78"
  ;;
  "14" )
    FECNUM="1"
    FECDEN="4"
    PFEC="14"
  ;;
  "13" )
    FECNUM="1"
    FECDEN="3"
    PFEC="13"
  ;;
  "1" | "12" )
    FECNUM="1"
    FECDEN="2"
    PFEC="12"
  ;;
  "35" )
    FECNUM="3"
    FECDEN="5"
    PFEC="35"
  ;;
  "2" | "23" )
    FECNUM="2"
    FECDEN="3"
    PFEC="23"
  ;;
  "3" | "34" )
    FECNUM="3"
    FECDEN="4"
    PFEC="34"
  ;;
  "5" | "56" )
    FECNUM="5"
    FECDEN="6"
    PFEC="56"
  ;;
  "89" )
    FECNUM="8"
    FECDEN="9"
    PFEC="89"
  ;;
  "91" )
    FECNUM="9"
    FECDEN="10"
    PFEC="910"
  ;;
esac

# Calculate Bits per symbol
MODULATION=$(get_config_var modulation $PCONFIGFILE)
case "$MODULATION" in
  "DVB-S" )
    BITSPERSYMBOL="2"
    MODTYPE="DVBS"
    CONSTLN="QPSK"
  ;;
  "S2QPSK" )
    BITSPERSYMBOL="2"
    MODTYPE="DVBS2"
    CONSTLN="QPSK"
  ;;
  "8PSK" )
    BITSPERSYMBOL="3"
    MODTYPE="DVBS2"
    CONSTLN="8PSK"
  ;;
  "16APSK" )
    BITSPERSYMBOL="4"
    MODTYPE="DVBS2"
    CONSTLN="16APSK"
  ;;
  "32APSK" )
    BITSPERSYMBOL="5"
    MODTYPE="DVBS2"
    CONSTLN="32APSK"
  ;;
esac

# Look up the capture device names and parameters
detect_audio
detect_video
ANALOGCAMNAME=$VID_USB

#Adjust the PIDs for avc2ts modes

case "$MODE_INPUT" in
  "CAMH264" | "ANALOGCAM" | "WEBCAMH264" | "CARDH264" | "PATERNAUDIO" \
    | "CONTEST" | "DESKTOP" | "HDMI" )
    let PIDPMT=$PIDVIDEO-1
  ;;
esac

########### Redirect old BATC Streamer mode to Streamer mode ###############

if [ "$MODE_OUTPUT" == "BATC" ]; then
  MODE_OUTPUT="STREAMER"
fi

########### Set 480p Output Format if compatible and required ###############

let IMAGE_HEIGHT=576

if [ "$MODE_INPUT" == "CAMMPEG-2" ] || [ "$MODE_INPUT" == "ANALOGMPEG-2" ]; then
  # Set IMAGE_HEIGHT
  if [ "$OPSTD" == "480" ]; then
    let IMAGE_HEIGHT=480
  fi
fi

######################### Calculate the output frequency in Hz ###############

# Round down to 1 kHz resolution.  Answer is always integer numeric

FREQ_OUTPUTKHZ=`echo - | awk '{print sprintf("%d", '$FREQ_OUTPUT' * 1000)}'`
FREQ_OUTPUTHZ="$FREQ_OUTPUTKHZ"000


######################### Pre-processing for each Output Mode ###############

case "$MODE_OUTPUT" in

  STREAMER)
    # Set Output string "-f flv "$STREAM_URL"/"$STREAM_KEY
    OUTPUT=$OUTPUT_STREAM
    # If CAMH264 is selected, temporarily select CAMMPEG-2
    if [ "$MODE_INPUT" == "CAMH264" ]; then
      MODE_INPUT="CAMMPEG-2"
    fi
    # If ANALOGCAM is selected, temporarily select ANALOGMPEG-2
    if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
      MODE_INPUT="ANALOGMPEG-2"
    fi
    # If WEBCAMH264 is selected, temporarily select WEBCAMMPEG-2
    if [ "$MODE_INPUT" == "WEBCAMH264" ]; then
      MODE_INPUT="WEBCAMMPEG-2"
    fi
    # If CARDH264 is selected, temporarily select CARDMPEG-2
    if [ "$MODE_INPUT" == "CARDH264" ]; then
      MODE_INPUT="CARDMPEG-2"
    fi
  ;;

  PLUTO)
    PLUTOPWR=$(get_config_var plutopwr $PCONFIGFILE)

    # Add an extra / to the beginning of the Pluto call if required to cope with /P
    echo $CALL | grep -q "/"
    if [ $? == 0 ]; then
      PLUTOCALL="/,${CALL}"
    else
      PLUTOCALL=",${CALL}"
    fi
  ;;

  DATVEXPRESS)
    if pgrep -x "express_server" > /dev/null
    then
      # Express already running
      :
    else
      # Stopped, so make sure the control file is not locked and start it
      # From its own folder otherwise it doesnt read the config file
      sudo rm /tmp/expctrl >/dev/null 2>/dev/null
      cd /home/pi/express_server
      sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
      cd /home/pi
      sleep 5
    fi
    # Set output for ffmpeg (avc2ts uses netcat to pipe output from videots)
     OUTPUT="udp://127.0.0.1:1314?pkt_size=1316&buffer_size=1316"
    FREQUENCY_OUT=0  # Not used in this mode?
    # Calculate output freq in Hz using floating point
    FREQ_OUTPUTHZ=`echo - | awk '{print '$FREQ_OUTPUT' * 1000000}'`
    echo "set freq "$FREQ_OUTPUTHZ >> /tmp/expctrl
    echo "set fec "$FECNUM"/"$FECDEN >> /tmp/expctrl
    echo "set srate "$SYMBOLRATE >> /tmp/expctrl
    # Set the ports
    $PATHSCRIPT"/ctlfilter.sh"

    # Set the output level
    GAIN=$(get_config_var explevel $PCONFIGFILE);

    # Set Gain
    echo "set level "$GAIN >> /tmp/expctrl

    # Make sure that carrier mode is off
    echo "set car off" >> /tmp/expctrl
  ;;

  IP)
    FREQUENCY_OUT=0
    # Set Output parameters for H264 (avc2ts) modes:
    OUTPUT_IP="-n"$UDPOUTADDR":"$UDPOUTPORT
    # Set Output parameters for MPEG-2 (ffmpeg) modes:
    OUTPUT="udp://"$UDPOUTADDR":"$UDPOUTPORT"?pkt_size=1316&buffer_size=1316" # Try this
  ;;

  COMPVID)
    # Temporarily set the symbol rate to something reasonable
    SYMBOLRATEK=1000
    let SYMBOLRATE=SYMBOLRATEK*1000
    FREQUENCY_OUT=0
    OUTPUT="/dev/null"
    # Get the Mic and on-board sound card numbers
    MICCARD="$(cat /proc/asound/modules | grep 'usb_audio' | head -c 2 | tail -c 1)"
    RPICARD="$(cat /proc/asound/modules | grep 'bcm2835' | head -c 2 | tail -c 1)"
    # Pass the USB mic input directly to the RPi Audio Out
    arecord -D plughw:"$MICCARD",0 -f cd - | aplay -D plughw:"$RPICARD",0 - &
  ;;

  "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
    OUTPUT=videots

    BAND_GPIO=$(get_config_var expports $PCONFIGFILE)

    # Allow for GPIOs in 16 - 31 range (direct setting)
    if [ "$BAND_GPIO" -gt "15" ]; then
      let BAND_GPIO=$BAND_GPIO-16
    fi

    LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
    $PATHSCRIPT"/ctlfilter.sh"

    if [ "$MODE_OUTPUT" == "LIMEUSB" ]; then
      LIMETYPE="-U"
    fi
  ;;

  "JLIME" | "JEXPRESS")
    LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
  ;;

  "MUNTJAC")
    OUTPUT=videots

    BAND_GPIO=$(get_config_var expports $PCONFIGFILE)

    # Allow for GPIOs in 16 - 31 range (direct setting)
    if [ "$BAND_GPIO" -gt "15" ]; then
      let BAND_GPIO=$BAND_GPIO-16
    fi

    MUNTJAC_GAIN=$(get_config_var muntjacgain $PCONFIGFILE)
    $PATHSCRIPT"/ctlfilter.sh"
  ;;

  "JLIME" | "JEXPRESS")
    LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
  ;;
esac

OUTPUT_QPSK="videots"
# MODE_DEBUG=quiet
 MODE_DEBUG=debug

# ************************ CALCULATE BITRATES AND IMAGE SIZES ******************

# Select pilots on if required
if [ "$PILOT" == "on" ]; then
  PILOTS="-p"
else
  PILOTS=""
fi

# Select Short Frames if required
if [ "$FRAME" == "short" ]; then
  FRAMES="-v"
else
  FRAMES=""
fi

UPSAMPLE=1  # Set here to allow bitrate calculation
let SYMBOLRATE_K=SYMBOLRATE/1000

BITRATE_TS="$($PATHRPI"/dvb2iq" -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
              -d -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES )"

# Calculate the Video Bit Rate for MPEG-2 Sound/no sound
if [ "$MODE_INPUT" == "CAMMPEG-2" ] || [ "$MODE_INPUT" == "ANALOGMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAMHDMPEG-2" ] || [ "$MODE_INPUT" == "CARDMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAM16MPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
  if [ "$AUDIO_CHANNELS" != 0 ]; then                 # MPEG-2 with Audio
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-74000
  else                                                # MPEG-2 no audio
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-10000
  fi

  # And the image size for MPEG-2
  # Increase video resolution for CAMHDMPEG-2 and CAM16MPEG-2
  if [ "$MODE_INPUT" == "CAMHDMPEG-2" ] && [ "$BITRATE_VIDEO" -gt 500000 ]; then
    VIDEO_WIDTH=1280
    VIDEO_HEIGHT=720
    VIDEO_FPS=15
  elif [ "$MODE_INPUT" == "CAM16MPEG-2" ] && [ "$BITRATE_VIDEO" -gt 500000 ]; then
    VIDEO_WIDTH=1024
    VIDEO_HEIGHT=576
    VIDEO_FPS=15
  else
    # Reduce video resolution at low bit rates
    if [ "$BITRATE_VIDEO" -lt 170000 ]; then
      VIDEO_WIDTH=160
      VIDEO_HEIGHT=140
    else
      if [ "$BITRATE_VIDEO" -lt 300000 ]; then
        VIDEO_WIDTH=352
        VIDEO_HEIGHT=288
      else
        VIDEO_WIDTH=720
        VIDEO_HEIGHT=$IMAGE_HEIGHT
      fi
    fi
    if [ "$BITRATE_VIDEO" -lt 100000 ]; then
      VIDEO_WIDTH=96
      VIDEO_HEIGHT=80
    fi

    # Reduce MPEG-2 frame rate at low bit rates
    if [ "$BITRATE_VIDEO" -lt 150000 ]; then  # was 300000
      VIDEO_FPS=15
    else
      # Switch to 30 fps if required
      if [ "$IMAGE_HEIGHT" == "480" ]; then
        VIDEO_FPS=30
      else
        VIDEO_FPS=25
      fi
    fi
  fi
else # h264
  if [ "$AUDIO_CHANNELS" != 0 ]; then                 # H264 or H265 with AAC audio
    ARECORD_BUF=55000     # arecord buffer in us
    BITRATE_AUDIO=24000
    let TS_AUDIO_BITRATE=$BITRATE_AUDIO*15/10
    let BITRATE_VIDEO=($BITRATE_TS-24000-$TS_AUDIO_BITRATE)*725/1000
  else                                                # H264 or H265 no audio
    let BITRATE_VIDEO=($BITRATE_TS-12000)*725/1000
  fi

  # Set the H264 image size
  if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
    VIDEO_WIDTH=704
    VIDEO_HEIGHT=576
    VIDEO_FPS=25
  else
    VIDEO_WIDTH=352
    VIDEO_HEIGHT=288
    VIDEO_FPS=25
  fi
  if [ "$BITRATE_VIDEO" -lt 100000 ]; then
    VIDEO_WIDTH=160
    VIDEO_HEIGHT=120
  fi
fi

# Set IDRPeriod for avc2ts
# Default is 100, ie one every 4 sec at 25 fps
IDRPERIOD=100


# Set the SDR Up-sampling rate and correct gain for Lime Modes
case "$MODE_OUTPUT" in
  "LIMEMINI" | "LIMEUSB" | "LIMEDVB" | "JLIME" | "JEXPRESS")

    # Make sure Lime gain is sensible
    if [ "$LIME_GAIN" -lt 6 ]; then
      LIMEGAIN=6
    fi

    # Set digital gain
    let DIGITAL_GAIN=($LIME_GAIN*31)/100 # For LIMEDVB

    if [ "$SYMBOLRATE_K" -lt 990 ] ; then
      UPSAMPLE=2
      LIME_GAINF=`echo - | awk '{print '$LIME_GAIN' / 100}'`
    elif [ "$SYMBOLRATE_K" -lt 1500 ] && [ "$MODULATION" == "DVB-S" ] ; then
      UPSAMPLE=2
      LIME_GAINF=`echo - | awk '{print '$LIME_GAIN' / 100}'`
    else
      UPSAMPLE=1
      LIME_GAINF=`echo - | awk '{print ( '$LIME_GAIN' - 6 ) / 100}'`
    fi

    # Equalise Carrier Mode Level for Lime
    if [ "$MODE_INPUT" == "CARRIER" ]; then
      LIME_GAINF=`echo - | awk '{print ( '$LIME_GAIN' - 6 ) / 100}'`
    fi

    # Override for LIMEDVB Mode, and allow overide from config for non-DVB

    if [ "$MODE_OUTPUT" == "LIMEDVB" ]; then

      # For Low SRs
      UPSAMPLE=4
      LIME_GAINF=`echo - | awk '{print '$LIME_GAIN' / 100}'`

      if [ "$SYMBOLRATE_K" -gt 1010 ] ; then
        UPSAMPLE=2
        LIME_GAINF=`echo - | awk '{print ( '$LIME_GAIN' - 6 ) / 100}'`
      fi
      if [ "$SYMBOLRATE_K" -gt 4010 ] ; then
        UPSAMPLE=1
        LIME_GAINF=`echo - | awk '{print ( '$LIME_GAIN' - 6 ) / 100}'`
      fi
      CUSTOM_FPGA="-F"
    else
      CUSTOM_FPGA=" "
      if [ "$REQ_UPSAMPLE" -eq "2" ] ; then
        UPSAMPLE=2
      fi
      if [ "$REQ_UPSAMPLE" -eq "4" ] ; then
        UPSAMPLE=4
      fi
      if [ "$REQ_UPSAMPLE" -eq "8" ] ; then
        UPSAMPLE=8
      fi
    fi

    # Determine if Lime needs to be calibrated

    # Data in
    # LimeCalFreq < -1.5)  Never Calibrate
    # LimeCalFreq < -0.5)  Always calibrate
    # LimeCalFreq > -0.5)  Calibrate on freq change of greater than 5%

    # Command out:
    # CAL=1 # Calibrate every time
    # CAL=0 # Use saved cal

    if [ -f "$PATH_LIME_CAL" ]; then                                   # Lime Cal file exists, so read it
      LIMECALFREQ=$(get_config_var limecalfreq $PATH_LIME_CAL)
    else                                                               # No file, so set to calibrate 
      LIMECALFREQ="-1.0"
    fi

    if (( $(echo "$LIMECALFREQ < -0.5" |bc -l) )); then                # Never calibrate or always calibrate
      if (( $(echo "$LIMECALFREQ < -1.5" |bc -l) )); then              # Never calibrate
        CAL=0
      else                                                             # Always calibrate
        CAL=1
      fi
    else                                                               # calibrate on freq change > 5%
      CAL=0                                                            # default to no Cal
      if (( $(echo "$FREQ_OUTPUT > $LIMECALFREQ*1.05" |bc -l) )); then # calibrate
        CAL=1
        set_config_var limecalfreq "$FREQ_OUTPUT" $PATH_LIME_CAL       # and reset cal freq
      fi
      if (( $(echo "$FREQ_OUTPUT < $LIMECALFREQ*0.95" |bc -l) )); then # calibrate
        CAL=1
        set_config_var limecalfreq "$FREQ_OUTPUT" $PATH_LIME_CAL       # and reset cal freq
      fi
    fi
  ;;
  "MUNTJAC")
    MUNTJAC_GAINF=`echo - | awk '{print '$MUNTJAC_GAIN' / 100}'`
    UPSAMPLE=2
  ;;
esac

# Set the LimeSDR Send buffer size
LIMESENDBUF=10000


# Clean up before starting fifos
sudo rm videots >/dev/null 2>/dev/null
sudo rm netfifo >/dev/null 2>/dev/null
sudo rm audioin.wav >/dev/null 2>/dev/null

# Create the fifos
mkfifo videots
mkfifo netfifo
mkfifo audioin.wav

OUTPUT_FILE="-o videots"

# Branch to custom file to calculate DVB-T bitrate
if [ "$MODULATION" == "DVB-T" ]; then
  source /home/pi/rpidatv/scripts/a_dvb-t.sh
fi

echo "************************************"
echo Bitrate TS $BITRATE_TS
echo Bitrate Video $BITRATE_VIDEO
echo Size $VIDEO_WIDTH x $VIDEO_HEIGHT at $VIDEO_FPS fps
echo "************************************"
echo "Modulation = "$MODULATION
echo "ModeINPUT = "$MODE_INPUT
echo "LIME_GAINF = "$LIME_GAINF
echo

case "$MODE_INPUT" in

  #============================================ H264 PI CAM INPUT MODE =========================================================
  "CAMH264")

    # Check PiCam is present to prevent kernel panic    
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -ne 0 ]; then
      exit
    fi

    ################# Pluto Code for DVB-S and DVB-S2 (not DVB-T)

    if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "CAMH264" ] && [ "$MODULATION" != "DVB-T" ]; then

      # Make sure that Pi Cam driver is loaded
      sudo modprobe bcm2835_v4l2

      # Size image as required
      if [ "$FORMAT" == "16:9" ]; then
        VIDEO_WIDTH=1024
        VIDEO_HEIGHT=576
      elif [ "$FORMAT" == "720p" ] || [ "$FORMAT" == "1080p" ]; then
        VIDEO_WIDTH=1280
        VIDEO_HEIGHT=720
      else
        VIDEO_WIDTH=768
        VIDEO_HEIGHT=576
      fi

      # Rotate image if required
      if [ "$PICAM" != "normal" ]; then
        v4l2-ctl -d $VID_PICAM --set-ctrl=rotate=180
      else
        v4l2-ctl -d $VID_PICAM --set-ctrl=rotate=0
      fi

      # Size the viewfinder
      v4l2-ctl -d $VID_PICAM --set-fmt-overlay=left=0,top=0,width=736,height=416 --overlay 1 # For 800x480 framebuffer
      v4l2-ctl -d $VID_PICAM -p $VIDEO_FPS

      if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

        ############### Pi Cam Pluto No Audio ######################

        rpidatv/bin/ffmpeg -thread_queue_size 2048 \
          -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -i $VID_PICAM \
          -c:v h264_omx -b:v $BITRATE_VIDEO -g 25 \
          -f flv \
          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &

      else

        ############### Pi Cam Pluto With Audio ######################

        rpidatv/bin/ffmpeg -thread_queue_size 2048 \
          -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -i $VID_PICAM \
          -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
          -i hw:$AUDIO_CARD_NUMBER,0 \
          -c:v h264_omx -b:v $BITRATE_VIDEO -g 25 \
          -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
          -f flv \
          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
      fi
      exit
    fi

    ################# Lime, DATV Express and DVB-T Code #################################

    if [ "$FORMAT" == "16:9" ]; then
      VIDEO_WIDTH=1024
      VIDEO_HEIGHT=576
    elif [ "$FORMAT" == "720p" ] || [ "$FORMAT" == "1080p" ]; then
      VIDEO_WIDTH=1280
      VIDEO_HEIGHT=720
    else
      # Set the image size depending on bitrate (except for widescreen)
      if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
        VIDEO_WIDTH=768
        VIDEO_HEIGHT=576
      else
        VIDEO_WIDTH=384
        VIDEO_HEIGHT=288
      fi
      if [ "$BITRATE_VIDEO" -lt 100000 ]; then
        VIDEO_WIDTH=160
        VIDEO_HEIGHT=112
      fi
    fi

    # Free up Pi Camera for direct OMX Coding by removing driver
    sudo modprobe -r bcm2835_v4l2

    if [ "$MODULATION" != "DVB-T" ]; then

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "IP")
        OUTPUT_FILE=""
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots & 
      ;;
      "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
        $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "MUNTJAC")
        $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
      "COMPVID")
        OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    else
      OUTPUT_FILE=""
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "PLUTO")
           OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a -"$PLUTOPWR" -r pluto \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -n $PLUTOIP -i /dev/null &
        ;;
        "LIMEMINI" | "LIMEUSB")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a $LIME_GAINF -r lime \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -n $PLUTOIP -i /dev/null &
        ;;
      esac
    fi

    # Now generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************

      sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 0 $H264ORIENTATION -e $ANALOGCAMNAME -p $PIDPMT -s $CALL $OUTPUT_IP \
         > /dev/null &
    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************

      if [ $AUDIO_SAMPLE != 48000 ]; then
        arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
          | sox --buffer 1024 -t wav - audioin.wav rate 48000 &
      else
        arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &
      fi

      sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 0 $H264ORIENTATION -e $ANALOGCAMNAME -p $PIDPMT -s $CALL $OUTPUT_IP \
        -a audioin.wav -z $BITRATE_AUDIO  >/dev/null 2>/dev/null &
    fi
  ;;


  #============================================ MPEG-2 PI CAM INPUT MODE =============================================================
  "CAMMPEG-2"|"CAMHDMPEG-2"|"CAM16MPEG-2")

# Set up the command for the MPEG-2 Callsign caption
# Note that spaces are not allowed in the CAPTION string below!

# Put caption at the bottom of the screen with locator for US users
if [ "$IMAGE_HEIGHT" == "480" ]; then
  if [ "$CAPTIONON" == "on" ]; then
    CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf:\
text=$CALL-$LOCATOR:fontcolor=white:fontsize=36:box=1:boxcolor=black@0.5:boxborderw=5:\
x=(w/2-(text_w/2)):y=(h-text_h-40)"
    VF="-vf "
  else
    CAPTION=""
    VF=""    
  fi
else
  if [ "$CAPTIONON" == "on" ]; then
    CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf:\
text=$CALL:fontcolor=white:fontsize=36:box=1:boxcolor=black@0.5:boxborderw=5:\
x=w/10:y=(h/4-text_h)/2"
    VF="-vf "
  else
    CAPTION=""
    VF=""    
  fi
fi

    # Rotate image if required
    if [ "$PICAM" != "normal" ]; then
      v4l2-ctl -d $VID_PICAM --set-ctrl=rotate=180
    else
      v4l2-ctl -d $VID_PICAM --set-ctrl=rotate=0
    fi

    # Size the viewfinder
    v4l2-ctl -d $VID_PICAM --set-fmt-overlay=left=0,top=0,width=736,height=416 --overlay 1 # For 800x480 framebuffer
    v4l2-ctl -d $VID_PICAM -p $VIDEO_FPS

    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "STREAMER")
        ITS_OFFSET="-00:00:00"
      ;;
      "IP")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "COMPVID")
        : # Do nothing
      ;;
      "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
        $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "MUNTJAC")
        $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    case "$MODE_OUTPUT" in
      "STREAMER")
        if [ "$VIDEO_WIDTH" -lt 720 ]; then
          VIDEO_WIDTH=720
          VIDEO_HEIGHT=576
        fi
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=1024
          VIDEO_HEIGHT=576
        fi

        # No code for beeps here
        sudo modprobe bcm2835_v4l2
        if [ "$AUDIO_CARD" == 0 ]; then
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
            -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_PICAM \
            $VF $CAPTION -framerate 25 \
            -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 576k \
            -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        else
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_PICAM -thread_queue_size 2048 \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            $VF $CAPTION -framerate 25 \
            -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
            -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi
      ;;
      *)
        if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

          # ******************************* MPEG-2 VIDEO WITH NO AUDIO ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i $VID_PICAM -fflags nobuffer \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 \
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

          # ******************************* MPEG-2 VIDEO WITH BEEP ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i $VID_PICAM -fflags nobuffer \
            \
            -f lavfi -ac 1 \
            -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        else

          # ******************************* MPEG-2 VIDEO WITH AUDIO ************************************

          # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  nice -n -30 
          # PMT, Vid and Audio PIDs can all be set. 

          sudo $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512 \
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i $VID_PICAM -fflags nobuffer \
            \
            -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
            -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS \
            -f mpegts  -blocksize 1880 \
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &
        fi
      ;;
    esac
  ;;

  #============================================ ANALOG and WEBCAM H264 =============================================================
  "ANALOGCAM" | "WEBCAMH264")

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    ##################### Pluto Code ##############################

    if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "WEBCAMH264" ] && [ "$MODULATION" != "DVB-T" ]; then

      # Set default format to catch undocumented webcams (note mono audio at 48K sample rate)
      INPUT_FORMAT="yuyv422"
      VIDEO_WIDTH=640
      VIDEO_HEIGHT=480
      AUDIO_SAMPLE=48000
      AUDIO_CHANNELS=1

      if [ "$WEBCAM_TYPE" == "OldC920" ]; then  # Old C920 with internal H264 encoder
        INPUT_FORMAT="h264"
        AUDIO_SAMPLE=32000
        AUDIO_CHANNELS=2
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=448
        elif [ "$FORMAT" == "720p" ] || [ "$FORMAT" == "1080p" ]; then
          VIDEO_WIDTH=1280
          VIDEO_HEIGHT=720
        else
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=600
        fi
      fi

      # Orbicam C920, recent C920, B910 or C910
      if [ "$WEBCAM_TYPE" == "OrbicamC920" ] || [ "$WEBCAM_TYPE" == "NewerC920" ] \
      || [ "$WEBCAM_TYPE" == "B910" ] || [ "$WEBCAM_TYPE" == "C910" ]; then
        INPUT_FORMAT="yuyv422"
        AUDIO_SAMPLE=32000
        AUDIO_CHANNELS=2
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=448
        else
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=600
        fi
      fi

      # Logitech Brio 4K
      if [ "$WEBCAM_TYPE" == "BRIO4K" ]; then
        INPUT_FORMAT="yuyv422"
        AUDIO_SAMPLE=24000
        AUDIO_CHANNELS=2
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=448
        else
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=600
        fi
      fi

      if [ "$WEBCAM_TYPE" == "C930e" ]; then  # C930e (G4KLB)
        INPUT_FORMAT="yuyv422"
        AUDIO_CHANNELS=2
        AUDIO_SAMPLE=48000
        if [ "$FORMAT" == "720p" ]; then
          VIDEO_WIDTH=1280
          VIDEO_HEIGHT=720
        elif [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=448
        else
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=600
        fi
      fi

      if [ "$WEBCAM_TYPE" == "ALI4K" ]; then  # Ali Express 4K UHD (PA3FBX)
        INPUT_FORMAT="yuyv422"
        AUDIO_CHANNELS=1
        AUDIO_SAMPLE=16000
        if [ "$FORMAT" == "720p" ]; then
          VIDEO_WIDTH=1280
          VIDEO_HEIGHT=720
        elif [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=480
        else
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=600
        fi
      fi

      if [ "$WEBCAM_TYPE" == "C170" ] || [ "$WEBCAM_TYPE" == "C525" ]; then
        INPUT_FORMAT="yuyv422"
        AUDIO_CHANNELS=1
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=640
          VIDEO_HEIGHT=360
        else
          VIDEO_WIDTH=640
          VIDEO_HEIGHT=480
        fi
      fi

      if [ "$WEBCAM_TYPE" == "EagleEye" ]; then
        INPUT_FORMAT="yuyv422"
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=640
          VIDEO_HEIGHT=360
        elif [ "$FORMAT" == "720p" ]; then
          VIDEO_WIDTH=1280
          VIDEO_HEIGHT=720
        elif [ "$FORMAT" == "1080p" ]; then
          VIDEO_WIDTH=1920
          VIDEO_HEIGHT=1080
        else
          VIDEO_WIDTH=640
          VIDEO_HEIGHT=480
        fi
      fi

      # Overide Audio settings if not using webcam mic
      # "auto" and "webcam" UNCHANGED. "mic", and "video" here
      # "bleeps" and "no_audio" not implemented
      case "$AUDIO_PREF" in
        "mic")
          AUDIO_CHANNELS=1
          AUDIO_SAMPLE=48000
        ;;
        "video")
          AUDIO_CHANNELS=2
          AUDIO_SAMPLE=48000
        ;;
      esac

      rpidatv/bin/ffmpeg -thread_queue_size 2048 \
        -f v4l2 -input_format $INPUT_FORMAT -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
        -i $VID_WEBCAM \
        -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
        -i hw:$AUDIO_CARD_NUMBER,0 \
        -c:v h264_omx -b:v $BITRATE_VIDEO -g 25 \
        -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
        -f flv \
        rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
      exit
    fi

    ##################### Pluto DVB-S/S2 H264 EasyCap Code ##############################

    # Widescreen switching
    if [ "$FORMAT" == "16:9" ]; then
      SCALE="-vf scale=1024:576"
      # SCALE="-vf scale=720:400"  Slightly better for fast moving images
    else
      SCALE=""
    fi

    if [ "$MODE_INPUT" == "ANALOGCAM" ] && [ "$MODULATION" != "DVB-T" ]; then
      # Set the EasyCap input and video standard
      if [ "$ANALOGCAMINPUT" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-input="$ANALOGCAMINPUT
      fi
      if [ "$ANALOGCAMSTANDARD" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-standard="$ANALOGCAMSTANDARD
      fi

      # Pluto H264 EasyCap
      if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "ANALOGCAM" ]; then
        INPUT_FORMAT="yuyv422"
        rpidatv/bin/ffmpeg -thread_queue_size 2048 \
          -f v4l2 -input_format $INPUT_FORMAT \
          -i $ANALOGCAMNAME \
          -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS  \
          -i hw:$AUDIO_CARD_NUMBER,0 \
          -c:v h264_omx -b:v $BITRATE_VIDEO $SCALE -g 25 \
          -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
          -f flv \
          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &

        # Set the EasyCap contrast to prevent crushed whites
        sleep 0.7
        v4l2-ctl -d "$ANALOGCAMNAME" --set-ctrl "$ECCONTRAST" >/dev/null 2>/dev/null

      exit
    fi

    ##################### Lime and DATV Express Code ##############################

    else
      # Webcam in use, so set parameters depending on camera in use
      # Check audio first
      if [ "$AUDIO_PREF" == "auto" ] || [ "$AUDIO_PREF" == "webcam" ]; then  # Webcam audio
        if [ "$WEBCAM_TYPE" == "OldC920" ] || [ "$WEBCAM_TYPE" == "OrbicamC920" ] || [ "$WEBCAM_TYPE" == "NewerC920" ] \
        || [ "$WEBCAM_TYPE" == "B910" ] || [ "$WEBCAM_TYPE" == "C910" ]; then
          AUDIO_SAMPLE=32000
        fi
        if [ "$WEBCAM_TYPE" == "C930e" ] || [ "$WEBCAM_TYPE" == "EagleEye" ]; then
          AUDIO_SAMPLE=48000
        fi
        if [ "$WEBCAM_TYPE" == "BRIO4K" ]; then
          AUDIO_SAMPLE=24000
        fi
        if [ "$WEBCAM_TYPE" == "ALI4K" ]; then
          AUDIO_SAMPLE=16000
        fi
      fi

      # If a C920 put it in the right mode
      # Anything over 800x448 does not work
      if [ "$WEBCAM_TYPE" == "OldC920" ] || [ "$WEBCAM_TYPE" == "OrbicamC920" ] || [ "$WEBCAM_TYPE" == "BRIO4K" ] \
      || [ "$WEBCAM_TYPE" == "NewerC920" ] || [ "$WEBCAM_TYPE" == "C930e" ]; then
        if [ "$FORMAT" == "4:3" ]; then
          if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=0 --set-parm=15 \
              --set-ctrl power_line_frequency=1
            VIDEO_WIDTH=640
            VIDEO_HEIGHT=480
            VIDEO_FPS=15
          else
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=320,height=240,pixelformat=0 --set-parm=15 \
              --set-ctrl power_line_frequency=1
            VIDEO_WIDTH=320
            VIDEO_HEIGHT=240
            VIDEO_FPS=15
          fi
        else
          if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=448,pixelformat=0 --set-parm=15 \
              --set-ctrl power_line_frequency=1
            VIDEO_WIDTH=800
            VIDEO_HEIGHT=448
            VIDEO_FPS=15
          else
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=448,height=240,pixelformat=0 --set-parm=15 \
              --set-ctrl power_line_frequency=1
            VIDEO_WIDTH=448
            VIDEO_HEIGHT=240
            VIDEO_FPS=15
          fi
        fi
      fi

      # If a new C910 put it in the right mode
      # Anything over 800x448 does not work
      if [ "$WEBCAM_TYPE" == "C910" ]; then
        if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=448,pixelformat=0 --set-parm=15 \
            --set-ctrl power_line_frequency=1
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=448
          VIDEO_FPS=15
        else
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=432,height=240,pixelformat=0 --set-parm=15 \
            --set-ctrl power_line_frequency=1
          VIDEO_WIDTH=416  # Camera modes are not 32 aligned
          VIDEO_HEIGHT=240
          VIDEO_FPS=15
        fi
      fi

      if [ "$WEBCAM_TYPE" == "C170" ] || [ "$WEBCAM_TYPE" == "C525" ]; then
        AUDIO_SAMPLE=48000
        AUDIO_CHANNELS=1
        if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=0 --set-parm=15
          VIDEO_WIDTH=640
          VIDEO_HEIGHT=480
          VIDEO_FPS=15
        else
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=352,height=288,pixelformat=0 --set-parm=15
          VIDEO_WIDTH=352
          VIDEO_HEIGHT=288
          VIDEO_FPS=15
        fi
      fi

      if [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        AUDIO_SAMPLE=16000
        AUDIO_CHANNELS=1
        if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=480,pixelformat=0 --set-parm=30
          VIDEO_WIDTH=800
          VIDEO_HEIGHT=480
          VIDEO_FPS=30
        else
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=320,height=240,pixelformat=0 --set-parm=30
          VIDEO_WIDTH=320
          VIDEO_HEIGHT=240
          VIDEO_FPS=30
        fi
      fi

      if [ "$WEBCAM_TYPE" == "EagleEye" ]; then
        AUDIO_SAMPLE=48000
        AUDIO_CHANNELS=1
        if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
          if [ "$FORMAT" == "16:9" ]; then
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=360,pixelformat=0 --set-parm=25
            VIDEO_WIDTH=640
            VIDEO_HEIGHT=360
            VIDEO_FPS=25
          else
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=0 --set-parm=25
            VIDEO_WIDTH=640
            VIDEO_HEIGHT=480
            VIDEO_FPS=25
          fi
        else
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=352,height=288,pixelformat=0 --set-parm=15
          VIDEO_WIDTH=352
          VIDEO_HEIGHT=288
          VIDEO_FPS=15
        fi
      fi
      ANALOGCAMNAME=$VID_WEBCAM
    fi

    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi    

    if [ "$MODULATION" != "DVB-T" ]; then      ## For DVB-S and DVB-S2
      # Set up means to transport of stream out of unit
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "DATVEXPRESS")
          echo "set ptt tx" >> /tmp/expctrl
          sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
        ;;
        "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
          $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
        ;;
        "MUNTJAC")
          $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
        ;;
        *)
          sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
        ;;
      esac
    else                                      ######### DVB-T
      OUTPUT_FILE=""
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "PLUTO")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a -"$PLUTOPWR" -r pluto \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -n $PLUTOIP -i /dev/null &
        ;;
        "LIMEMINI" | "LIMEUSB")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a $LIME_GAINF -r lime \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -i /dev/null &
        ;;
      esac
    fi

    # Now generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************

      sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CALL $OUTPUT_IP \
         > /dev/null &

      if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
        # Set the EasyCap contrast to prevent crushed whites
        sleep 2
        v4l2-ctl -d "$ANALOGCAMNAME" --set-ctrl "$ECCONTRAST" >/dev/null 2>/dev/null
      fi

    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************

      if [ "$MODE_INPUT" == "ANALOGCAM" ]; then              # EasyCap.  No audio resampling required

        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=768
          VIDEO_HEIGHT=400
          #VIDEO_WIDTH=1024
          #VIDEO_HEIGHT=576
        fi

        # Select correct audio capture
        lsusb | grep -q "1b71:3002"
        if [ $? == 0 ]; then #                              Fushicai EasyCap
          arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &
        else
          rpidatv/bin/ffmpeg -f alsa -flags low_delay -ac 2 -i hw:$AUDIO_CARD_NUMBER,0 -f wav - > audioin.wav &
        fi

        sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
          -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CALL $OUTPUT_IP \
          -a audioin.wav > /dev/null &

          #-a audioin.wav -z $BITRATE_AUDIO > /dev/null &

        # Set the EasyCap contrast to prevent crushed whites
        sleep 2
        v4l2-ctl -d "$ANALOGCAMNAME" --set-ctrl "$ECCONTRAST" >/dev/null 2>/dev/null
      else
        # Resample the audio (was 32k or 48k which overruns, so this is reduced to 47500)
        arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
         | sox -c $AUDIO_CHANNELS --buffer 1024 -t wav - audioin.wav rate 47500 &  

        sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
          -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CALL $OUTPUT_IP \
          -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
      fi

      # Auto restart for arecord (which dies after about an hour)  in repeater TX modes
      while [[ "$MODE_STARTUP" == "TX_boot" || "$MODE_STARTUP" == "Keyed_TX_boot" ]]
      do
        sleep 10
        if ! pgrep -x "arecord" > /dev/null; then # arecord is not running, so restart it
          if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
            arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &
          else
            arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
              | sox -c $AUDIO_CHANNELS --buffer 1024 -t wav - audioin.wav rate 47500 &
          fi 
        fi
      done
    fi
  ;;

#============================================ DESKTOP MODES H264 =============================================================

  "DESKTOP" | "CONTEST" | "CARDH264")

    ############ Pluto CardH264 ##################################

    if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "CARDH264" ] && [ "$MODULATION" != "DVB-T" ]; then

      if [ "$FORMAT" == "16:9" ] || [ "$FORMAT" == "720p" ] || [ "$FORMAT" == "1080p" ]; then
        VIDEO_WIDTH=1024
        VIDEO_HEIGHT=576
        if [ "$CAPTIONON" == "on" ]; then
          rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
          rm /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
          convert -font "FreeSans" -size 1024x80 xc:transparent -fill white -gravity Center -pointsize 50 -annotate 0 $CALL /home/pi/tmp/caption.png
          convert /home/pi/rpidatv/scripts/images/tcfw16.jpg /home/pi/tmp/caption.png -geometry +0+478 -composite /home/pi/tmp/tcfw162.jpg
          IMAGEFILE="/home/pi/tmp/tcfw162.jpg"
          sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
        else
          IMAGEFILE="/home/pi/rpidatv/scripts/images/tcfw16.jpg"
          sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcfw16.jpg >/dev/null 2>/dev/null
        fi
      else
        VIDEO_WIDTH=720
        VIDEO_HEIGHT=576
        if [ "$CAPTIONON" == "on" ]; then
          rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
          rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
          convert -font "FreeSans" -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 $CALL /home/pi/tmp/caption.png
          convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png -geometry +0+475 -composite /home/pi/tmp/tcf2.jpg
          IMAGEFILE="/home/pi/tmp/tcf2.jpg"
          sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
        else
          IMAGEFILE="/home/pi/rpidatv/scripts/images/tcf.jpg"
          sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcf.jpg >/dev/null 2>/dev/null
        fi
      fi

      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

      # Turn the viewfinder off
      v4l2-ctl --overlay=0
      # Capture for web view after a delay
      (sleep 1; /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh) &

      $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
        -re -loop 1 \
        -i $IMAGEFILE -framerate 1 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
        -c:v h264_omx -b:v $BITRATE_VIDEO \
        -f flv \
        rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
      exit
    fi

    ############ Pluto Contest ##################################

    if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "CONTEST" ] && [ "$MODULATION" != "DVB-T" ]; then

      # Delete the old numbers image
      rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null

      # Set size of contest numbers image up front to save resizing afterwards
      CNGEOMETRY="720x576"

      # Create the numbers image in the tempfs folder
      convert -font "FreeSans" -size "${CNGEOMETRY}" xc:white \
        -gravity NorthWest -pointsize 125 -annotate 0,0,20,20 "$CALL" \
        -gravity NorthEast -pointsize 30 -annotate 0,0,20,35 "$NUMBERS" \
        -gravity Center -pointsize 225 -annotate 0,0,0,20 "$NUMBERS" \
        -gravity South -pointsize 75 -annotate 0,0,0,20 "$LOCATOR   ""$BAND_LABEL" \
        /home/pi/tmp/contest.jpg
      IMAGEFILE="/home/pi/tmp/contest.jpg"

      # Display the numbers on the desktop
      sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

      # Turn the viewfinder off
      v4l2-ctl --overlay=0

      # Capture for web view
      /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh

         $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
          -re -loop 1 \
          -i $IMAGEFILE \
          -framerate 25 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -c:v h264_omx -b:v $BITRATE_VIDEO \
          -f flv \
          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
      exit
    fi

    ############ Pluto Desktop ##################################


    if [ "$MODE_OUTPUT" == "PLUTO" ] && [ "$MODE_INPUT" == "DESKTOP" ] && [ "$MODULATION" != "DVB-T" ]; then

      # Grab an image of the desktop
      rm /home/pi/tmp/desktop.jpg >/dev/null 2>/dev/null
      $PATHRPI"/ffmpeg" -f fbdev -i /dev/fb0 -qscale:v 2 -vframes 1 /home/pi/tmp/desktop.jpg
      IMAGEFILE="/home/pi/tmp/desktop.jpg"

      # Turn the viewfinder off
      v4l2-ctl --overlay=0

      # Capture for web view
      /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh

         $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
            -re -loop 1 \
            -i $IMAGEFILE \
            -framerate 25 -video_size 800x480 -c:v h264_omx -b:v $BITRATE_VIDEO \
        -f flv \
        rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
      exit
    fi


    ############## Lime and DATV Express Contest and Card ##############################################

    # Set the image size depending on bitrate (except for widescreen)
    if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
      VIDEO_WIDTH=768
      VIDEO_HEIGHT=576
    else
      VIDEO_WIDTH=384
      VIDEO_HEIGHT=288
    fi
    if [ "$BITRATE_VIDEO" -lt 100000 ]; then
      VIDEO_WIDTH=160
      VIDEO_HEIGHT=112
    fi

    if [ "$MODE_INPUT" == "CONTEST" ]; then
      # Delete the old numbers image
      rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null

      # Set size of contest numbers image up front to save resizing afterwards.
      CNGEOMETRY="800x480"

      # Create the numbers image in the tempfs folder
      convert -font "FreeSans" -size "${CNGEOMETRY}" xc:white \
        -gravity NorthWest -pointsize 125 -annotate 0,0,20,20 "$CALL" \
        -gravity NorthEast -pointsize 30 -annotate 0,0,20,35 "$NUMBERS" \
        -gravity Center -pointsize 225 -annotate 0,0,0,20 "$NUMBERS" \
        -gravity South -pointsize 75 -annotate 0,0,0,20 "$LOCATOR   ""$BAND_LABEL" \
        /home/pi/tmp/contest.jpg

      # Display the numbers on the desktop
      sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

    elif [ "$MODE_INPUT" == "CARDH264" ]; then
      if [ "$FORMAT" == "16:9" ] || [ "$FORMAT" == "720p" ] || [ "$FORMAT" == "1080p" ]; then
        VIDEO_WIDTH=800
        VIDEO_HEIGHT=448
        rm /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
        if [ "$CAPTIONON" == "on" ]; then
          rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
          convert -font "FreeSans" -size 1024x80 xc:transparent -fill white -gravity Center -pointsize 50 -annotate 0 $CALL /home/pi/tmp/caption.png
          convert /home/pi/rpidatv/scripts/images/tcfw16.jpg /home/pi/tmp/caption.png -geometry +0+478 -composite /home/pi/tmp/tcfw162.jpg
        else
          cp /home/pi/rpidatv/scripts/images/tcfw16.jpg /home/pi/tmp/tcfw162.jpg
        fi
        convert /home/pi/tmp/tcfw162.jpg -resize '800x480!' /home/pi/tmp/tcfw162.jpg
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
      else
        rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
        if [ "$CAPTIONON" == "on" ]; then
          rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
          convert -font "FreeSans" -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 $CALL /home/pi/tmp/caption.png
          convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png -geometry +0+475 -composite /home/pi/tmp/tcf2.jpg
        else
          cp /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/tcf2.jpg
        fi
        convert /home/pi/tmp/tcf2.jpg -resize '800x480!' /home/pi/tmp/tcf2.jpg
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

    else  # DESKTOP
      sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    fi

    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi

    # Capture for web view after a delay
    (sleep 1; /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh) &

    # Set up means to transport of stream out of unit

    if [ "$MODULATION" != "DVB-T" ]; then
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "DATVEXPRESS")
          echo "set ptt tx" >> /tmp/expctrl
          sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
        ;;
        "COMPVID")
          OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
        ;;
        "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
          $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
        ;;
        "MUNTJAC")
          $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
          *)
          sudo nice -n -30 $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
        ;;
      esac
    else                     # DVB-T
      OUTPUT_FILE=""
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "PLUTO")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a -"$PLUTOPWR" -r pluto \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -n $PLUTOIP -i /dev/null &
        ;;
        "LIMEMINI" | "LIMEUSB")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a $LIME_GAINF -r lime \
            -g 1/"$GUARD" -b $SYMBOLRATE -p 1314 -e "$FECNUM"/"$FECDEN" -i /dev/null &
        ;;
      esac
    fi

    # Pause to allow test card to be displayed
#    sleep 1

    # Now generate the stream

    # Audio does not work well, and is not required, so disabled.

    # ******************************* H264 VIDEO, NO AUDIO ************************************

    VIDEO_FPS=10  #
    IDRPERIOD=10  #  Setting these parameters prevents the partial picture problem

     sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
      -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 3 -p $PIDPMT -s $CALL $OUTPUT_IP \
       > /dev/null &
  ;;

# *********************************** TRANSPORT STREAM INPUT THROUGH IP ******************************************

  "IPTSIN" | "IPTSIN264" | "IPTSIN265")

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    if [ "$MODULATION" != "DVB-T" ]; then
      # Set up means to transport of stream out of unit
      case "$MODE_OUTPUT" in
        "DATVEXPRESS")
          echo "set ptt tx" >> /tmp/expctrl
          sudo nice -n -30 nc -u -4 127.0.0.1 1314 < videots &
        ;;
        "COMPVID")
          : # Do nothing.  Mode does not work yet
        ;;
        "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
        $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
        ;;
        "MUNTJAC")
          $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
        ;;
        "PLUTO")
        rpidatv/bin/ffmpeg -thread_queue_size 2048 \
          -i udp://:@:"$UDPINPORT"?fifo_size=1000000"&"overrun_nonfatal=1 -c:v copy -c:a copy \
          -f flv \
          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
        exit
        ;;
        *)
          sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
        ;;
      esac

      # Now generate the stream
      if [ "$MODE_INPUT" == "IPTSIN" ]; then                        # TS with service info
        netcat -u -4 -l $UDPINPORT > videots &

      elif [ "$MODE_INPUT" == "IPTSIN264" ]; then                   # Generate H264 service info for raw TS
        rpidatv/bin/ffmpeg -probesize 500000 -analyzeduration 500000 -fflags nobuffer -flags low_delay -thread_queue_size 1024 \
          -i udp:\\\\@:"$UDPINPORT" -ss 2 \
          -c:v copy -muxrate $BITRATE_TS \
          -c:a copy -f mpegts \
          -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -mpegts_service_type "0x19" -mpegts_flags system_b \
          -muxrate $BITRATE_TS -y $OUTPUT &

      elif [ "$MODE_INPUT" == "IPTSIN265" ]; then                   # Generate H265service info for raw TS
        rpidatv/bin/ffmpeg -probesize 500000 -analyzeduration 500000 -fflags nobuffer -flags low_delay -thread_queue_size 1024 \
          -i udp:\\\\@:"$UDPINPORT" -ss 2 \
          -c:v copy -muxrate $BITRATE_TS \
          -c:a copy -f mpegts \
          -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -mpegts_service_type "0x1f" -mpegts_flags system_b \
          -muxrate $BITRATE_TS -y $OUTPUT &
      fi
    else  # DVB-T
      case "$MODE_OUTPUT" in
        "PLUTO")
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a -"$PLUTOPWR" -r pluto \
            -g 1/"$GUARD" -b $SYMBOLRATE -p $UDPINPORT -e "$FECNUM"/"$FECDEN" -n $PLUTOIP -i /dev/null &
        ;;
        "LIMEMINI" | "LIMEUSB")
          OUTPUT_IP="-n 127.0.0.1:1314"
          /home/pi/rpidatv/bin/dvb_t_stack -m $CONSTLN -f $FREQ_OUTPUTHZ -a $LIME_GAINF -r lime \
            -g 1/"$GUARD" -b $SYMBOLRATE -p $UDPINPORT -e "$FECNUM"/"$FECDEN" -i /dev/null &
        ;;
      esac
    fi
  ;;

  # *********************************** TRANSPORT STREAM INPUT FILE ******************************************

  "FILETS")

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < $TSVIDEOFILE &
        #sudo nice -n -30 cat $TSVIDEOFILE | sudo nice -n -30 netcat -u -4 127.0.0.1 1314 & 
      ;;
      "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
      $PATHRPI"/limesdr_dvb" -i $TSVIDEOFILE -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "MUNTJAC")
        $PATHRPI"/muntjacsdr_dvb" -i $TSVIDEOFILE -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
#        "PLUTO")
#        rpidatv/bin/ffmpeg -thread_queue_size 2048 \
#          -i $TSVIDEOFILE -c:v copy -c:a copy \
#          -f flv \
#          rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
#        exit
#      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i $TSVIDEOFILE -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -l -x $PIN_I -y $PIN_Q &;;
    esac
  ;;

  # *********************************** CARRIER  ******************************************

  "CARRIER")
    case "$MODE_OUTPUT" in
      "DATVEXPRESS")
        echo "set car on" >> /tmp/expctrl
        echo "set ptt tx" >> /tmp/expctrl
      ;;
      "LIMEMINI" | "LIMEUSB")
      $PATHRPI"/limesdr_dvb" -s 1000000 -f carrier -r 1 -m DVBS2 -c QPSK \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "LIMEDVB")
      let DIGITAL_GAIN=6  # To equalise levels with normal DVB-S2 +/- 2 dB
      $PATHRPI"/limesdr_dvb" -s 1000000 -f carrier -r 1 -m DVBS2 -c QPSK \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO &
      ;;
      "PLUTO")
        # Put Pluto carrier code here
      ;;
    esac
  ;;

  #============================================ ANALOG AND WEBCAM MPEG-2 INPUT MODES ======================================================
  #============================================ INCLUDES STREAMING OUTPUTS (H264)    ======================================================
  "ANALOGMPEG-2" | "ANALOG16MPEG-2" | "WEBCAMMPEG-2" | "WEBCAM16MPEG-2" | "WEBCAMHDMPEG-2")

    # Set the default sound/video lipsync
    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    if [ "$MODE_INPUT" == "WEBCAMMPEG-2" ] && [ "$FORMAT" == "16:9" ]; then
      MODE_INPUT="WEBCAM16MPEG-2"
    fi

    # Sort out image size, video delay and scaling
    SCALE=""
    case "$MODE_INPUT" in
    ANALOG16MPEG-2)
      SCALE="scale=512:288,"
    ;;
    WEBCAMMPEG-2)
      if [ "$WEBCAM_TYPE" == "OldC920" ] || [ "$WEBCAM_TYPE" == "OrbicamC920" ] \
      || [ "$WEBCAM_TYPE" == "NewerC920" ] || [ "$WEBCAM_TYPE" == "C910" ] \
      || [ "$WEBCAM_TYPE" == "C930e" ] || [ "$WEBCAM_TYPE" == "B910" ] \
      || [ "$WEBCAM_TYPE" == "BRIO4K" ] || [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=0 \
          --set-ctrl power_line_frequency=1
        AUDIO_SAMPLE=32000
      fi
      if [ "$WEBCAM_TYPE" == "C270" ] ; then
        ITS_OFFSET="-00:00:1.8"
      fi
      if [ "$WEBCAM_TYPE" == "C310" ]; then
        ITS_OFFSET="-00:00:1.8"
      fi
      if [ "$WEBCAM_TYPE" == "BRIO4K" ]; then
        AUDIO_SAMPLE=24000
      fi
      if [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        AUDIO_SAMPLE=16000
      fi
      VIDEO_WIDTH="640"
      VIDEO_HEIGHT="480"
      VIDEO_FPS=$WC_VIDEO_FPS
    ;;
    WEBCAM16MPEG-2)
      if [ "$WEBCAM_TYPE" == "OldC920" ] || [ "$WEBCAM_TYPE" == "OrbicamC920" ] \
      || [ "$WEBCAM_TYPE" == "NewerC920" ] || [ "$WEBCAM_TYPE" == "C910" ] \
      || [ "$WEBCAM_TYPE" == "C930e" ] || [ "$WEBCAM_TYPE" == "B910" ] \
      || [ "$WEBCAM_TYPE" == "BRIO4K" ] || [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=0 \
          --set-ctrl power_line_frequency=1
        AUDIO_SAMPLE=32000
      fi
      if [ "$WEBCAM_TYPE" == "C270" ]; then
        ITS_OFFSET="-00:00:1.8"
      fi
      if [ "$WEBCAM_TYPE" == "C310" ]; then
        ITS_OFFSET="-00:00:1.8"
      fi
      if [ "$WEBCAM_TYPE" == "BRIO4K" ]; then
        AUDIO_SAMPLE=24000
      fi
      if [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        AUDIO_SAMPLE=16000
      fi
      VIDEO_WIDTH="1280"
      VIDEO_HEIGHT="720"
      if [ "$WEBCAM_TYPE" == "C170" ] || [ "$WEBCAM_TYPE" == "C525" ]; then
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=360,pixelformat=0
        VIDEO_WIDTH="640"
        VIDEO_HEIGHT="360"
      fi
      VIDEO_FPS=$WC_VIDEO_FPS
      SCALE="scale=1024:576,"
    ;;
    WEBCAMHDMPEG-2)
      if [ "$WEBCAM_TYPE" == "OldC920" ] || [ "$WEBCAM_TYPE" == "OrbicamC920" ] \
      || [ "$WEBCAM_TYPE" == "NewerC920" ] || [ "$WEBCAM_TYPE" == "C910" ] \
      || [ "$WEBCAM_TYPE" == "C930e" ] || [ "$WEBCAM_TYPE" == "B910" ] \
      || [ "$WEBCAM_TYPE" == "BRIO4K" ] || [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=0 \
          --set-ctrl power_line_frequency=1
        AUDIO_SAMPLE=32000
      fi
      if [ "$WEBCAM_TYPE" == "C270" ]; then
        ITS_OFFSET="-00:00:2.0"
      fi
      if [ "$WEBCAM_TYPE" == "C310" ]; then
        ITS_OFFSET="-00:00:2.0"
      fi
      if [ "$WEBCAM_TYPE" == "BRIO4K" ]; then
        AUDIO_SAMPLE=24000
      fi
      if [ "$WEBCAM_TYPE" == "ALI4K" ]; then
        AUDIO_SAMPLE=16000
      fi
      VIDEO_WIDTH="1280"
      VIDEO_HEIGHT="720"
      VIDEO_FPS=$WC_VIDEO_FPS
    ;;
    esac

    # Over-ride for C930e
    if [ "$WEBCAM_TYPE" == "C930e" ]; then
      AUDIO_SAMPLE=48000
      VIDEO_FPS=30
    fi

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    # Set up the command for the MPEG-2 Callsign caption
    # Put caption at the bottom of the screen with locator for US users
    if [ "$IMAGE_HEIGHT" == "480" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf: \
          text=\'$CALL-$LOCATOR\': fontcolor=white: fontsize=36: box=1: boxcolor=black@0.5: \
          boxborderw=5: x=(w/2-(text_w/2)): y=(h-text_h-40), "
      else
        CAPTION=""
      fi
    else
      if [ "$CAPTIONON" == "on" ]; then
        CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf: \
          text=\'$CALL\': fontcolor=white: fontsize=36: box=1: boxcolor=black@0.5: \
          boxborderw=5: x=w/10: y=(h/4-text_h)/2, "
      else
        CAPTION=""    
      fi
    fi

    # Select USB Video Dongle or Webcam and set USB Video modes
    if [ "$MODE_INPUT" == "ANALOGMPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
      # Set the EasyCap input 
      if [ "$ANALOGCAMINPUT" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-input="$ANALOGCAMINPUT
      fi
      # Set PAL or NTSC standard
      if [ "$ANALOGCAMSTANDARD" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-standard="$ANALOGCAMSTANDARD
      fi
    else
      VID_USB=$VID_WEBCAM
    fi

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "IP" | "STREAMER" | "COMPVID")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
        $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "MUNTJAC")
        $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream

    case "$MODE_OUTPUT" in
      "STREAMER")
        if [ "$VIDEO_WIDTH" -lt 640 ]; then
          VIDEO_WIDTH=720
          VIDEO_HEIGHT=576
        fi
        if [ "$FORMAT" == "16:9" ]; then
          SCALE="scale=1024:576,"
        else
          SCALE=""
        fi

        if [ "$AUDIO_CARD" == "0" ]; then
          # No audio
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048\
            -f v4l2 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_USB \
            -framerate 25 -c:v h264_omx -b:v 576k \
            -vf "$CAPTION""$SCALE"yadif=0:1:0 -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        else
          # With audio
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048\
            -f v4l2 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_USB \
            -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            -framerate 25 -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
            -vf "$CAPTION""$SCALE"yadif=0:1:0 -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi

        if [ "$MODE_INPUT" == "ANALOGMPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
          # Set the EasyCap contrast to prevent crushed whites
          sleep 1
          v4l2-ctl -d "$ANALOGCAMNAME" --set-ctrl "$ECCONTRAST" >/dev/null 2>/dev/null
        fi
exit
      ;;

      *) # Transmitting modes

      if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

        # ******************************* MPEG-2 ANALOG VIDEO WITH NO AUDIO ************************************

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512 \
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -i $VID_USB -fflags nobuffer \
          \
          -c:v mpeg2video  -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
          -f mpegts -blocksize 1880 \
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
          -muxrate $BITRATE_TS -y $OUTPUT &

      elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

        # ******************************* MPEG-2 ANALOG VIDEO WITH BEEP ************************************

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
          -i $VID_USB -fflags nobuffer \
          \
          -f lavfi -ac 1 \
          -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
          \
          -c:v mpeg2video -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
          -f mpegts -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
          -muxrate $BITRATE_TS -y $OUTPUT &

      else

        # ******************************* MPEG-2 ANALOG VIDEO WITH AUDIO ************************************

        # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
        # PMT, Vid and Audio PIDs can all be set.

        $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
          -i $VID_USB -fflags nobuffer \
          \
          -thread_queue_size 512 \
          -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
          -i hw:$AUDIO_CARD_NUMBER,0 \
          \
          -c:v mpeg2video -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
          -f mpegts -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
          -muxrate $BITRATE_TS -y $OUTPUT &

#          -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \

      fi

      if [ "$MODE_INPUT" == "ANALOGMPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
        # Set the EasyCap contrast to prevent crushed whites
        sleep 2
        v4l2-ctl -d "$ANALOGCAMNAME" --set-ctrl "$ECCONTRAST" >/dev/null 2>/dev/null
      fi
    ;;
    esac
  ;;
  #============================================ MPEG-2 STATIC TEST CARD MODE =============================================================
  "CARDMPEG-2" | "CONTESTMPEG-2" | "CARD16MPEG-2"| "CARDHDMPEG-2"  )

    if [ "$MODE_INPUT" == "CONTESTMPEG-2" ]; then
      # Delete the old numbers image
      rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null

      # Set size of contest numbers image up front to save resizing afterwards
      CNGEOMETRY="720x576"
      if [ "$DISPLAY" == "Element14_7" ]; then
        CNGEOMETRY="800x480"
      fi

      # Create the numbers image in the tempfs folder
      convert -font "FreeSans" -size "${CNGEOMETRY}" xc:white \
        -gravity NorthWest -pointsize 125 -annotate 0,0,20,20 "$CALL" \
        -gravity NorthEast -pointsize 30 -annotate 0,0,20,35 "$NUMBERS" \
        -gravity Center -pointsize 225 -annotate 0,0,0,20 "$NUMBERS" \
        -gravity South -pointsize 75 -annotate 0,0,0,20 "$LOCATOR   ""$BAND_LABEL" \
        /home/pi/tmp/contest.jpg
      IMAGEFILE="/home/pi/tmp/contest.jpg"

      # Display the numbers on the desktop
      sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    
    elif [ "$MODE_INPUT" == "CARDMPEG-2" ] && [ "$FORMAT" != "16:9" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
        convert -font "FreeSans" -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png -geometry +0+475 -composite /home/pi/tmp/tcf2.jpg
        IMAGEFILE="/home/pi/tmp/tcf2.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcf.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcf.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    elif [ "$MODE_INPUT" == "CARD16MPEG-2" ] || [ "$FORMAT" == "16:9" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
        convert -font "FreeSans" -size 1024x80 xc:transparent -fill white -gravity Center -pointsize 50 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcfw16.jpg /home/pi/tmp/caption.png -geometry +0+478 -composite /home/pi/tmp/tcfw162.jpg
        IMAGEFILE="/home/pi/tmp/tcfw162.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcfw16.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcfw16.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    elif [ "$MODE_INPUT" == "CARDHDMPEG-2" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcfw2.jpg >/dev/null 2>/dev/null
        convert -font "FreeSans" -size 1280x80 xc:transparent -fill white -gravity Center -pointsize 65 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcfw.jpg /home/pi/tmp/caption.png -geometry +0+608 -composite /home/pi/tmp/tcfw2.jpg
        IMAGEFILE="/home/pi/tmp/tcfw2.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw2.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcfw.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcfw.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    fi

  # Turn the viewfinder off
  v4l2-ctl --overlay=0

    # Capture for web view
    /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh

    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "STREAMER")
        ITS_OFFSET="-00:00:00"
      ;;
      "IP")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "COMPVID")
        : # Do nothing
      ;;
      "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
        $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
      ;;
      "MUNTJAC")
        $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    case "$MODE_OUTPUT" in
      "STREAMER")
        if [ "$FORMAT" == "16:9" ]; then
          VIDEO_WIDTH=1280
          VIDEO_HEIGHT=720
        else
          VIDEO_WIDTH=720
          VIDEO_HEIGHT=576
        fi

        # No code for beeps here
        if [ "$AUDIO_CARD" == 0 ]; then
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
            -f image2 -loop 1 \
            -i $IMAGEFILE \
            -framerate 25 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 576k \
            $VF $CAPTION \
            \
            -f flv $STREAM_URL/$STREAM_KEY &
        else
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -f image2 -loop 1 \
            -i $IMAGEFILE \
            \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            -framerate 25 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k            \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi
      ;;
      *)
        if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

          # ******************************* MPEG-2 CARD WITH NO AUDIO ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -re -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 \
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

          # ******************************* MPEG-2 CARD WITH BEEP ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -re -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -f lavfi -ac 1 \
            -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        else

          # ******************************* MPEG-2 CARD WITH AUDIO ************************************

          # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
          # PMT, Vid and Audio PIDs can all be set. nice -n -30 

          sudo $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -thread_queue_size 512 \
            -re -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -thread_queue_size 512 \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CHANNEL -metadata service_name=$CALL \
            -muxrate $BITRATE_TS -y $OUTPUT &
        fi
      ;;
    esac
  ;;

  #==================================== HDMI INPUT FROM LKV373A FOR PLUTO OR STREAMING ==========================
  "HDMI")

    if [ "$WEBCAM_TYPE" != "CamLink4K" ]; then

      case "$MODE_OUTPUT" in
        "STREAMER")
          rpidatv/bin/ffmpeg -thread_queue_size 2048 -fifo_size 229376 \
            -i udp://"$LKVUDP":"$LKVPORT" \
            -c:v h264_omx -b:v 1024k \
            -codec:a aac -b:a 128k \
            -f flv $STREAM_URL/$STREAM_KEY &
        ;;

        "PLUTO")
          rpidatv/bin/ffmpeg -thread_queue_size 2048 -fifo_size 229376 \
            -i udp://"$LKVUDP":"$LKVPORT" \
            -c:v h264_omx -b:v $BITRATE_VIDEO -g 25 \
            -ar 22050 -ac 2 -ab 64k \
            -f flv \
            rtmp://$PLUTOIP:7272/,$FREQ_OUTPUT,$MODTYPE,$CONSTLN,$SYMBOLRATE_K,$PFEC,-$PLUTOPWR,nocalib,800,32,/$PLUTOCALL, &
        ;;
      esac
    fi

  #==================================== HDMI INPUT FROM CAM LINK 4K.  OUTPUT TO LIME OR STREAMER ==========================

    if [ "$WEBCAM_TYPE" == "CamLink4K" ]; then
#echo
#echo HDMI Parameters ==============
#echo FORMAT $FORMAT
#echo WEBCAM_TYPE $WEBCAM_TYPE                  # CamLink4K
#echo VID_WEBCAM $VID_WEBCAM                    # /dev/video? for Camlink
#echo ANALOGCAMNAME $ANALOGCAMNAME              # /dev/video? for EasyCap
#echo VIDEO_FPS $VIDEO_FPS
#echo AUDIO_PREF $AUDIO_PREF
#echo AUDIO_CARD $AUDIO_CARD
#echo AUDIO_CARD_NUMBER $AUDIO_CARD_NUMBER
#echo ARECORD_BUF $ARECORD_BUF

      # If PiCam is present unload driver   
      vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
      RESULT="$?"
      if [ "$RESULT" -eq 0 ]; then
        sudo modprobe -r bcm2835_v4l2
      fi    

      # Set Video parameters and control the Cam Link 4K
      if [ "$BITRATE_VIDEO" -gt 190000 ]; then  # 333KS FEC 1/2 or better
        if [ "$FORMAT" == "1080p" ]; then
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1920,height=1080,pixelformat=0,field=25
            VIDEO_WIDTH=1920
            VIDEO_HEIGHT=1080
            VIDEO_FPS=25
          elif [ "$FORMAT" == "720p" ]; then
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=0,field=25
            VIDEO_WIDTH=1280
            VIDEO_HEIGHT=720
            VIDEO_FPS=25
          else
            v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=448,pixelformat=0,field=25
            VIDEO_WIDTH=800
            VIDEO_HEIGHT=448
            VIDEO_FPS=25
          fi
        else
          v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=352,height=288,pixelformat=0,field=25
          VIDEO_WIDTH=352
          VIDEO_HEIGHT=288
          VIDEO_FPS=25
        fi
      fi

      SCALE="$VIDEO_WIDTH":"$VIDEO_HEIGHT"

  #==================================== HDMI INPUT FROM CAM LINK 4K.  OUTPUT TO STREAMER  ==========================


      if [ "$MODE_OUTPUT" == "STREAMER" ]; then

        CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf: \
          text=\'$CALL\': fontcolor=white: fontsize=36: box=1: boxcolor=black@0.5: \
          boxborderw=5: x=w/20: y=(h/8-text_h)/2,"

        # Audio does not yet work
        $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
          -f v4l2 \
          -i $VID_WEBCAM \
          -c:v h264_omx -b:v 1024k \
          -vf "$CAPTION"scale="$SCALE",fps=30 -g 25 \
          -f flv $STREAM_URL/$STREAM_KEY &

        exit

      fi

  #==================================== HDMI INPUT FROM CAM LINK 4K.  OUTPUT TO LIME  ==========================

      # Increase audio output bitrate for 1MS transmissions
      if [ "$BITRATE_TS" -gt 1000000 ]; then
        BITRATE_AUDIO=48000
      fi

      # Set up means to transport of stream out of unit
      case "$MODE_OUTPUT" in
        "IP")
          OUTPUT_FILE=""
        ;;
        "DATVEXPRESS")
          echo "set ptt tx" >> /tmp/expctrl
          sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
        ;;
        "LIMEMINI" | "LIMEUSB" | "LIMEDVB")
          $PATHRPI"/limesdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q $CAL $CUSTOM_FPGA -D $DIGITAL_GAIN -e $BAND_GPIO $LIMETYPE &
        ;;
        "MUNTJAC")
          $PATHRPI"/muntjacsdr_dvb" -i videots -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
            -t "$FREQ_OUTPUT"e6 -g $MUNTJAC_GAINF -e $BAND_GPIO &
        ;;
        *)
          sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
        ;;
      esac

    # Now generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************

      sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $VID_WEBCAM -p $PIDPMT -s $CALL $OUTPUT_IP \
         > /dev/null &

    else

      # ******************************* H264 VIDEO WITH AUDIO ************************************

      if [ "$AUDIO_PREF" == "video" ]; then              # EasyCap

        #echo EASYCAP AUDIO ===================================

        AUDIO_SAMPLE=48000
        AUDIO_CHANNELS=2

        #arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

        arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
         | sox -c $AUDIO_CHANNELS --buffer 1024 -t wav - audioin.wav rate 48000 &  

        sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
          -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $VID_WEBCAM -p $PIDPMT -s $CALL $OUTPUT_IP \
          -a audioin.wav -z $BITRATE_AUDIO > /dev/null &

      else                           # CamLink Audio

        #echo CAM LINK HDMI AUDIO ===================================

        AUDIO_SAMPLE=48000
        AUDIO_CHANNELS=2

        # Resample the audio (was 32k or 48k which overruns, so this is reduced to 46500)

        arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
         | sox -c $AUDIO_CHANNELS --buffer 1024 -t wav - audioin.wav rate 48000 &  

        sudo $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -d 300 -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
          -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $VID_WEBCAM -p $PIDPMT -s $CALL $OUTPUT_IP \
          -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
      fi

      # Auto restart for arecord (which dies after about an hour)  in repeater TX modes
      while [[ "$MODE_STARTUP" == "TX_boot" || "$MODE_STARTUP" == "Keyed_TX_boot" ]]
      do
        sleep 10
        if ! pgrep -x "arecord" > /dev/null; then # arecord is not running, so restart it
          arecord -f S16_LE -r $AUDIO_SAMPLE -c 2 -B $ARECORD_BUF -D plughw:$AUDIO_CARD_NUMBER,0 \
           | sox -c $AUDIO_CHANNELS --buffer 1024 -t wav - audioin.wav rate 48000 &  
        fi
      done
    fi
  ;;
esac

#=================================== JETSON CODE ==========================================================

# None of these input modes were valid for the Jetson, so we can source the Jetson code here

if [ "$MODE_OUTPUT" == "JLIME" ]; then
  source /home/pi/rpidatv/scripts/a_jetson.sh
fi

if [ "$MODE_OUTPUT" == "JSTREAM" ]; then
  source /home/pi/rpidatv/scripts/a_jstream.sh
fi

# ============================================ END =============================================================

# flow exits from a.sh leaving ffmpeg or avc2ts and rpidatv running
# these processes are killed by menu.sh, rpidatvgui4 or b.sh on selection of "stop transmit"
