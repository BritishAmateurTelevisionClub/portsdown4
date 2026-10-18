#include <stdio.h>
#include <stdlib.h>
#include <iio.h>
#include <signal.h>
#include <memory.h>

#include "dvb_t.h"
#ifdef __USE_PLUTO__
#include "pluto.h"


/* IIO structs required for streaming */
static struct iio_context *m_ctx = NULL;
static struct iio_channel *m_rx0_i = NULL;
static struct iio_channel *m_rx0_q = NULL;
static struct iio_channel *m_tx0_i = NULL;
static struct iio_channel *m_tx0_q = NULL;
static struct iio_buffer  *m_rxbuf = NULL;
static struct iio_buffer  *m_txbuf = NULL;
// Streaming devices
static struct iio_device *m_tx = NULL;
static struct iio_device *m_rx = NULL;
static struct iio_device *m_dev_ph = NULL;

static uint32_t m_tx_rx_block_size;
// Is it running ?
static bool m_pluto_running;
static bool stop;
/* static scratch mem for strings */
static char tmpstr[64];
static char error_string[256];

#define FMC_ERROR(expr,error_s) { \
	if (!(expr)) { \
		(void) sprintf(error_string, "errorion failed (%s:%d)\n", __FILE__, __LINE__); \
		 return -1; \
	} \
}
/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

/* RX is input, TX is output */
enum iodev { RX, TX };

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
};
/* cleanup and exit */
void pluto_close(void)
{
	if (m_pluto_running == true) {
		m_pluto_running = false;

		if (m_ctx != NULL) {
			//printf("* Destroying buffers\n");
			if (m_rxbuf) { iio_buffer_destroy(m_rxbuf); m_rxbuf = NULL; }
			if (m_txbuf) { iio_buffer_destroy(m_txbuf); m_txbuf = NULL; }

			//printf("* Disabling streaming channels\n");
			if (m_rx0_i) { iio_channel_disable(m_rx0_i); }
			if (m_rx0_q) { iio_channel_disable(m_rx0_q); }
			if (m_tx0_i) { iio_channel_disable(m_tx0_i); }
			if (m_tx0_q) { iio_channel_disable(m_tx0_q); }

			//printf("* Destroying context\n");
			if (m_ctx) { iio_context_destroy(m_ctx); }
		}
	}
}


static void handle_sig(int sig)
{
	printf("Waiting for process to finish...\n");
	stop = true;
}

/* check return value of attr_write function */
static void errchk(int v, const char* what) {
	if (v < 0) { fprintf(stderr, "FMC_ERROR %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what); pluto_close(); }
}
/* read attribute: long long int */
static void rd_ch_lli(struct iio_channel *chn, const char* what, long long *val)
{
	errchk(iio_channel_attr_read_longlong(chn, what, val), what);
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	errchk(iio_channel_attr_write(chn, what, str), what);
}

/* helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
	snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
	return tmpstr;
}

/* returns ad9361 phy device */
static struct iio_device* get_ad9361_phy(struct iio_context *ctx)
{
	struct iio_device *dev = iio_context_find_device(ctx, "ad9361-phy");
//	FMC_ERROR(dev && "No ad9361-phy found");
	return dev;
}
/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
	switch (d) {
	case TX: *dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc"); return *dev != NULL;
	case RX: *dev = iio_context_find_device(ctx, "cf-ad9361-lpc");  return *dev != NULL;
	default: FMC_ERROR(0,""); return false;
	}
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
	if (!*chn)
		*chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
	switch (d) {
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), true);  return *chn != NULL;
	default: FMC_ERROR(0,""); return false;
	}
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn)
{
	switch (d) {
		// LO chan is always output, i.e. true
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 0), true); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 1), true); return *chn != NULL;
	default: FMC_ERROR(0,""); return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad9361_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	// Configure phy and lo channels
	printf("* Acquiring AD9361 phy channel %d\n", chid);
	if (!get_phy_chan(ctx, type, chid, &chn)) { return false; }
	wr_ch_str(chn, "rf_port_select", cfg->rfport);
	wr_ch_lli(chn, "rf_bandwidth", cfg->bw_hz);
	wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);

	// Configure LO channel
	printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
	if (!get_lo_chan(ctx, type, &chn)) { return false; }
	wr_ch_lli(chn, "frequency", cfg->lo_hz);
	return true;
}
static uint32_t m_tx_max_len; // Maximum size of the buffer
static uint32_t m_tx_offset;  // Current offset into the buffer

