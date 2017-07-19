##
## Process this file with automake to produce 'Makefile.in'
##
## To create the configure script using this file follow these steps
## in order
## 1. write Makefile.am
## 2. autoscan
## 3. aclocal
## 4. autoheader
## 5. autoconf
## 6. automake -ac --foreign
##

AM_CFLAGS=-g -O2 -Wall -Wextra -fstack-check -fPIC \
			-D PREFIX=\"${prefix}\" -D SYSCONFDIR=\"${sysconfdir}\"

SUBDIRS = etc

sbin_PROGRAMS = axfrdns dnscache rbldns tinydns walldns

bin_PROGRAMS = dnsip dnsipq dnsq dnsname dnstxt dnsqr dnsfilter \
	dnstrace tinydns-data tinydns-edit tinydns-get randomip axfr-get \
	tcprules rbldns-data

noinst_PROGRAMS = dnsmx

bin_SCRIPTS = dnstracesort

noinst_MANS = dnscache.ms dnsip.ms dnsq.ms dnsfilter.ms djbdns.ms \
	tinydns-data.ms tinydns-edit.ms tinydns-get.ms tinydns.ms dnsipq.ms \
	dnstxt.ms tcprules.ms dnsqr.ms random-ip.ms dnstrace.ms axfrdns.ms \
	axfr-get.ms dnsname.ms rbldns.ms rbldns-data.ms walldns.ms

nodist_man8_MANS = axfrdns.8 dnscache.8 rbldns.8 tinydns.8 walldns.8

nodist_man1_MANS = dnsip.1 dnsq.1 dnsfilter.1 djbdns.1 tinydns-data.1 \
	tinydns-edit.1 tinydns-get.1 dnsipq.1 dnstxt.1 tcprules.1 dnsqr.1 \
	randomip.1 dnstrace.1 axfr-get.1 dnsname.1 rbldns-data.1

CLEANFILES = $(bin_SCRIPTS) $(nodist_man1_MANS) $(nodist_man8_MANS)
EXTRA_DIST = dnstracesort.sh readme.ms README.md INSTALL TODO ChangeLog \
	COPYING $(noinst_MANS) ndjbdns.logrotate

axfrdns_SOURCES = axfrdns.c iopause.c droproot.c tdlookup.c response.c \
	log.c prot.c timeoutread.c timeoutwrite.c common.c \
	droproot.h timeoutread.h timeoutwrite.h response.h select.h \
	prot.h hasshsgr.h common.h
axfrdns_LDADD = libdns.a libtai.a liballoc.a libenv.a libcdb.a \
	libbuffer.a libunix.a libbyte.a

axfrdns.8: axfrdns.ms
	sed -e 's:PREFIX:${sysconfdir}:' axfrdns.ms > axfrdns.8

axfr_get_SOURCES = axfr-get.c iopause.c timeoutread.c timeoutwrite.c \
	timeoutread.h timeoutwrite.h select.h
axfr_get_LDADD = libdns.a libtai.a liballoc.a libbuffer.a libunix.a libbyte.a

axfr-get.1: axfr-get.ms
	cp axfr-get.ms axfr-get.1

djbdns.1: djbdns.ms
	cp djbdns.ms djbdns.1

dnscache_SOURCES = dnscache.c droproot.c okclient.c log.c siphash.c cache.c \
	dns_random.c query.c response.c dd.c roots.c iopause.c prot.c common.c \
	response.h select.h prot.h roots.h query.h siphash.h cache.h log.h \
	okclient.h dd.h direntry.h hasshsgr.h version.h common.h clients.h cdb.h
dnscache_LDADD = libdns.a libenv.a liballoc.a libbuffer.a libtai.a libcdb.a \
	libunix.a libbyte.a

dnscache.8: dnscache.ms
	sed -e 's:PREFIX:${sysconfdir}:' dnscache.ms > dnscache.8

dnsfilter_SOURCES = dnsfilter.c iopause.c select.h
dnsfilter_LDADD = libdns.a libenv.a libtai.a liballoc.a \
	libbuffer.a libunix.a libbyte.a

dnsfilter.1: dnsfilter.ms
	cp dnsfilter.ms dnsfilter.1

