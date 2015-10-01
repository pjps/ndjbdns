#include "uint16.h"
#include "uint32.h"
#include "error.h"
#include "byte.h"
#include "dns.h"
#include "printrecord.h"

static char *d;

unsigned int printrecord_cat(stralloc *out,const char *buf,unsigned int len,unsigned int pos,const char *q,const char qtype[2])
{
  const char *x;
  char misc[20];
  uint16 datalen;
  uint16 u16;
  uint32 u32;
  unsigned int newpos;
  int i;
  unsigned char ch;

  pos = dns_packet_getname(buf,len,pos,&d); if (!pos) return 0;
  pos = dns_packet_copy(buf,len,pos,misc,10); if (!pos) return 0;
  uint16_unpack_big(misc + 8,&datalen);
  newpos = pos + datalen;

  if (q) {
    if (!dns_domain_equal(d,q))
      return newpos;
    if (byte_diff(qtype,2,misc) && byte_diff(qtype,2,DNS_T_ANY))
      return newpos;
  }

  if (!dns_domain_todot_cat(out,d)) return 0;
  if (!stralloc_cats(out," ")) return 0;
  uint32_unpack_big(misc + 4,&u32);
  if (!stralloc_catulong0(out,u32,0)) return 0;

  if (byte_diff(misc + 2,2,DNS_C_IN)) {
    if (!stralloc_cats(out," weird class\n")) return 0;
    return newpos;
  }

  x = 0;
  if (byte_equal(misc,2,DNS_T_NS)) x = " NS ";
  if (byte_equal(misc,2,DNS_T_PTR)) x = " PTR ";
  if (byte_equal(misc,2,DNS_T_CNAME)) x = " CNAME ";
  if (x) {
    pos = dns_packet_getname(buf,len,pos,&d); if (!pos) return 0;
    if (!stralloc_cats(out,x)) return 0;
    if (!dns_domain_todot_cat(out,d)) return 0;
  }
  else if (byte_equal(misc,2,DNS_T_MX)) {
    if (!stralloc_cats(out," MX ")) return 0;
    pos = dns_packet_copy(buf,len,pos,misc,2); if (!pos) return 0;
    pos = dns_packet_getname(buf,len,pos,&d); if (!pos) return 0;
    uint16_unpack_big(misc,&u16);
    if (!stralloc_catulong0(out,u16,0)) return 0;
    if (!stralloc_cats(out," ")) return 0;
    if (!dns_domain_todot_cat(out,d)) return 0;
  }
  else if (byte_equal(misc,2,DNS_T_SOA)) {
    if (!stralloc_cats(out," SOA ")) return 0;
    pos = dns_packet_getname(buf,len,pos,&d); if (!pos) return 0;
    if (!dns_domain_todot_cat(out,d)) return 0;
    if (!stralloc_cats(out," ")) return 0;
    pos = dns_packet_getname(buf,len,pos,&d); if (!pos) return 0;
    if (!dns_domain_todot_cat(out,d)) return 0;
    pos = dns_packet_copy(buf,len,pos,misc,20); if (!pos) return 0;
    for (i = 0;i < 5;++i) {
      if (!stralloc_cats(out," ")) return 0;
      uint32_unpack_big(misc + 4 * i,&u32);
      if (!stralloc_catulong0(out,u32,0)) return 0;
    }
  }
  else if (byte_equal(misc,2,DNS_T_A)) {
    if (datalen != 4) { errno = error_proto; return 0; }
    if (!stralloc_cats(out," A ")) return 0;
    pos = dns_packet_copy(buf,len,pos,misc,4); if (!pos) return 0;
    for (i = 0;i < 4;++i) {
      ch = misc[i];
      if (i) if (!stralloc_cats(out,".")) return 0;
      if (!stralloc_catulong0(out,ch,0)) return 0;
    }
  }
  else if (byte_equal(misc,2,DNS_T_AAAA)) {
    if (datalen != 16) { errno = error_proto; return 0; }
    if (!stralloc_cats(out," AAAA ")) return 0;
    pos = dns_packet_copy(buf,len,pos,misc,16); if (!pos) return 0;
    for (i = 0;i < 16; ++i) {
      ch = misc[i];
      if (i && (i % 2 == 0)) if (!stralloc_cats(out,":")) return 0;
      if (!stralloc_catulong_ashex0(out,ch,2)) return 0;
    }
  }
  else {
    if (!stralloc_cats(out," ")) return 0;
    uint16_unpack_big(misc,&u16);
    if (!stralloc_catulong0(out,u16,0)) return 0;
    if (!stralloc_cats(out," ")) return 0;
    while (datalen--) {
      pos = dns_packet_copy(buf,len,pos,misc,1); if (!pos) return 0;
      if ((misc[0] >= 33) && (misc[0] <= 126) && (misc[0] != '\\')) {
        if (!stralloc_catb(out,misc,1)) return 0;
      }
      else {
        ch = misc[0];
        misc[3] = '0' + (7 & ch); ch >>= 3;
        misc[2] = '0' + (7 & ch); ch >>= 3;
        misc[1] = '0' + (7 & ch);
        misc[0] = '\\';
        if (!stralloc_catb(out,misc,4)) return 0;
      }
    }
  }

  if (!stralloc_cats(out,"\n")) return 0;
  if (pos != newpos) { errno = error_proto; return 0; }
  return newpos;
}

unsigned int printrecord(stralloc *out,const char *buf,unsigned int len,unsigned int pos,const char *q,const char qtype[2])
{
  if (!stralloc_copys(out,"")) return 0;
  return printrecord_cat(out,buf,len,pos,q,qtype);
}
