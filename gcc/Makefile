# ---------------------------------------------------------------------------
# Makefile - A makefile for the IRIG 106 Chapter 10 utilities
#
# $RCSfile: Makefile,v $
# $Date: 2006-12-10 01:15:33 $
# $Revision: 1.2 $
# ---------------------------------------------------------------------------

CFLAGS=-I../irig106lib -I-
LIBS=../irig106lib/libirig106.a

idmptmat: idmptmat.o $(LIBS)

idmp1553: idmp1553.o $(LIBS)

i106stat: i106stat.o $(LIBS)

i106trim: i106trim.o $(LIBS)


idmptmat.o: idmptmat.c

idmp1553.o: idmptmat.c

i106stat.o: i106stat.c

i106trim.o: i106trim.c