dnsip_SOURCES = dnsip.c iopause.c select.h version.h
dnsip_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnsip.1: dnsip.ms
	cp dnsip.ms dnsip.1

dnsipq_SOURCES = dnsipq.c iopause.c select.h
dnsipq_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnsipq.1: dnsipq.ms
	cp dnsipq.ms dnsipq.1

dnsmx_SOURCES = dnsmx.c iopause.c select.h
dnsmx_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnsname_SOURCES = dnsname.c iopause.c select.h
dnsname_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnsname.1: dnsname.ms
	cp dnsname.ms dnsname.1

dnsq_SOURCES = dnsq.c iopause.c printrecord.c printpacket.c parsetype.c \
	select.h printpacket.h parsetype.h printrecord.h
dnsq_LDADD = libdns.a libenv.a libtai.a libbuffer.a liballoc.a \
	libunix.a libbyte.a

dnsq.1: dnsq.ms
	cp dnsq.ms dnsq.1

dnsqr_SOURCES = dnsqr.c iopause.c printrecord.c printpacket.c parsetype.c \
	select.h printpacket.h parsetype.h printrecord.h
dnsqr_LDADD = libdns.a libenv.a libtai.a libbuffer.a liballoc.a \
	libunix.a libbyte.a

dnsqr.1: dnsqr.ms
	cp dnsqr.ms dnsqr.1

dnstrace_SOURCES = dnstrace.c dd.c iopause.c printrecord.c parsetype.c \
	select.h dd.h parsetype.h printrecord.h
dnstrace_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnstracesort: dnstracesort.sh
	cp dnstracesort.sh dnstracesort

dnstrace.1: dnstrace.ms
	cp dnstrace.ms dnstrace.1

dnstxt_SOURCES = dnstxt.c iopause.c select.h
dnstxt_LDADD = libdns.a libenv.a libtai.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

dnstxt.1: dnstxt.ms
	cp dnstxt.ms dnstxt.1

randomip_SOURCES = random-ip.c
randomip_LDADD = libdns.a libtai.a libbuffer.a libunix.a libbyte.a

randomip.1: random-ip.ms
	cp random-ip.ms randomip.1

rbldns_SOURCES = rbldns.c server.c response.c dd.c droproot.c prot.c \
	common.c iopause.c log.c \
	log.h response.h prot.h dd.h hasshsgr.h common.h iopause.h
rbldns_LDADD = libdns.a libenv.a libtai.a libcdb.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

rbldns.8: rbldns.ms
	sed -e 's:PREFIX:${sysconfdir}:' rbldns.ms > rbldns.8

rbldns_data_SOURCES = rbldns-data.c
rbldns_data_LDADD = libcdb.a liballoc.a libbuffer.a libunix.a libbyte.a

rbldns-data.1: rbldns-data.ms
	cp rbldns-data.ms rbldns-data.1

tcprules_SOURCES = tcprules.c
tcprules_LDADD = libcdb.a liballoc.a libbuffer.a libunix.a libbyte.a

tcprules.1: tcprules.ms
	cp tcprules.ms tcprules.1

tinydns_SOURCES = tinydns.c server.c droproot.c tdlookup.c response.c \
	log.c prot.c common.c iopause.c \
	log.h response.h prot.h hasshsgr.h common.h iopause.h
tinydns_LDADD = libdns.a libtai.a libenv.a libcdb.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

# an init script for tinydns service.
tinydnsd: tinydns.sh
	sed -e 's:PREFIX:${prefix}:' tinydns.sh > tinydnsd

tinydns.8: tinydns.ms
	sed -e 's:PREFIX:${sysconfdir}:' tinydns.ms > tinydns.8

tinydns_data_SOURCES = tinydns-data.c
tinydns_data_LDADD = libcdb.a libdns.a liballoc.a libbuffer.a \
	libunix.a libbyte.a

tinydns-data.1: tinydns-data.ms
	cp tinydns-data.ms tinydns-data.1

tinydns_edit_SOURCES = tinydns-edit.c
tinydns_edit_LDADD = libdns.a liballoc.a libbuffer.a libunix.a libbyte.a

