==================
IRIG 106 Utilities
==================

Copyright (C) 2010 Irig106.org

Introduction
------------

These programs are relatively simple utility programs for reading and
manipulating IRIG 106 format data files.  They are run from a command line
and are invoked with various command line parameters.  Run just the command
with no parameters to get a brief summary of available command line parameters.


I106STAT
--------

Generate a summary of data channels and message types found in an IRIG 106
data file.

Usage: i106stat <input file> <output file> [flags]
   <filename> Input/output file names
   -r         Log both sides of RT to RT transfers
   -v         Verbose


I106TRIM
--------

Trim a data file based on start and/or stop time, or file offset. It 
is assumed the day is the same as the starting day.  If the stop time 
is less than the start time, it is assumed that the time spans 
midnight. In this case stop time is the next day.

There are some new requirements for modifying or adding TMATS records when
rewriting data files. This utility makes no attempt to modify TMATS and so
technically may result in a non-IRIG106 compliant data file.

Usage: i106trim <infile> <outfile> [+hh:mm:ss] [-hh:mm:ss]
  +hh:mm:ss - Start copy time
  -hh:mm:ss - Stop copy time
  +<num>%   - Start copy at position <num> percent into the file
  -<num>%   - Stop copy at position <num> percent into the file

Or:    fftrim <infile> to get stats


IDMPTMATS
---------

Read and print out the TMATS record in various formats

Usage: idmptmat <infile> <outfile> <flags>
  -c      Output channel summary format (default)
  -t      Output tree view format
  -r      Output raw TMATS


IDMP1553
--------

Read and dump 1553 messages in a humanly readable format. With no command line
parameters, all 1553 messages are dumped.  Output can be filtered by Channel ID, 
RT number, T/R bit, and Subaddress number.  Output can be further limited by
a decimation factor, useful for thing like INS data.

Usage: idmp1553 <input file> <output file> [flags]
   <filename> Input/output file names
   -v         Verbose
   -c ChNum   Channel Number (default all)
   -r RT      RT Address(1-30) (default all)
   -t T/R     T/R Bit (0=R 1=T) (default all)
   -s SA      Subaddress (default all)
   -d Num     Dump 1 in 'Num' messages
   -i         Dump data as decimal integers
   -u         Dump status response

   -T         Print TMATS summary and exit

The output data fields are:
  Time Bus RT T/R SA WC Errs Data...

Error Bits:
  0x01    Word Error
  0x02    Sync Error
  0x04    Word Count Error
  0x08    Response Timeout
  0x10    Format Error
  0x20    Message Error
  0x80    RT to RT

IDMPINS
-------

Read and dump recorded INS data from F-16, C-130, and A-10 EGI aircraft in
a humanly (and Microsoft Excel) readable form.  On the command line specify
channel number, RT address, TR bit, and Subaddress of the INS message.  Use
the -d flag to reduce the number of dumped INS points.  Currently two types
of INS messages are supported, the EGI INS unit commonly found on F-16, A-10,
and C-130 aircraft, and the F-15 INS message type.  Use the -a flag to output
additional aircraft attitude data.

The -g flag can be used to specify a reference position on the ground. More
than one reference position can be specified by using multiple -g flags.
When one or more reference positions are defined, the INS output will also
include position and attitude information relative to that point.  The -m 
flag can be used to only dump INS data points when within a prescribed 
distance from the reference point.

The -T flag can be used to dump a brief TMATS summary of the data file to
help in choosing command line parameters.

Usage: idmpins <input file> <output file> [flags]
   <filename>       Input/output file names
   -v               Verbose
   -a               Dump aircraft INS attitude
   -c Bus           IRIG Channel Number
   -r RT            INS RT Address(1-30) (default 6)
   -t T/R           INS T/R Bit (0=R 1=T) (default 1)
   -s SA            INS Message Subaddress (default 16)
   -d Num           Dump 1 in 'Num' messages (default all)
   -i Type          INS Type (default 1)
                      1 = F-16/C-130/A-10 EGI
                      2 = F-15
   -g Lat Lon Elev  Ground target position (ft)
   -m Dist          Only dump within this many nautical miles
                      of ground target position
   -T               Print TMATS summary and exit

IDMPGPS
-------

Read GPS NMEA strings from a UART channel.  NMEA GPGGA and GPRMC sentences
are supported, and it is assumed both are available.  Use the "-G" flag to
restrict output to just those data fields supported by GPGGA.  Use the "-C"
flag to restrict output to data fields supported by GPRMC.

The -g flag can be used to specify a reference position on the ground. More
than one reference position can be specified by using multiple -g flags.
When one or more reference positions are defined, the GPS output will also
include position and attitude information relative to that point.  The -m 
flag can be used to only dump GPS data points when within a prescribed 
distance from the reference point.

Usage: idmpgps <input file> <output file> [flags]
   <filename> Input/output file names
   -v               Verbose
   -c ChNum         Channel Number (required)
   -G               Print NMEA GGA data
   -C               Print NMEA RMC data
   -g Lat Lon Elev  Ground target position (ft)
   -m Dist          Only dump within this many nautical miles
                      of ground target position
   -T               Print TMATS summary and exit


IDMPUART
--------

Read and dump UART packet data.  By default UART data is dumped as a series of 
hexidecimal bytes.  

By default all channels will be dumped.  Use the -c flag to select a single 
channel.

Use the -s flag to dump the UART packet as an ASCII string.  This is handy for 
data such as GPS NMEA sentences.  Any non-printable ASCII data is dumped as a 
hexidecimal value.

The -T flag can be used to dump a brief TMATS summary of the data file to help 
in choosing command line parameters.

Usage: idmpUART <input file> <output file> [flags]
   <filename> Input/output file names
   -v         Verbose
   -c ChNum   Channel Number (default all)
   -s         Print out data as ASCII string
   -T         Print TMATS summary and exit

The output data fields are:
  Time Chan-Subchan Data...


IDMPTIME
--------

Read and dump time packets.  This is useful for debugging time problems.

Usage: idmptime <input file> <output file> [flags]
   <filename> Input/output file names
   -v         Verbose
   -c ChNum   Channel Number (default all)
   -T         Print TMATS summary and exit

Time columns are:
  Channel ID
  Relative Time Counter Value
  Time
  Time Source
  Time Format
  Leap Year Flag


IDMP429
-------

Read and dump ARINC 429 packets.  Use the "-c" flag to restrict dumps to
a specific channel number.  Each channel may have multiple 429 buses.  Use
the "-b" flag to restrict dumps to a specific 429 bus.

Usage: idmpa429 <input file> <output file> [flags]
   <filename> Input/output file names
   -v         Verbose
   -c ChNum   Channel Number (default all)
   -b BusNum  429 Bus Number (default all)
   -T         Print TMATS summary and exit

The output data fields are:
Time  ChanID  BusNum  Label  SDI  Data  SSM


IDMPETH
-------

