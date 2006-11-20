# ---------------------------------------------------------------------------
# Makefile - A makefile for the IRIG 106 Chapter 10 utilities
#
# $RCSfile: Makefile,v $
# $Date: 2006-11-20 04:34:21 $
# $Revision: 1.1 $
# ---------------------------------------------------------------------------

CFLAGS=-I../irig106lib -I-
LIBS=../irig106lib/libirig106.a

idmptmat: idmptmat.o $(LIBS)

i106stat: i106stat.o $(LIBS)

i106trim: i106trim.o $(LIBS)


idmptmat.o: idmptmat.c

i106stat.o: i106stat.c

i106trim.o: i106trim.c