void pluto_set_tx_buff_length(uint32_t len) {
	// Change the size of the buffer
	m_tx_max_len = len;
	if (m_txbuf) { iio_buffer_destroy(m_txbuf); m_txbuf = NULL; }
	m_txbuf = iio_device_create_buffer( m_tx, len, false);
	scmplx *s_b = (scmplx*)iio_buffer_first(m_txbuf, m_tx0_i);
	memset(s_b, 0, sizeof(scmplx)*len);
	iio_buffer_set_blocking_mode(m_txbuf, true);
	m_tx_offset = 0;
}
static uint32_t m_rx_max_len; // Maximum size of the buffer
static uint32_t m_rx_offset;  // Current offset into the buffer
void pluto_set_rx_buff_length(uint32_t len) {
	// Change the size of the buffer
	m_rx_max_len = len;
	if (m_rxbuf) { iio_buffer_destroy(m_rxbuf); m_rxbuf = NULL; }
	m_rxbuf = iio_device_create_buffer( m_rx, len, false);
	iio_buffer_first(m_rxbuf, m_rx0_i);
//	scmplx *s_b = (scmplx*)iio_buffer_first(m_rxbuf, m_rx0_i);
//	memset(s_b, 0, sizeof(scmplx)*len);
	iio_buffer_set_blocking_mode(m_rxbuf, true);
	m_rx_offset = 0;
}
void pluto_load_tx_filter(short *fir, int taps, int ratio, bool enable) {
	if (m_pluto_running == false) return;
	if (m_ctx == NULL) return;
	int res = 0;
	struct iio_channel *chn = NULL;
	iio_device* dev = NULL;
	dev = get_ad9361_phy(m_ctx);
	int buffsize = 8192;
	char *buf = (char *)malloc(buffsize);
	int clen = 0;
	clen += sprintf(&buf[clen], "RX 3 GAIN 0 DEC %d\n", ratio);
	clen += sprintf(&buf[clen], "TX 3 GAIN 0 INT %d\n", ratio);
	for (int i = 0; i < taps; i++) clen += sprintf(&buf[clen], "%d,%d\n", fir[i], fir[i]);
	//for (int i = 0; i < taps; i++) clen += sprintf_s(buf + clen, buffsize - clen, "%d\n", fir[i]);
	clen += sprintf(&buf[clen], "\n");
	res = iio_device_attr_write_raw(dev, "filter_fir_config", buf, clen);

	chn = iio_device_find_channel(dev, "voltage0", true);
	res = iio_channel_attr_write_bool(chn, "filter_fir_en", true);
	chn = iio_device_find_channel(dev, "voltage0", false);
	res = iio_channel_attr_write_bool(chn, "filter_fir_en", true);

    res = res + 1;  // dummy statement to clear "variable not used" warning
	//	if (!get_phy_chan(m_ctx, RX, 0, &chn)) { return; }
	free(buf);
}
void pluto_load_tx_lpf_filter(float roff) {
	if (m_pluto_running == false) return;
//	int16_t *fir = lpf_make_filter(roff, 4, 128);
//	pluto_load_tx_filter(fir, 128, 4, true);
}
void pluto_configure_x8_int_dec(long long int  sr) {
	if (m_pluto_running == false) return;
	if (m_ctx == NULL) return;
	iio_device* dev = NULL;
	struct iio_channel *chn = NULL;
	// Receive
	dev = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
	if (dev == NULL) return;
	chn = iio_device_find_channel(dev, "voltage0", false);
	wr_ch_lli(chn, "sampling_frequency", sr);
	// Transmit
	dev = iio_context_find_device(m_ctx, "cf-ad9361-dds-core-lpc");
	if (dev == NULL) return;
	chn = iio_device_find_channel(dev, "voltage0", true);
	wr_ch_lli(chn, "sampling_frequency", sr);
}

