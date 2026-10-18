#ifndef __LIME_H__
#define __LIME_H__

#include <stdint.h>
#include "dvb_t.h"

// Lime
int  lime_open(DVBTFormat *fmt);
void lime_close(void);
void lime_transmit_samples( scmplx *in, int len);
void lime_set_gain(float gain);
void lime_set_freq(uint64_t freq);
void lime_set_port(int port);
void lime_receive(void);
void lime_transmit(void);

#endif
