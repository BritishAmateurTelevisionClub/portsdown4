all:	siggentouch4

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread  -ljpeg -lrt -lm -ldl -lfftw3f -lwiringPi -liio -lLimeSuite -lfcgi -rdynamic

siggentouch4: siggentouch4.c 
	gcc -Wall $(INCLUDEFLAGS2) -o  siggen \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_24.c \
		font/dejavu_sans_26.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_30.c \
		font/dejavu_sans_32.c \
		font/dejavu_sans_72.c \
		cmt2119a.c \
		ffunc.c \
		ffunc.h \
		siggentouch4.c $(LIBFLAGS2) 

install:
	cp siggen ../../bin/