int pluto_open(DVBTFormat *fmt, uint32_t block_size){
	if (m_pluto_running == true) return 0;

	// Stream configurations
	struct stream_cfg rxcfg;
	struct stream_cfg txcfg;

	// Listen to ctrl+c and FMC_ERROR
	signal(SIGINT, handle_sig);


	// RX stream config
	rxcfg.bw_hz = MHZ(2);   // 2 MHz rf bandwidth
	rxcfg.fs_hz = MHZ(3.072);   // 3.072 MS/s rx sample rate
	rxcfg.lo_hz = GHZ(1.3); // 1.3 GHz rf frequency
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

    // TX stream config
	txcfg.bw_hz = MHZ(2); // 2 MHz rf bandwidth
	txcfg.fs_hz = MHZ(3.072);   // 3.072 MS/s tx sample rate
	txcfg.lo_hz = GHZ(1.3); // 1.3 GHz rf frequency
	txcfg.rfport = "A"; // port A (select for rf freq.)

	m_tx_rx_block_size = block_size;

	if ((m_ctx = iio_create_network_context(fmt->n_addr)) == NULL) {
	    printf("Adalm-Pluto NOT found\n");
	    m_pluto_running = false;
	    return -1;
	}

	if (iio_context_get_devices_count(m_ctx) <= 0) {
	    printf("No devices found\n");
		return -1;
	}
	printf("* Acquiring AD9361 streaming devices\n");
	FMC_ERROR(get_ad9361_stream_dev(m_ctx, RX, &m_rx),"No rx dev found");
	FMC_ERROR(get_ad9361_stream_dev(m_ctx, TX, &m_tx),"No tx dev found");

	printf("* Configuring AD9361 for streaming\n");
	FMC_ERROR(cfg_ad9361_streaming_ch(m_ctx, &rxcfg, RX, 0),"RX port 0 not found");
	FMC_ERROR(cfg_ad9361_streaming_ch(m_ctx, &txcfg, TX, 0),"TX port 0 not found");

	printf("* Initializing AD9361 IIO streaming channels\n");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, RX, m_rx, 0, &m_rx0_i),"RX chan i not found");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, RX, m_rx, 1, &m_rx0_q),"RX chan q not found");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, TX, m_tx, 0, &m_tx0_i),"TX chan i not found");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, TX, m_tx, 1, &m_tx0_q),"TX chan q not found");

	iio_channel_enable(m_tx0_i);
	iio_channel_enable(m_tx0_q);
	iio_channel_enable(m_rx0_i);
	iio_channel_enable(m_rx0_q);

	iio_context_set_timeout(m_ctx, 0);
	pluto_set_tx_buff_length(block_size);
	pluto_set_rx_buff_length(block_size);
	iio_device_set_kernel_buffers_count(m_tx, 4);
	iio_device_set_kernel_buffers_count(m_rx, 4);
	// Enable manual GPIO
	m_dev_ph =  get_ad9361_phy(m_ctx);
	iio_device_debug_attr_write_bool(m_dev_ph,"adi,gpo-manual-mode-enable",1);
    iio_device_debug_attr_write(m_dev_ph,"direct_reg_access","0x26 0x10");

	// Load an FIR interpolating filter to allow low sample rates
    m_pluto_running = true;
	pluto_load_tx_lpf_filter(0.4);
	printf("Adalm-Pluto Initialised\n");
	return 0;
}
int pluto_start_tx_stream(void){
	printf("* Initializing AD9361 IIO TX streaming channels\n");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, TX, m_tx, 0, &m_tx0_i),"TX chan i not found");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, TX, m_tx, 1, &m_tx0_q),"TX chan q not found");
	iio_channel_enable(m_tx0_i);
	iio_channel_enable(m_tx0_q);
	pluto_set_tx_buff_length(m_tx_rx_block_size);

    //power on TX local oscillator
	struct iio_channel *chn = NULL;
	if (!get_lo_chan(m_ctx, TX, &chn)) { return 1; }
    iio_channel_attr_write_bool(chn, "powerdown", false);

    pluto_transmit();  // Pluto PTT on
	return 0;
}

