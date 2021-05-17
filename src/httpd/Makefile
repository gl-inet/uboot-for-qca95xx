#
# Makefile for http stuff
#

include $(TOPDIR)/config.mk

LIB   = libhttpd.a
OBJS += uip.o uip_arch.o uip_arp.o httpd.o fs.o


all:    $(LIB)

fs.c:   fsdata.c

fsdata.c:
ifeq ($(CFG_BOARD_TYPE), ar300m)
	cp ./ar300m-index.html ./vendors/general/index.html
	echo "cp switch index.html"
else
	cp ./index.html ./vendors/general/index.html
	echo "cp index.html"
endif
	@sed -i "s/version:.*</version:`date +%y.%m.%d`</g" $(TOPDIR)/httpd/vendors/general/index.html
	    ./vendors/makefsdatac general

$(LIB): $(START) $(OBJS)
	    $(AR) crv $@ $(OBJS)

#########################################################################

.depend: Makefile $(OBJS:.o=.c)
	     $(CC) -M $(CFLAGS) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################

