# ---------------------------------------------------------------------------
# Makefile - A makefile for the IRIG 106 Chapter 10 utilities
# ---------------------------------------------------------------------------

SRC_DIR=../src
CFLAGS=-I../../irig106lib/src -I- -g
LIBS=../../irig106lib/gcc/libirig106.a

all: i106stat i106trim i106vid idmptmat idmp1553 idmpins idmpuart

i106stat: $(SRC_DIR)/i106stat.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

i106trim: $(SRC_DIR)/i106trim.c $(LIBS)
	cc $(CFLAGS) $< $(LIBS) -o $@

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


