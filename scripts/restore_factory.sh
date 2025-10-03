#!/bin/bash

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts

# portsdown_config.txt
cp -f $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_config.txt.factory" $PATHSCRIPT"/portsdown_config.txt"

# portsdown_presets.txt
cp -f $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_presets.txt.factory" $PATHSCRIPT"/portsdown_presets.txt"

# siggencal.txt
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt.bak
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt.factory /home/pi/rpidatv/src/siggen/siggencal.txt

# siggenconfig.txt
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt.bak
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt.factory /home/pi/rpidatv/src/siggen/siggenconfig.txt

# rtl-fm_presets.txt
cp -f $PATHSCRIPT"/rtl-fm_presets.txt" $PATHSCRIPT"/rtl-fm_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/rtl-fm_presets.txt.factory" $PATHSCRIPT"/rtl-fm_presets.txt"

# portsdown_locators.txt
cp -f $PATHSCRIPT"/portsdown_locators.txt" $PATHSCRIPT"/portsdown_locators.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_locators.txt.factory" $PATHSCRIPT"/portsdown_locators.txt"

# rx_presets.txt
cp -f $PATHSCRIPT"/rx_presets.txt" $PATHSCRIPT"/rx_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/rx_presets.txt.factory" $PATHSCRIPT"/rx_presets.txt"

# stream_presets.txt
cp -f $PATHSCRIPT"/stream_presets.txt" $PATHSCRIPT"/stream_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/stream_presets.txt.factory" $PATHSCRIPT"/stream_presets.txt"

# jetson_config.txt
cp -f $PATHSCRIPT"/jetson_config.txt" $PATHSCRIPT"/jetson_config.txt.bak"
cp -f $PATHSCRIPT"/configs/jetson_config.txt.factory" $PATHSCRIPT"/jetson_config.txt"

# longmynd_config.txt
cp -f $PATHSCRIPT"/longmynd_config.txt" $PATHSCRIPT"/longmynd_config.txt.bak"
cp -f $PATHSCRIPT"/configs/longmynd_config.txt.factory" $PATHSCRIPT"/longmynd_config.txt"

# portsdown_C_codes.txt
cp -f $PATHSCRIPT"/portsdown_C_codes.txt" $PATHSCRIPT"/portsdown_C_codes.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_C_codes.txt.factory" $PATHSCRIPT"/portsdown_C_codes.txt"

# bandview_config.txt
cp -f /home/pi/rpidatv/src/bandview/bandview_config.txt /home/pi/rpidatv/src/bandview/bandview_config.txt.bak
cp -f /home/pi/rpidatv/src/bandview/bandview_config.txt.factory /home/pi/rpidatv/src/bandview/bandview_config.txt

# airspyview_config.txt
cp -f /home/pi/rpidatv/src/airspyview/airspyview_config.txt /home/pi/rpidatv/src/airspyview/airspyview_config.txt.bak
cp -f /home/pi/rpidatv/src/airspyview/airspyview_config.txt.factory /home/pi/rpidatv/src/airspyview/airspyview_config.txt

# rtlsdrview_config.txt
cp -f /home/pi/rpidatv/src/rtlsdrview/rtlsdrview_config.txt /home/pi/rpidatv/src/rtlsdrview/rtlsdrview_config.txt.bak
cp -f /home/pi/rpidatv/src/rtlsdrview/rtlsdrview_config.txt.factory /home/pi/rpidatv/src/rtlsdrview/rtlsdrview_config.txt

# plutoview_config.txt
cp -f /home/pi/rpidatv/src/plutoview/plutoview_config.txt /home/pi/rpidatv/src/plutoview/plutoview_config.txt.bak
cp -f /home/pi/rpidatv/src/plutoview/plutoview_config.txt.factory /home/pi/rpidatv/src/plutoview/plutoview_config.txt

# sdrplayview_config.txt
cp -f /home/pi/rpidatv/src/sdrplayview/sdrplayview_config.txt /home/pi/rpidatv/src/sdrplayview/sdrplayview_config.txt.bak
cp -f /home/pi/rpidatv/src/sdrplayview/sdrplayview_config.txt.factory /home/pi/rpidatv/src/sdrplayview/sdrplayview_config.txt

# meteorview_config.txt
cp -f /home/pi/rpidatv/src/meteorview/meteorview_config.txt /home/pi/rpidatv/src/meteorview/meteorview_config.txt.bak
cp -f /home/pi/rpidatv/src/meteorview/meteorview_config.txt.factory /home/pi/rpidatv/src/meteorview/meteorview_config.txt

# nf_meter_config.txt
cp -f /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt.bak
cp -f /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt.factory /home/pi/rpidatv/src/nf_meter/nf_meter_config.txt

# pluto_nf_meter_config.txt
cp -f /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt.bak
cp -f /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt.factory /home/pi/rpidatv/src/pluto_nf_meter/pluto_nf_meter_config.txt

# noise_meter_config.txt
cp -f /home/pi/rpidatv/src/noise_meter/noise_meter_config.txt /home/pi/rpidatv/src/noise_meter/noise_meter_config.txt.bak
cp -f /home/pi/rpidatv/src/noise_meter/noise_meter_config.txt.factory /home/pi/rpidatv/src/noise_meter/noise_meter_config.txt

# pluto_noise_meter_config.txt
cp -f /home/pi/rpidatv/src/pluto_noise_meter/pluto_noise_meter_config.txt /home/pi/rpidatv/src/pluto_noise_meter/pluto_noise_meter_config.txt.bak
cp -f /home/pi/rpidatv/src/pluto_noise_meter/pluto_noise_meter_config.txt.factory /home/pi/rpidatv/src/pluto_noise_meter/pluto_noise_meter_config.txt

# sweeper_config.txt
cp -f /home/pi/rpidatv/src/sweeper/sweeper_config.txt /home/pi/rpidatv/src/sweeper/sweeper_config.txt.bak
cp -f /home/pi/rpidatv/src/sweeper/sweeper_config.txt.factory /home/pi/rpidatv/src/sweeper/sweeper_config.txt

# merger_config.txt
cp -f $PATHSCRIPT"/merger_config.txt" $PATHSCRIPT"/merger_config.txt.bak"
cp -f $PATHSCRIPT"/configs/merger_config.txt.factory" $PATHSCRIPT"/merger_config.txt"

