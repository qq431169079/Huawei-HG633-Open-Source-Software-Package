/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		The Internet Protocol (IP) module.
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *		Alan Cox, <alan@lxorguk.ukuu.org.uk>
 *		Richard Underwood
 *		Stefan Becker, <stefanb@yello.ping.de>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *
 *
 * Fixes:
 *		Alan Cox	:	Commented a couple of minor bits of surplus code
 *		Alan Cox	:	Undefining IP_FORWARD doesn't include the code
 *					(just stops a compiler warning).
 *		Alan Cox	:	Frames with >=MAX_ROUTE record routes, strict routes or loose routes
 *					are junked rather than corrupting things.
 *		Alan Cox	:	Frames to bad broadcast subnets are dumped
 *					We used to process them non broadcast and
 *					boy could that cause havoc.
 *		Alan Cox	:	ip_forward sets the free flag on the
 *					new frame it queues. Still crap because
 *					it copies the frame but at least it
 *					doesn't eat memory too.
 *		Alan Cox	:	Generic queue code and memory fixes.
 *		Fred Van Kempen :	IP fragment support (borrowed from NET2E)
 *		Gerhard Koerting:	Forward fragmented frames correctly.
 *		Gerhard Koerting: 	Fixes to my fix of the above 8-).
 *		Gerhard Koerting:	IP interface addressing fix.
 *		Linus Torvalds	:	More robustness checks
 *		Alan Cox	:	Even more checks: Still not as robust as it ought to be
 *		Alan Cox	:	Save IP header pointer for later
 *		Alan Cox	:	ip option setting
 *		Alan Cox	:	Use ip_tos/ip_ttl settings
 *		Alan Cox	:	Fragmentation bogosity removed
 *					(Thanks to Mark.Bush@prg.ox.ac.uk)
 *		Dmitry Gorodchanin :	Send of a raw packet crash fix.
 *		Alan Cox	:	Silly ip bug when an overlength
 *					fragment turns up. Now frees the
 *					queue.
 *		Linus Torvalds/ :	Memory leakage on fragmentation
 *		Alan Cox	:	handling.
 *		Gerhard Koerting:	Forwarding uses IP priority hints
 *		Teemu Rantanen	:	Fragment problems.
 *		Alan Cox	:	General cleanup, comments and reformat
 *		Alan Cox	:	SNMP statistics
 *		Alan Cox	:	BSD address rule semantics. Also see
 *					UDP as there is a nasty checksum issue
 *					if you do things the wrong way.
 *		Alan Cox	:	Always defrag, moved IP_FORWARD to the config.in file
 *		Alan Cox	: 	IP options adjust sk->priority.
 *		Pedro Roque	:	Fix mtu/length error in ip_forward.
 *		Alan Cox	:	Avoid ip_chk_addr when possible.
 *	Richard Underwood	:	IP multicasting.
 *		Alan Cox	:	Cleaned up multicast handlers.
 *		Alan Cox	:	RAW sockets demultiplex in the BSD style.
 *		Gunther Mayer	:	Fix the SNMP reporting typo
 *		Alan Cox	:	Always in group 224.0.0.1
 *	Pauline Middelink	:	Fast ip_checksum update when forwarding
 *					Masquerading support.
 *		Alan Cox	:	Multicast loopback error for 224.0.0.1
 *		Alan Cox	:	IP_MULTICAST_LOOP option.
 *		Alan Cox	:	Use notifiers.
 *		Bjorn Ekwall	:	Removed ip_csum (from slhc.c too)
 *		Bjorn Ekwall	:	Moved ip_fast_csum to ip.h (inline!)
 *		Stefan Becker   :       Send out ICMP HOST REDIRECT
 *	Arnt Gulbrandsen	:	ip_build_xmit
 *		Alan Cox	:	Per socket routing cache
 *		Alan Cox	:	Fixed routing cache, added header cache.
 *		Alan Cox	:	Loopback didn't work right in original ip_build_xmit - fixed it.
 *		Alan Cox	:	Only send ICMP_REDIRECT if src/dest are the same net.
 *		Alan Cox	:	Incoming IP option handling.
 *		Alan Cox	:	Set saddr on raw output frames as per BSD.
 *		Alan Cox	:	Stopped broadcast source route explosions.
 *		Alan Cox	:	Can disable source routing
 *		Takeshi Sone    :	Masquerading didn't work.
 *	Dave Bonn,Alan Cox	:	Faster IP forwarding whenever possible.
 *		Alan Cox	:	Memory leaks, tramples, misc debugging.
 *		Alan Cox	:	Fixed multicast (by popular demand 8))
 *		Alan Cox	:	Fixed forwarding (by even more popular demand 8))
 *		Alan Cox	:	Fixed SNMP statistics [I think]
 *	Gerhard Koerting	:	IP fragmentation forwarding fix
 *		Alan Cox	:	Device lock against page fault.
 *		Alan Cox	:	IP_HDRINCL facility.
 *	Werner Almesberger	:	Zero fragment bug
 *		Alan Cox	:	RAW IP frame length bug
 *		Alan Cox	:	Outgoing firewall on build_xmit
 *		A.N.Kuznetsov	:	IP_OPTIONS support throughout the kernel
 *		Alan Cox	:	Multicast routing hooks
 *		Jos Vos		:	Do accounting *before* call_in_firewall
 *	Willy Konynenberg	:	Transparent proxying support
 *
 *
 *
 * To Fix:
 *		IP fragmentation wants rewriting cleanly. The RFC815 algorithm is much more efficient
 *		and could be made very efficient with the addition of some virtual memory hacks to permit
 *		the allocation of a buffer that can then be 'grown' by twiddling page tables.
 *		Output fragmentation wants updating along with the buffer management to use a single
 *		interleaved copy algorithm so that fragmenting has a one copy overhead. Actual packet
 *		output should probably do its own fragmentation at the UDP/RAW layer. TCP shouldn't cause
 *		fragmentation anyway.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <asm/system.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#include <linux/net.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <net/snmp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/raw.h>
