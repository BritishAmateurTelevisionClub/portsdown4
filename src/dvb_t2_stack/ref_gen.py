#!/usr/bin/env python3
"""ref_gen.py - GNU Radio reference IQ for t2mod verification (any GI)."""
import sys, math
from gnuradio import gr, blocks, digital, dtv
ts, out, bw, mod, rate, gi, secs = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4], sys.argv[5], sys.argv[6], float(sys.argv[7])
MODS={"qpsk":(dtv.MOD_QPSK,2),"16qam":(dtv.MOD_16QAM,4),"64qam":(dtv.MOD_64QAM,6)}
RATES={"1/2":dtv.C1_2,"3/5":dtv.C3_5,"2/3":dtv.C2_3,"3/4":dtv.C3_4,"4/5":dtv.C4_5,"5/6":dtv.C5_6}
GIS={"1/4":(dtv.GI_1_4,4,dtv.PILOT_PP1,1522,1136,804),"1/8":(dtv.GI_1_8,8,dtv.PILOT_PP2,1532,1420,1309),
     "1/16":(dtv.GI_1_16,16,dtv.PILOT_PP4,1602,1562,1415),"1/32":(dtv.GI_1_32,32,dtv.PILOT_PP4,1602,1562,1415)}
cm,bpc=MODS[mod]; code=RATES[rate]; g,gd,pp,cd,nfc,cfc=GIS[gi]
fs=131e6/71 if abs(bw-1.7)<1e-6 else bw*8e6/7; FFT=2048; gl=FFT//gd; sym=FFT+gl
nd=max(8,min(int(math.floor((0.25*fs-FFT)/sym))-8,2000))
fb=(8*1118+(nd-1)*cd+nfc-1840-(nfc-cfc)-4000)//(64800//bpc)
tb=gr.top_block()
ch=[blocks.file_source(1,ts,True,0,0),
 dtv.dvb_bbheader_bb(dtv.STANDARD_DVBT2,dtv.FECFRAME_NORMAL,code,dtv.RO_0_35,dtv.INPUTMODE_NORMAL,dtv.INBAND_OFF,fb,4000000),
 dtv.dvb_bbscrambler_bb(dtv.STANDARD_DVBT2,dtv.FECFRAME_NORMAL,code), dtv.dvb_bch_bb(dtv.STANDARD_DVBT2,dtv.FECFRAME_NORMAL,code),
 dtv.dvb_ldpc_bb(dtv.STANDARD_DVBT2,dtv.FECFRAME_NORMAL,code,dtv.MOD_OTHER), dtv.dvbt2_interleaver_bb(dtv.FECFRAME_NORMAL,code,cm),
 dtv.dvbt2_modulator_bc(dtv.FECFRAME_NORMAL,cm,dtv.ROTATION_ON), dtv.dvbt2_cellinterleaver_cc(dtv.FECFRAME_NORMAL,cm,fb,1),
 dtv.dvbt2_framemapper_cc(dtv.FECFRAME_NORMAL,code,cm,dtv.ROTATION_ON,fb,1,dtv.CARRIERS_NORMAL,dtv.FFTSIZE_2K,g,dtv.L1_MOD_BPSK,pp,2,nd,dtv.PAPR_OFF,dtv.VERSION_111,dtv.PREAMBLE_T2_SISO,dtv.INPUTMODE_NORMAL,dtv.RESERVED_OFF,dtv.L1_SCRAMBLED_OFF,dtv.INBAND_OFF),
 dtv.dvbt2_freqinterleaver_cc(dtv.CARRIERS_NORMAL,dtv.FFTSIZE_2K,pp,g,nd,dtv.PAPR_OFF,dtv.VERSION_111,dtv.PREAMBLE_T2_SISO),
 dtv.dvbt2_pilotgenerator_cc(dtv.CARRIERS_NORMAL,dtv.FFTSIZE_2K,pp,g,nd,dtv.PAPR_OFF,dtv.VERSION_111,dtv.PREAMBLE_T2_SISO,dtv.MISO_TX1,dtv.EQUALIZATION_OFF,dtv.BANDWIDTH_1_7_MHZ,FFT),
 digital.ofdm_cyclic_prefixer(FFT,FFT+gl,0,''),
 dtv.dvbt2_p1insertion_cc(dtv.CARRIERS_EXTENDED,dtv.FFTSIZE_2K,g,nd,dtv.PREAMBLE_T2_SISO,dtv.SHOWLEVELS_OFF,3.3),
 blocks.multiply_const_cc(0.2,1), blocks.head(8,int(secs*fs)), blocks.file_sink(8,out,False)]
tb.connect(*ch); tb.run()