tinydns-edit.1: tinydns-edit.ms
	cp tinydns-edit.ms tinydns-edit.1

tinydns_get_SOURCES = tinydns-get.c tdlookup.c response.c printpacket.c \
	printrecord.c parsetype.c \
	response.h printrecord.h
tinydns_get_LDADD = libdns.a libtai.a libcdb.a libbuffer.a liballoc.a \
	libunix.a libbyte.a

tinydns-get.1: tinydns-get.ms
	cp tinydns-get.ms tinydns-get.1

walldns_SOURCES = walldns.c server.c response.c droproot.c prot.c dd.c \
	common.c iopause.c log.c \
	log.h response.h prot.h dd.h hasshsgr.h common.h iopause.h
walldns_LDADD = libdns.a libenv.a libcdb.a liballoc.a libbuffer.a \
	libunix.a libbyte.a libtai.a

walldns.8: walldns.ms
	sed -e 's:PREFIX:${sysconfdir}:' walldns.ms > walldns.8


#
# Static libraries built for compilation. These are NOT installed.
#

noinst_LIBRARIES = liballoc.a libbuffer.a libbyte.a libcdb.a libdns.a \
	libenv.a libtai.a libunix.a

liballoc_a_SOURCES = alloc.c getln.c getln2.c stralloc.c \
	alloc.h error.h byte.h getln.h buffer.h stralloc.h gen_alloc.h \
	gen_allocdefs.h str.h
#	stralloc_cat.c
#	stralloc_catb.c stralloc_cats.c stralloc_copy.c stralloc_eady.c \
#	stralloc_num.c stralloc_opyb.c stralloc_opys.c stralloc_pend.c

libbuffer_a_SOURCES = buffer.c strerr_die.c strerr_sys.c \
	buffer.h byte.h error.h str.h strerr.h
#	buffer_1.c buffer_2.c buffer_copy.c \
#	buffer_get.c buffer_put.c buffer_read.c buffer_write.c

libbyte_a_SOURCES = byte_chr.c byte_rchr.c byte_copy.c byte_cr.c \
	byte_diff.c byte_zero.c case_diffb.c case_diffs.c case_lowerb.c \
	fmt_ulong.c ip4_fmt.c ip4_scan.c scan_ulong.c str_chr.c str_diff.c \
	str_len.c str_rchr.c str_start.c uint16_pack.c uint16_unpack.c \
	uint32_pack.c uint32_unpack.c byte.h case.h fmt.h ip4.h scan.h str.h \
	uint16.h uint32.h

libcdb_a_SOURCES = cdb.c cdb_hash.c cdb_make.c \
	error.h seek.h byte.h cdb.h uint32.h alloc.h cdb_make.h buffer.h

libdns_a_SOURCES = dns_dfd.c dns_domain.c dns_dtda.c dns_ip.c dns_ipq.c \
	dns_mx.c dns_name.c dns_nd.c dns_packet.c dns_random.c dns_rcip.c \
	dns_rcrw.c dns_resolve.c dns_sortip.c dns_transmit.c dns_txt.c \
	error.h alloc.h byte.h dns.h stralloc.h gen_alloc.h iopause.h taia.h \
	tai.h uint64.h case.h uint16.h str.h fmt.h uint32.h openreadclose.h \
	ip4.h env.h socket.h select.h

libenv_a_SOURCES = env.c str.h env.h

libtai_a_SOURCES = tai_add.c tai_now.c tai_pack.c tai_sub.c tai_uint.c \
	tai_unpack.c taia_add.c taia_approx.c taia_frac.c taia_less.c \
	taia_now.c taia_pack.c taia_sub.c taia_tai.c taia_uint.c \
	tai.h uint64.h taia.h

libunix_a_SOURCES = error.c error_str.c ndelay_off.c ndelay_on.c \
	open_read.c open_trunc.c openreadclose.c readclose.c seek_set.c \
	socket_accept.c socket_bind.c socket_conn.c socket_listen.c \
	socket_recv.c socket_send.c socket_tcp.c socket_udp.c error.h \
	ndelay.h open.h readclose.h stralloc.h gen_alloc.h openreadclose.h \
	seek.h byte.h socket.h uint16.h