#include <net/checksum.h>
#include <linux/netfilter_ipv4.h>
#include <net/xfrm.h>
#include <linux/mroute.h>
#include <linux/netlink.h>
#if defined (CONFIG_MIPS_BRCM)
#include <linux/blog.h>
#endif

#if defined(CONFIG_BR_IGMP_STATISTIC)
#include "atp_interface.h"
#include <linux/igmp_statistic.h>
#endif

#ifdef CONFIG_FIREWALL_LOG
#include "atplogdef.h"
#include "msg/kcmsmonitormsgtypes.h"

static unsigned int zerolengthiptime = 0;
static unsigned int ipspoofingtime = 0;
#endif

	/* Start of modified by f00120964 for qos function 2012-4-2*/
#ifdef CONFIG_DT_QOS
	static unsigned char s_dtQosMarkToTOS[] = {0x0, 0x60, 0x80, 0xA0, 0xC0, 0x0, 0x0, 0x0};
#endif
    /* End of modified by f00120964 for qos function 2012-4-2*/

/*
 *	Process Router Attention IP option
 */
int ip_call_ra_chain(struct sk_buff *skb)
{
	struct ip_ra_chain *ra;
	u8 protocol = ip_hdr(skb)->protocol;
	struct sock *last = NULL;
	struct net_device *dev = skb->dev;

	read_lock(&ip_ra_lock);
	for (ra = ip_ra_chain; ra; ra = ra->next) {
		struct sock *sk = ra->sk;

		/* If socket is bound to an interface, only report
		 * the packet if it came  from that interface.
		 */
		if (sk && inet_sk(sk)->num == protocol &&
		    (!sk->sk_bound_dev_if ||
		     sk->sk_bound_dev_if == dev->ifindex) &&
		    sock_net(sk) == dev_net(dev)) {
			if (ip_hdr(skb)->frag_off & htons(IP_MF | IP_OFFSET)) {
				if (ip_defrag(skb, IP_DEFRAG_CALL_RA_CHAIN)) {
					read_unlock(&ip_ra_lock);
					return 1;
				}
			}
			if (last) {
				struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2)
					raw_rcv(last, skb2);
			}
			last = sk;
		}
	}

	if (last) {
		raw_rcv(last, skb);
		read_unlock(&ip_ra_lock);
		return 1;
	}
	read_unlock(&ip_ra_lock);
	return 0;
}