void pluto_stop_tx_stream(void){
	printf("* Disabling AD9361 IIO TX streaming channels\n");
	if (m_tx0_i) { iio_channel_disable(m_tx0_i); }
	if (m_tx0_q) { iio_channel_disable(m_tx0_q); }

    //power off TX local oscillator
	struct iio_channel *chn = NULL;
	if (!get_lo_chan(m_ctx, TX, &chn)) { return; }
	iio_channel_attr_write_bool(chn, "powerdown", true);

    pluto_receive();  // Pluto PTT off
}

int pluto_start_rx_stream(void){
	printf("* Initializing AD9361 IIO streaming channels\n");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, RX, m_rx, 0, &m_rx0_i),"RX chan i not found");
	FMC_ERROR(get_ad9361_stream_ch(m_ctx, RX, m_rx, 1, &m_rx0_q),"RX chan q not found");
	iio_channel_enable(m_rx0_i);
	iio_channel_enable(m_rx0_q);
	pluto_set_rx_buff_length(m_tx_rx_block_size);
    return 0;
}
void pluto_stop_rx_stream(void){
	printf("* Disabling AD9361 IIO RX streaming channels\n");
	if (m_rx0_i) { iio_channel_disable(m_rx0_i); }
	if (m_rx0_q) { iio_channel_disable(m_rx0_q); }
}
void pluto_set_tx_level(double level) {
	if (m_pluto_running == false) return;
	if (m_ctx == NULL) return;
	if (level > 0) level = 0;
	if (level < -89) level = -89;

	struct iio_channel *chn = NULL;
	if (!get_phy_chan(m_ctx, TX, 0, &chn)) { return; }
	wr_ch_lli(chn, "hardwaregain", (long long int)level);
}
void pluto_set_rx_level(double level) {
	if (m_pluto_running == false) return;
	if (m_ctx == NULL) return;
//	if (level > 0) level = 0;
//	if (level < -89) level = -89;

	struct iio_channel *chn = NULL;
	if (!get_phy_chan(m_ctx, RX, 0, &chn)) { return; }
	wr_ch_lli(chn, "hardwaregain", (long long int)level);
}

void pluto_set_tx_frequency(long long int freq) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_lo_chan(m_ctx, TX, &chn)) { return; }
	wr_ch_lli(chn, "frequency", freq);
}
void pluto_set_rx_frequency(long long int freq) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_lo_chan(m_ctx, RX, &chn)) { return; }
	wr_ch_lli(chn, "frequency", freq);
}
void pluto_get_tx_frequency(long long int *freq) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_lo_chan(m_ctx, TX, &chn)) { return; }
	rd_ch_lli(chn, "frequency", freq);
}
void pluto_get_rx_frequency(long long int *freq) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_lo_chan(m_ctx, RX, &chn)) { return; }
	rd_ch_lli(chn, "frequency", freq);
}

void pluto_set_tx_sample_rate( long int sr) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_phy_chan(m_ctx, TX, 0, &chn)) { return; }
	wr_ch_lli(chn, "sampling_frequency", (long long int)sr);
}

void pluto_set_rx_sample_rate( long int sr) {
	if (m_pluto_running == false) return;
	struct iio_channel *chn = NULL;
	if (m_ctx == NULL) return;
	if (!get_phy_chan(m_ctx, RX, 0, &chn)) { return; }
	wr_ch_lli(chn, "sampling_frequency", (long long int)sr);
}

void pluto_agc_type(AgcType type){
	if (m_pluto_running == false) return;
	iio_device* dev = NULL;
	struct iio_channel *chn = NULL;
	// Receive
	dev = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
	if (dev == NULL) return;
	chn = iio_device_find_channel(dev, "voltage0", false);
	switch(type){
		case AGC_MANUAL:
			wr_ch_str( chn, "gain_control_mode", "manual");
			break;
		case AGC_SLOW:
			wr_ch_str( chn, "gain_control_mode", "slow_attack");
			break;
		case AGC_FAST:
			wr_ch_str( chn, "gain_control_mode", "fast_attack");
			break;
		case AGC_HYBRID:
			wr_ch_str( chn, "gain_control_mode", "hybrid");
			break;
		default:
			break;
	}
}

