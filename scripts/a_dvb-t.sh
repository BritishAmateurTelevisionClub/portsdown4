# This script is called from a.sh when DVB-T modulation is required
# Control is returned to a.sh after the calculations

# First, look up the guard interval and QAM
GUARD=$(get_config_var guard $PCONFIGFILE)
QAM=$(get_config_var qam $PCONFIGFILE)

# Calculate the max bitrate
# bitrate = 423/544 * bandwidth * FEC * (bits per symbol) * (Guard Factor)

case "$QAM" in
  "qpsk" )
    BITSPERSYMBOL="2"
    CONSTLN="qpsk"
  ;;
  "16qam" )
    BITSPERSYMBOL="4"
    CONSTLN="16qam"
  ;;
  "64qam" )
    BITSPERSYMBOL="6"
    CONSTLN="64qam"
  ;;
  * )
    BITSPERSYMBOL="2"
    CONSTLN="qpsk"
  ;;
esac

let GUARDDEN=$GUARD+1

# Top line first
let BITRATE_TS=423*$SYMBOLRATE
let BITRATE_TS=$BITRATE_TS*$FECNUM
let BITRATE_TS=$BITRATE_TS*$BITSPERSYMBOL
let BITRATE_TS=$BITRATE_TS*$GUARD

# Now divide (smallest first)
let BITRATE_TS=$BITRATE_TS/$FECDEN

let BITRATE_TS=$BITRATE_TS/$GUARDDEN
let BITRATE_TS=$BITRATE_TS/544

if [ "$MODULATION" == "DVB-T2" ]; then
  BITRATE_TS=$(/home/pi/rpidatv/bin/dvb_t2_stack -c -m $CONSTLN -b $SYMBOLRATE -e "$FECNUM"/"$FECDEN" -g 1/"$GUARD")
  echo "DVB-T2 TS bitrate calculated as "$BITRATE_TS
else
  echo "Full DVB-T TS bitrate calculated as "$BITRATE_TS
fi

# Now apply margin % for non-CBR encoding (Vary for modulator and encoder)

MARGIN=100           # 

let BITRATE_TS=$BITRATE_TS*$MARGIN
let BITRATE_TS=$BITRATE_TS/100

# Adjust Video bitrate  and image size based on TS Bitrate

  if [ "$AUDIO_CHANNELS" != 0 ]; then                 # H264 with AAC audio
    ARECORD_BUF=55000     # arecord buffer in us
    BITRATE_AUDIO=24000
    let TS_AUDIO_BITRATE=$BITRATE_AUDIO*15/10
    let BITRATE_VIDEO=($BITRATE_TS-24000-$TS_AUDIO_BITRATE)*725/1000

  else                                                # H264 or H265 no audio
    let BITRATE_VIDEO=($BITRATE_TS-12000)*725/1000
  fi

  # Set the H264 image size
  if [ "$BITRATE_VIDEO" -lt 190000 ]; then  # less than 333KS FEC 1/2
    VIDEO_WIDTH=352
    VIDEO_HEIGHT=288
    VIDEO_FPS=25
  fi
  if [ "$BITRATE_VIDEO" -lt 100000 ]; then
    VIDEO_WIDTH=160
    VIDEO_HEIGHT=120
  fi