static int ip_local_deliver_finish(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);

	__skb_pull(skb, ip_hdrlen(skb));

	/* Point into the IP datagram, just past the header. */
	skb_reset_transport_header(skb);

	rcu_read_lock();
	{
		int protocol = ip_hdr(skb)->protocol;
		int hash, raw;
		struct net_protocol *ipprot;

	resubmit:
		raw = raw_local_deliver(skb, protocol);

		hash = protocol & (MAX_INET_PROTOS - 1);
		ipprot = rcu_dereference(inet_protos[hash]);
		if (ipprot != NULL) {
			int ret;

			if (!net_eq(net, &init_net) && !ipprot->netns_ok) {
				if (net_ratelimit())
					printk("%s: proto %d isn't netns-ready\n",
						__func__, protocol);
				kfree_skb(skb);
				goto out;
			}

			if (!ipprot->no_policy) {
				if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb)) {
					kfree_skb(skb);
					goto out;
				}
				nf_reset(skb);
			}
			ret = ipprot->handler(skb);
			if (ret < 0) {
				protocol = -ret;
				goto resubmit;
			}
			IP_INC_STATS_BH(net, IPSTATS_MIB_INDELIVERS);
		} else {
			if (!raw) {
				if (xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb)) {
					IP_INC_STATS_BH(net, IPSTATS_MIB_INUNKNOWNPROTOS);
					icmp_send(skb, ICMP_DEST_UNREACH,
						  ICMP_PROT_UNREACH, 0);
				}
			} else
				IP_INC_STATS_BH(net, IPSTATS_MIB_INDELIVERS);
			kfree_skb(skb);
		}
	}
 out:
	rcu_read_unlock();

	return 0;
}

/*
 * 	Deliver IP Packets to the higher protocol layers.
 */
int ip_local_deliver(struct sk_buff *skb)
{

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	/* do not track locally delivered packets, excluding 6rd tunnel packets */
	if (IPPROTO_IPV6 != ip_hdr(skb)->protocol)
	{
		blog_skip(skb);
	}
#endif

	/*
	 *	Reassemble IP fragments.
	 */

	if (ip_hdr(skb)->frag_off & htons(IP_MF | IP_OFFSET)) {
		if (ip_defrag(skb, IP_DEFRAG_LOCAL_DELIVER))
			return 0;
	}

	/* Start of modified by f00120964 for qos function 2012-4-2*/
#ifdef CONFIG_DT_QOS
	if ((0x0 != (skb->mark & 0x7)) && (0x0 == ip_hdr(skb)->tos)) {
		/* Without TOS value, mark it from skb->mark */
		u_int16_t diffs[2];
		diffs[0] = htons(ip_hdr(skb)->tos) ^ 0xffff;
		ip_hdr(skb)->tos = s_dtQosMarkToTOS[(skb->mark & 0x7)];
		diffs[1] = htons(ip_hdr(skb)->tos);
		ip_hdr(skb)->check = csum_fold(csum_partial((char *)diffs,
			sizeof(diffs), ip_hdr(skb)->check^0xffff));		
	}
#endif
    /* End of modified by f00120964 for qos function 2012-4-2*/

	return NF_HOOK(PF_INET, NF_INET_LOCAL_IN, skb, skb->dev, NULL,
		       ip_local_deliver_finish);
}

