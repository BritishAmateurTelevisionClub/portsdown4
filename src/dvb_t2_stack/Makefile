# dvb_t2_stack / t2mod - narrowband DVB-T2 for the Portsdown (G8YTZ, GPLv3)
#   make            builds t2mod and dvb_t2_stack (needs libfftw3-dev, libiio, LimeSuite)
#   make PI4=1      Raspberry Pi 4 tuned (32-bit Raspbian/Buster, e.g. Portsdown 4)
#   make NO_PLUTO=1 / NO_LIME=1   leave a radio out
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -pthread
CPPFLAGS += -Ishim -Isrc -I/usr/local/include
LDFLAGS  += -L/usr/local/lib
ifdef PI4
CXXFLAGS += -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard
endif
RADIO_OBJS =
RADIO_LIBS =
ifndef NO_LIME
RADIO_OBJS += radio/lime.o
RADIO_LIBS += -lLimeSuite
else
CPPFLAGS += -DNO_LIME
endif
ifndef NO_PLUTO
RADIO_OBJS += radio/pluto.o
RADIO_LIBS += -liio
else
CPPFLAGS += -DNO_PLUTO
endif
DTV_OBJS = $(patsubst %.cc,%.o,$(wildcard src/dvb/*.cc) $(wildcard src/dvbt2/*.cc))

all: t2mod dvb_t2_stack

t2mod: t2mod.o t2core.o $(DTV_OBJS)
	$(CXX) -pthread -o $@ $^ $(LDFLAGS) -lfftw3f -latomic

dvb_t2_stack: dvb_t2_stack.o t2core.o $(DTV_OBJS) $(RADIO_OBJS)
	$(CXX) -pthread -o $@ $^ $(LDFLAGS) -lfftw3f $(RADIO_LIBS) -latomic

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

radio/%.o: radio/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Iradio -c $< -o $@

clean:
	rm -f *.o src/dvb/*.o src/dvbt2/*.o radio/*.o t2mod dvb_t2_stack
