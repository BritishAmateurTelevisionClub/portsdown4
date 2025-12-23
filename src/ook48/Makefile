all: wmaser

CFLAGS	= -Wall -g -O2 -Wno-unused-variable
LDFLAGS	= -lm -lrt -lpthread  -lwiringPi

wmaser : adf4351.h adf4351.c ook48.c
		$(CC) $(CFLAGS) -o ook48 adf4351.c timing.c ook48.c $(LDFLAGS) 
clean:
	rm -f  *.o