static inline int ip_rcv_options(struct sk_buff *skb)
{
	struct ip_options *opt;
	struct iphdr *iph;
	struct net_device *dev = skb->dev;

	/* It looks as overkill, because not all
	   IP options require packet mangling.
	   But it is the easiest for now, especially taking
	   into account that combination of IP options
	   and running sniffer is extremely rare condition.
					      --ANK (980813)
	*/
	if (skb_cow(skb, skb_headroom(skb))) {
		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INDISCARDS);
		goto drop;
	}

	iph = ip_hdr(skb);
	opt = &(IPCB(skb)->opt);
	opt->optlen = iph->ihl*4 - sizeof(struct iphdr);

	if (ip_options_compile(dev_net(dev), opt, skb)) {
		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INHDRERRORS);
		goto drop;
	}

	if (unlikely(opt->srr)) {
		struct in_device *in_dev = in_dev_get(dev);
		if (in_dev) {
			if (!IN_DEV_SOURCE_ROUTE(in_dev)) {
				if (IN_DEV_LOG_MARTIANS(in_dev) &&
				    net_ratelimit())
					printk(KERN_INFO "source route option %pI4 -> %pI4\n",
					       &iph->saddr, &iph->daddr);
				in_dev_put(in_dev);
				goto drop;
			}

			in_dev_put(in_dev);
		}

		if (ip_options_rcv_srr(skb))
			goto drop;
	}

	return 0;
drop:
	return -1;
}

/* This and several other changes in the file are for avoiding unaligned accesses. Merged from brcm 2.6.8 kernel changes.*/
#if defined(CONFIG_MIPS_BRCM) 
#define READ32_ALIGNED(a) ((((__u16 *) &(a))[0]<<16)|((__u16 *) &(a))[1])
#endif

