/*
 * dvb_t.h - compatibility header so the Portsdown dvb_t_stack Lime and Pluto
 * drivers (lime.cpp, pluto.cpp by Charles Brain G4GUO, Portsdown 4, GPLv3)
 * build unchanged inside dvb_t2_stack. Only the types they use are defined.
 */
#ifndef __DVB_T_H__
#define __DVB_T_H__
#include <stdint.h>
#ifndef NO_LIME
#define __USE_LIME__
#endif
#ifndef NO_PLUTO
#define __USE_PLUTO__
#endif
typedef struct { float re; float im; } fcmplx;
typedef struct { short re; short im; } scmplx;
typedef enum { R_PLUTO, R_LIME, R_EXPRESS } RadioType;
typedef struct {
    uint8_t co, sf, gi, tm, fec, ir;
    uint32_t chan_bw_hz;
    uint32_t tx_sample_rate;
    uint32_t chan_capacity;
    char     n_addr[32];
    uint64_t freq;
    float level;
    uint16_t port;
    double sr;
    int br;
    RadioType radio;
} DVBTFormat;
#endif
