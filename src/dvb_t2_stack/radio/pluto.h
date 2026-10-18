//
// pluto access library
// G4GUO
//
#ifndef __PLUTO_H__
#define __PLUTO_H__

#include <stdint.h>
#include "dvb_t.h"

typedef enum{
	AGC_MANUAL, AGC_SLOW, AGC_FAST, AGC_HYBRID
}AgcType;

int pluto_open(DVBTFormat *fmt, uint32_t block_size);
void pluto_close(void);
int pluto_start_tx_stream(void);
void pluto_stop_tx_stream(void);
int pluto_start_rx_stream(void);
void pluto_stop_rx_stream(void);
void pluto_set_tx_level(double level);
void pluto_set_rx_level(double level);
void pluto_agc_type(AgcType type);
void pluto_set_tx_frequency(long long int freq);
void pluto_set_rx_frequency(long long int freq);
void pluto_get_tx_frequency(long long int *freq);
void pluto_get_rx_frequency(long long int *freq);
void pluto_set_tx_sample_rate( long int sr);
void pluto_set_rx_sample_rate( long int sr);
void pluto_configure_x8_int_dec(long long int  sr);

uint32_t pluto_rx_samples( scmplx *s );
uint32_t pluto_rx_samples( fcmplx *s );
void pluto_tx_samples( scmplx *s, int len);

void pluto_transmit(void);
void pluto_receive(void);

#endif