static int ip_rcv_finish(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct rtable *rt;

	/*
	 *	Initialise the virtual path cache for the packet. It describes
	 *	how the packet travels inside Linux networking.
	 */
	if (skb->dst == NULL) {
#if defined(CONFIG_MIPS_BRCM)
		int err = ip_route_input(skb, READ32_ALIGNED(iph->daddr), READ32_ALIGNED(iph->saddr), iph->tos,
					 skb->dev);
#else
        int err = ip_route_input(skb, iph->daddr, iph->saddr, iph->tos,
					 skb->dev);
#endif
		if (unlikely(err)) {
			if (err == -EHOSTUNREACH)
				IP_INC_STATS_BH(dev_net(skb->dev),
						IPSTATS_MIB_INADDRERRORS);
			else if (err == -ENETUNREACH)
				IP_INC_STATS_BH(dev_net(skb->dev),
						IPSTATS_MIB_INNOROUTES);
			goto drop;
		}
	}

#ifdef CONFIG_NET_CLS_ROUTE
	if (unlikely(skb->dst->tclassid)) {
		struct ip_rt_acct *st = per_cpu_ptr(ip_rt_acct, smp_processor_id());
		u32 idx = skb->dst->tclassid;
		st[idx&0xFF].o_packets++;
		st[idx&0xFF].o_bytes += skb->len;
		st[(idx>>16)&0xFF].i_packets++;
		st[(idx>>16)&0xFF].i_bytes += skb->len;
	}
#endif

	if (iph->ihl > 5 && ip_rcv_options(skb))
		goto drop;

	rt = skb->rtable;
	if (rt->rt_type == RTN_MULTICAST)
		IP_INC_STATS_BH(dev_net(rt->u.dst.dev), IPSTATS_MIB_INMCASTPKTS);
	else if (rt->rt_type == RTN_BROADCAST)
		IP_INC_STATS_BH(dev_net(rt->u.dst.dev), IPSTATS_MIB_INBCASTPKTS);

#if defined(CONFIG_BR_IGMP_STATISTIC)
    if ( IS_BRIDGE_DEV(skb->dev->name) 
      && NULL != skb->dst 
      && NULL != skb->dst->dev 
      && IS_WAN_DEVICE(skb->dst->dev->name) )
    {
        ppp_ip_statistic_inc(skb,IGMP_UP_REQUEST_PACKET);
    }
#endif

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

#if defined(CONFIG_SIPALG_RECOREDDSTIP)

#define IS_LAN_DEV(skb) ((skb->mark & 0xf00000) ==0x200000)


extern uint32_t g_ulSiproxdStatus;
extern uint32_t g_ulSipPortInfo;
extern void UpdateSipRouteByCallid(unsigned char *call_id, unsigned int call_id_len, __be32 daddr, __be16 dport, uint32_t proto, bool bAdd);




struct sip_header {
	const char	*name;
	const char	*cname;
	const char	*search;
	unsigned int	len;
	unsigned int	clen;
	unsigned int	slen;
	int		(*match_len)(const char *dptr, const char *limit,
				     int *shift);
};

#define __SIP_HDR(__name, __cname, __search, __match)			\
{									\
	.name		= (__name),					\
	.len		= sizeof(__name) - 1,				\
	.cname		= (__cname),					\
	.clen		= (__cname) ? sizeof(__cname) - 1 : 0,		\
	.search		= (__search),					\
	.slen		= (__search) ? sizeof(__search) - 1 : 0,	\
	.match_len	= (__match),					\
}

#define SIP_HDR(__name, __cname, __search, __match) \
	__SIP_HDR(__name, __cname, __search, __match)

#define SDP_HDR(__name, __search, __match) \
	__SIP_HDR(__name, NULL, __search, __match)

enum sip_header_types {
	SIP_HDR_CSEQ,
	SIP_HDR_FROM,
	SIP_HDR_TO,
	SIP_HDR_CONTACT,
	SIP_HDR_VIA,
	SIP_HDR_EXPIRES,
	SIP_HDR_CONTENT_LENGTH,
	SIP_HDR_CALLID,
};

enum sdp_header_types {
	SDP_HDR_UNSPEC,
	SDP_HDR_VERSION,
	SDP_HDR_OWNER_IP4,
	SDP_HDR_CONNECTION_IP4,
	SDP_HDR_OWNER_IP6,
	SDP_HDR_CONNECTION_IP6,
	SDP_HDR_MEDIA,
};

extern unsigned char _ctype[];

#define _U	0x01	/* upper */
#define _L	0x02	/* lower */
#define _D	0x04	/* digit */
#define _C	0x08	/* cntrl */
#define _P	0x10	/* punct */
#define _S	0x20	/* white space (space/lf/tab) */
#define _X	0x40	/* hex digit */
#define _SP	0x80	/* hard space (0x20) */



#define __ismask(x) (_ctype[(int)(unsigned char)(x)])

#define isalpha(c)	((__ismask(c)&(_U|_L)) != 0)
#define isgraph(c)	((__ismask(c)&(_P|_U|_L|_D)) != 0)


static int graph_len(const char *dptr,
		      const char *limit, int *shift)
{
	int len = 0;

	while (dptr < limit && isgraph(*dptr)) {
		dptr++;
		len++;
	}
	return len;
}


static const struct sip_header ct_sip_hdrs[] = {
#if 0
	[SIP_HDR_CSEQ]			= SIP_HDR("CSeq", NULL, NULL, digits_len),
	[SIP_HDR_FROM]			= SIP_HDR("From", "f", "sip:", skp_epaddr_len),
	[SIP_HDR_TO]			= SIP_HDR("To", "t", "sip:", skp_epaddr_len),
	[SIP_HDR_CONTACT]		= SIP_HDR("Contact", "m", "sip:", skp_epaddr_len),
	[SIP_HDR_VIA]			= SIP_HDR("Via", "v", "UDP ", epaddr_len),
	[SIP_HDR_EXPIRES]		= SIP_HDR("Expires", NULL, NULL, digits_len),
	[SIP_HDR_CONTENT_LENGTH]	= SIP_HDR("Content-Length", "l", NULL, digits_len),
#endif
	[SIP_HDR_CALLID]	    = SIP_HDR("Call-ID", NULL, NULL, graph_len),
};

static const char *sip_follow_continuation(const char *dptr, const char *limit)
{
	/* Walk past newline */
	if (++dptr >= limit)
		return NULL;

	/* Skip '\n' in CR LF */
	if (*(dptr - 1) == '\r' && *dptr == '\n') {
		if (++dptr >= limit)
			return NULL;
	}

	/* Continuation line? */
	if (*dptr != ' ' && *dptr != '\t')
		return NULL;

	/* skip leading whitespace */
	for (; dptr < limit; dptr++) {
		if (*dptr != ' ' && *dptr != '\t')
			break;
	}
	return dptr;
}

static const char *sip_skip_whitespace(const char *dptr, const char *limit)
{
	for (; dptr < limit; dptr++) {
		if (*dptr == ' ')
			continue;
		if (*dptr != '\r' && *dptr != '\n')
			break;
		dptr = sip_follow_continuation(dptr, limit);
		if (dptr == NULL)
			return NULL;
	}
	return dptr;
}

/* Search within a SIP header value, dealing with continuation lines */
static const char *ct_sip_header_search(const char *dptr, const char *limit,
					const char *needle, unsigned int len)
{
	for (limit -= len; dptr < limit; dptr++) {
		if (*dptr == '\r' || *dptr == '\n') {
			dptr = sip_follow_continuation(dptr, limit);
			if (dptr == NULL)
				break;
			continue;
		}

		if (strnicmp(dptr, needle, len) == 0)
			return dptr;
	}
	return NULL;
}

int ct_sip_get_header(const char *dptr,
		      unsigned int dataoff, unsigned int datalen,
		      enum sip_header_types type,
		      unsigned int *matchoff, unsigned int *matchlen)
{
	const struct sip_header *hdr = &ct_sip_hdrs[type];
	const char *start = dptr, *limit = dptr + datalen;
	int shift = 0;

    

	for (dptr += dataoff; dptr < limit; dptr++) {
		/* Find beginning of line */
		if (*dptr != '\r' && *dptr != '\n')
			continue;
		if (++dptr >= limit)
			break;
		if (*(dptr - 1) == '\r' && *dptr == '\n') {
			if (++dptr >= limit)
				break;
		}

		/* Skip continuation lines */
		if (*dptr == ' ' || *dptr == '\t')
			continue;


		/* Find header. Compact headers must be followed by a
		 * non-alphabetic character to avoid mismatches. */
		if (limit - dptr >= hdr->len &&
		    strnicmp(dptr, hdr->name, hdr->len) == 0)
			dptr += hdr->len;
		else if (hdr->cname && limit - dptr >= hdr->clen + 1 &&
			 strnicmp(dptr, hdr->cname, hdr->clen) == 0 &&
			 !isalpha(*(dptr + hdr->clen + 1)))
			dptr += hdr->clen;
		else
			continue;

		/* Find and skip colon */
		dptr = sip_skip_whitespace(dptr, limit);
		if (dptr == NULL)
			break;
		if (*dptr != ':' || ++dptr >= limit)
			break;

		/* Skip whitespace after colon */
		dptr = sip_skip_whitespace(dptr, limit);
		if (dptr == NULL)
			break;


		*matchoff = dptr - start;
		if (hdr->search) {
			dptr = ct_sip_header_search(dptr, limit, hdr->search,
						    hdr->slen);
			if (!dptr)
				return -1;
			dptr += hdr->slen;
		}

		*matchlen = hdr->match_len(dptr, limit, &shift);
		if (!*matchlen)
			return -1;
		*matchoff = dptr - start + shift;
		return 1;
	}
	return 0;
}

void RecordSipContent(struct sk_buff *skb, __be32 daddr, __be16 dport, uint32_t proto)
{
	const char *dptr = NULL;
	unsigned int dataoff = 0;
    unsigned int matchoff = 0;
    unsigned int matchlen = 0;


	dataoff = sizeof(struct iphdr) + sizeof(struct udphdr);
    dptr = skb->data + dataoff;

    /* 记录sip请求的目的地址，在应用层siproxd中读出该记录 */
    if ((0 == strnicmp(dptr, "REGISTER", (sizeof("REGISTER")-1)))
        || (0 == strnicmp(dptr, "INVITE", (sizeof("INVITE")-1)))
        || (0 == strnicmp(dptr, "ACK", (sizeof("ACK")-1)))
        || (0 == strnicmp(dptr, "OPTIONS", (sizeof("OPTIONS")-1)))
        || (0 == strnicmp(dptr, "BYE", (sizeof("BYE")-1)))
        || (0 == strnicmp(dptr, "CANCEL", (sizeof("CANCEL")-1)))
        || (0 == strnicmp(dptr, "INFO", (sizeof("INFO")-1)))
        || (0 == strnicmp(dptr, "PRACK", (sizeof("PRACK")-1)))
        || (0 == strnicmp(dptr, "SUBSCRIBE", (sizeof("SUBSCRIBE")-1)))
        || (0 == strnicmp(dptr, "NOTIFY", (sizeof("NOTIFY")-1)))
        || (0 == strnicmp(dptr, "UPDATE", (sizeof("UPDATE")-1)))
        || (0 == strnicmp(dptr, "MESSAGE", (sizeof("MESSAGE")-1)))
        || (0 == strnicmp(dptr, "UNSUBSCRIBE", (sizeof("UNSUBSCRIBE")-1)))
        || (0 == strnicmp(dptr, "REFER", (sizeof("REFER")-1)))
        || (0 == strnicmp(dptr, "PUBLISH", (sizeof("PUBLISH")-1))/*SIP 请求*/)
        )                    //(0 == strnicmp(dptr, "SIP", (sizeof("SIP")-1)))/*SIP 响应*/
    {
        
        if (ct_sip_get_header(dptr, 0, skb->len-dataoff, SIP_HDR_CALLID,
              &matchoff, &matchlen) > 0)
        {
            UpdateSipRouteByCallid(dptr + matchoff, matchlen, daddr, dport, proto, 1);
        }
        
    }
    
}
#endif
/*
 * 	Main IP Receive routine.
 */
int ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct iphdr *iph;
	u32 len;
#if defined(CONFIG_MIPS_BRCM)
	__u8 iph_ihl, iph_version;
#endif

#if defined(CONFIG_SIPALG_RECOREDDSTIP)
    struct iphdr *iph2 = ip_hdr(skb);
    struct udphdr* uhdr = NULL;
#endif

#ifdef CONFIG_FIREWALL_LOG
	struct net_device *device = NULL;  
	struct in_device* in_dev = NULL;
	struct in_ifaddr* if_info = NULL;
	u_int8_t* pszLogBuf = NULL;
#endif
#if defined(CONFIG_SIPALG_RECOREDDSTIP)
    uhdr = (struct udphdr*)(skb->data + sizeof(struct iphdr));
    /* Record destination and call-id of sip packet, so siproxd can accqure destination by call-id */
    if ((0 != g_ulSipPortInfo) && (g_ulSiproxdStatus == ALG_SIP_START_SUCCESS) && 
      ((IPPROTO_UDP == iph2->protocol) || (IPPROTO_TCP == iph2->protocol)))// && IS_LAN_DEV(skb)
    {
        if (__constant_htons(uhdr->dest) == g_ulSipPortInfo)
        {
            RecordSipContent(skb, iph2->daddr, uhdr->dest, iph2->protocol);
        }
    }
#endif


	/* When the interface is in promisc. mode, drop all the crap
	 * that it receives, do not try to analyse it.
	 */
	if (skb->pkt_type == PACKET_OTHERHOST)
		goto drop;

	IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INRECEIVES);

	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL) {
		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INDISCARDS);
		goto out;
	}

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto inhdr_error;

	iph = ip_hdr(skb);
	