//
// Transmit 1.15 samples
//
void pluto_tx_samples( scmplx *s, int len) {
	if (m_pluto_running == false) return;
	if (len == 0) return;
	// Get position of first sample in the buffer
	scmplx *s_b = (scmplx*)iio_buffer_first(m_txbuf, m_tx0_i);

	if ((m_tx_offset + len) > m_tx_max_len) {
		int l = m_tx_max_len - m_tx_offset;
		scmplx *b = (scmplx*)&s_b[m_tx_offset];
		memcpy(b, s, sizeof(scmplx)*l);
		iio_buffer_push(m_txbuf);
		s = &s[l];
		len = len - l;
		m_tx_offset = 0;
	}
	// Calculate the offset for the beginning of the next update
	scmplx *b = (scmplx*)&s_b[m_tx_offset];
	// Copy the Samples into the buffer
	memcpy(b, s, sizeof(scmplx)*len);
	m_tx_offset += len;
}
//
// Transmit FP samples
//
void pluto_tx_samples( fcmplx *s, int len) {
	if (m_pluto_running == false) return;
	if (len == 0) return;
	// Get position of first sample in the buffer
	scmplx *s_b = (scmplx*)iio_buffer_first(m_txbuf, m_tx0_i);

	if ((m_tx_offset + len) > m_tx_max_len) {
		int l = m_tx_max_len - m_tx_offset;
		scmplx *b = (scmplx*)&s_b[m_tx_offset];
		// Convert from FP to Fixed and copy
		for( int i = 0; i < l; i++){
			b[i].re = s[i].re*0x8000;
			b[i].im = s[i].im*0x8000;
		}
		iio_buffer_push(m_txbuf);
		s = &s[l];
		len = len - l;
		m_tx_offset = 0;
	}
	// Calculate the offset for the beginning of the next update
	scmplx *b = (scmplx*)&s_b[m_tx_offset];
	// Copy the Samples into the buffer
	// Convert from FP to Fixed and copy
	for( int i = 0; i < len; i++){
		b[i].re = s[i].re*0x8000;
		b[i].im = s[i].im*0x8000;
	}
	m_tx_offset += len;
}
//
// returns the number of samples received
//
uint32_t pluto_rx_samples( scmplx *s ) {
	if (m_pluto_running == false) return 0;
	// Get the number of bytes that have been received
	uint32_t nbytes = iio_buffer_refill(m_rxbuf);
	// Get position of first sample in the buffer
	scmplx *s_b = (scmplx*)iio_buffer_first(m_rxbuf, m_rx0_i);
    // Copy the bytes
	memcpy(s, s_b, nbytes);
	// return the length
	return nbytes/sizeof(scmplx);// Convert to IQ samples
}
uint32_t pluto_rx_samples( fcmplx *s ) {
	if (m_pluto_running == false) return 0;
	// Get the number of bytes that have been received
	uint32_t nbytes = iio_buffer_refill(m_rxbuf);
	// Get position of first sample in the buffer
	scmplx *s_b = (scmplx*)iio_buffer_first(m_rxbuf, m_rx0_i);
	// Convert from Fixed to Floating and copy
	uint32_t len = nbytes/sizeof(scmplx);
	for( uint32_t i = 0; i < len; i++){
		s[i].re = (float)(s_b[i].re*0.00003);
		s[i].im = (float)(s_b[i].im*0.00003);
	}
	// return the length
	return len;// Convert to IQ samples
}
void pluto_transmit(void){
    iio_device_debug_attr_write(m_dev_ph,"direct_reg_access","0x27 0x10");
}
void pluto_receive(void){
    iio_device_debug_attr_write(m_dev_ph,"direct_reg_access","0x27 0x00");
}
#endif
