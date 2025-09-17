#ifndef __METEORVIEW_H__
#define __METEORVIEW__


#define TCP_DATA_PRE 32
#define FFT_BUFFER_SIZE 1024
extern unsigned char transmit_buffer[];

void cleanexit(int);
void GetConfigParam(char *PathConfigFile, char *Param, char *Value);
void SetConfigParam(char *PathConfigFile, char *Param, char *Value);

#endif /* __METEORVIEW_H__ */