#ifdef CONFIG_FIREWALL_LOG
	/* IP Spoofing Attack */
	if ((0 == ipspoofingtime || time_after(jiffies, ipspoofingtime))
	&& ('b' != dev->name[0]))
	{
		device = dev_get_by_name(&init_net, "br0");
		if (NULL != device)
		{
			in_dev = (struct in_device*)device->ip_ptr;
			if (in_dev && (NULL != (if_info = in_dev->ifa_list)))
			{
				for (; if_info; if_info = if_info->ifa_next)
				{
					if ((if_info->ifa_address & if_info->ifa_mask) == (iph->saddr & if_info->ifa_mask))
					{
						ipspoofingtime = jiffies + FIREWALL_ATTACK_LOG_INTERVAL*HZ;
						pszLogBuf = (u_int8_t*)kmalloc(ATP_LOG_LENGTH_128, GFP_ATOMIC);
						if (NULL != pszLogBuf)
						{
							memset(pszLogBuf, 0, ATP_LOG_LENGTH_128);
							snprintf(pszLogBuf, ATP_LOG_LENGTH_128, "Detect IP Spoofing Attack, packet from "NIPQUAD_FMT".\n", NIPQUAD(iph->saddr));
							syswatch_sendLogEx(ATP_LOG_TYPE_FIREWALL, ATP_LOG_LEVEL_WARNING, 0, pszLogBuf);
							kfree(pszLogBuf);
							pszLogBuf = NULL;
						}
						break;
					}
				}
			}
		}
	}
