# ---------------------------------------------------------------------------
# Makefile - A makefile for the IRIG 106 Chapter 10 utilities
# ---------------------------------------------------------------------------

SRC_DIR=../src
CFLAGS=-DIRIG_NETWORKING -iquote../../irig106lib/src -ggdb -fpack-struct=1
LIBS=../../irig106lib/gcc/libirig106.a

#all: i106stat i106trim i106vid idmptmat idmp1553 idmpins idmpuart idmpeth idmp429 idmpindex idmptime idmpgps
all: i106stat i106vid i106udprcv idmptmat idmp1553 idmpins idmpuart idmpeth idmp429 idmpindex idmptime idmpgps idmpcan idmppcm idmpanalog

i106stat: $(SRC_DIR)/i106stat.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

i106trim: $(SRC_DIR)/i106trim.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

i106udprcv: $(SRC_DIR)/i106udprcv.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmptmat: $(SRC_DIR)/idmptmat.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

idmp1553: $(SRC_DIR)/idmp1553.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

i106vid: $(SRC_DIR)/i106vid.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

idmpins: $(SRC_DIR)/idmpins.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpuart: $(SRC_DIR)/idmpuart.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpeth: $(SRC_DIR)/idmpeth.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmp429: $(SRC_DIR)/idmp429.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpindex: $(SRC_DIR)/idmpindex.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmptime: $(SRC_DIR)/idmptime.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpgps: $(SRC_DIR)/idmpgps.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpcan: $(SRC_DIR)/idmpcan.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmppcm: $(SRC_DIR)/idmppcm.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

idmpanalog: $(SRC_DIR)/idmpanalog.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -lm -o $@

clean:
	rm i106stat i106vid idmptmat idmp1553 idmpins idmpuart idmpeth idmp429 idmpindex idmptime idmpgps idmpcan idmppcm idmpanalog