#endif

	/*
	 *	RFC1122: 3.2.1.2 MUST silently discard any IP frame that fails the checksum.
	 *
	 *	Is the datagram acceptable?
	 *
	 *	1.	Length at least the size of an ip header
	 *	2.	Version of 4
	 *	3.	Checksums correctly. [Speed optimisation for later, skip loopback checksums]
	 *	4.	Doesn't have a bogus length
	 */

#if defined(CONFIG_MIPS_BRCM)
	iph_ihl = *(__u8 *)iph & 0xf;
	iph_version = *(__u8 *)iph >> 4;

	if (iph_ihl < 5 || iph_version != 4)
#else
	if (iph->ihl < 5 || iph->version != 4)
#endif
		goto inhdr_error;

#if defined(CONFIG_MIPS_BRCM)
	if (!pskb_may_pull(skb, iph_ihl*4))
#else
	if (!pskb_may_pull(skb, iph->ihl*4))
#endif
		goto inhdr_error;

	iph = ip_hdr(skb);

#if defined(CONFIG_MIPS_BRCM)
	if (unlikely(ip_fast_csum((u8 *)iph, iph_ihl)))
#else
	if (unlikely(ip_fast_csum((u8 *)iph, iph->ihl)))
#endif
		goto inhdr_error;

	len = ntohs(iph->tot_len);
	if (skb->len < len) {
		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INTRUNCATEDPKTS);
		goto drop;
	} else if (len < (iph->ihl*4))
		goto inhdr_error;
#ifdef CONFIG_FIREWALL_LOG
	else if (len == (iph->ihl*4))
	{
		/* Zero Length IP Attack */
		if(0 == zerolengthiptime 
		|| time_after(jiffies, zerolengthiptime))
		{
			zerolengthiptime = jiffies + FIREWALL_ATTACK_LOG_INTERVAL*HZ;
			pszLogBuf = (u_int8_t*)kmalloc(ATP_LOG_LENGTH_128, GFP_ATOMIC);
			if (NULL != pszLogBuf)
			{
				memset(pszLogBuf, 0, ATP_LOG_LENGTH_128);
				snprintf(pszLogBuf, ATP_LOG_LENGTH_128, "Detect Zero Length IP Attack, packet from "NIPQUAD_FMT".\n", NIPQUAD(iph->saddr));
				syswatch_sendLogEx(ATP_LOG_TYPE_FIREWALL, ATP_LOG_LEVEL_WARNING, 0, pszLogBuf);
				kfree(pszLogBuf);
				pszLogBuf = NULL;
			}
		}        
	}
#endif
	/* Our transport medium may have padded the buffer out. Now we know it
	 * is IP we can trim to the true length of the frame.
	 * Note this now means skb->len holds ntohs(iph->tot_len).
	 */
	if (pskb_trim_rcsum(skb, len)) {
		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INDISCARDS);
		goto drop;
	}

	/* Remove any debris in the socket control block */
	memset(IPCB(skb), 0, sizeof(struct inet_skb_parm));
#if defined(CONFIG_BR_IGMP_STATISTIC)
    if (IS_WAN_DEVICE(skb->dev->name))
    {
        ppp_ip_statistic_inc(skb,IGMP_DOWN_RETRANSMIT_PACKET);
    }
#endif    
	return NF_HOOK(PF_INET, NF_INET_PRE_ROUTING, skb, dev, NULL,
		       ip_rcv_finish);

inhdr_error:
	IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INHDRERRORS);
drop:
	kfree_skb(skb);
out:
	return NET_RX_DROP;
}
