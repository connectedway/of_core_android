/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#define ANDROID_MULTINETWORKING

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <android/multinetwork.h>

#include "ofc/core.h"
#include "ofc/types.h"
#include "ofc/config.h"
#include "ofc/libc.h"
#include "ofc/heap.h"
#include "ofc/net.h"
#include "ofc/net_internal.h"
#include "ofc/framework.h"

#if defined(OFC_KERBEROS)
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#endif

static OFC_UINT64 g_network_handle;
/**
 * \defgroup net_android Android Network Implementation
 */

/** \{ */

OFC_VOID ofc_net_init_impl(OFC_VOID) {
  g_network_handle = 0L;

  signal (SIGPIPE, SIG_IGN) ;
}

OFC_VOID ofc_net_register_config_impl(OFC_HANDLE hEvent) {
}

OFC_VOID ofc_net_unregister_config_impl(OFC_HANDLE hEvent) {
}

#define MAX_IFS 16

OFC_INT ofc_net_interface_count_impl(OFC_VOID) {
  int sock;
  int max_count;
  struct ifconf ifconf;
  struct ifreq ifreq;
  int ret;
  int i;
  struct ifreq *pifreq;
  struct sockaddr *psockaddr ;

  /* Create an unbound datagram socket to do the SIOCGIFADDR ioctl on. */
  max_count = 0 ;
  sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP) ;
  if (sock >= 0)
    {
      /* Get the interface configuration information... */
      ifconf.ifc_len = MAX_IFS * sizeof (struct ifreq) ;
      ifconf.ifc_ifcu.ifcu_buf = ofc_malloc (ifconf.ifc_len) ;

      ret = ioctl (sock, SIOCGIFCONF, &ifconf) ;
      if (ret >= 0)
	{
	  pifreq = (struct ifreq *) ifconf.ifc_req ;
	  for (i = 0 ; i < ifconf.ifc_len ; i += sizeof (struct ifreq) )
	    {
	      psockaddr = (struct sockaddr *) &pifreq->ifr_addr ;
	      if (psockaddr->sa_family == AF_INET)
		{
		  ofc_strncpy (ifreq.ifr_name, pifreq->ifr_name, IFNAMSIZ) ;
                  if ((ofc_strncmp(ifreq.ifr_name, "wlan", 4) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "eth", 3) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "tun", 3) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "p2p-wlan", 8) == 0))
                    {
                      if (ioctl (sock, SIOCGIFFLAGS, &ifreq) >= 0)
                        {
                          if (ifreq.ifr_flags & IFF_UP && 
                              !(ifreq.ifr_flags & IFF_LOOPBACK))
                            {
                              ++max_count;
                            }
                        }
                      else
                        {
                          int err ;
                          err = errno ;
                        }
                    }
		}
	      pifreq++ ;
	    }
	}
      ofc_free (ifconf.ifc_ifcu.ifcu_buf) ;

      close(sock);
    }

  return(max_count);
}

OFC_VOID ofc_net_interface_addr_impl(OFC_INT index,
                                     OFC_IPADDR *pinaddr,
                                     OFC_IPADDR *pbcast,
                                     OFC_IPADDR *pmask)
{
  int sock ;
  OFC_INT max_count ;
  struct ifconf ifconf ;
  struct ifreq ifreq ;
  int ret ;
  int i;
  struct ifreq *pifreq;
  OFC_BOOL found ;
  struct sockaddr_in *pAddrInet ;
  struct sockaddr *psockaddr ;

  if (pinaddr != OFC_NULL)
    {
      pinaddr->ip_version = OFC_FAMILY_IP ;
      pinaddr->u.ipv4.addr = OFC_INADDR_NONE ;
    }
  if (pbcast != OFC_NULL)
    {
      pbcast->ip_version = OFC_FAMILY_IP ;
      pbcast->u.ipv4.addr = OFC_INADDR_NONE ;
    }
  if (pmask != OFC_NULL)
    {
      pmask->ip_version = OFC_FAMILY_IP ;
      pmask->u.ipv4.addr = OFC_INADDR_NONE ;
    }

  /* Create an unbound datagram socket to do the SIOCGIFADDR ioctl on. */
  max_count = 0 ;
  sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP) ;
  if (sock >= 0)
    {
      /* Get the interface configuration information... */
      ifconf.ifc_len = MAX_IFS * sizeof (struct ifreq) ;
      ifconf.ifc_ifcu.ifcu_buf = ofc_malloc (ifconf.ifc_len) ;

      ret = ioctl (sock, SIOCGIFCONF, &ifconf) ;
      if (ret >= 0)
	{
	  pifreq = (struct ifreq *) ifconf.ifc_req ;
	  found = OFC_FALSE ;
	  for (i = 0 ; i < ifconf.ifc_len && !found; 
	       i += sizeof (struct ifreq) )
	    {
	      psockaddr = (struct sockaddr *) &pifreq->ifr_addr ;
	      if (psockaddr->sa_family == AF_INET)
		{
		  ofc_strncpy (ifreq.ifr_name, pifreq->ifr_name, IFNAMSIZ) ;
                  if ((ofc_strncmp(ifreq.ifr_name, "wlan", 4) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "eth", 3) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "tun", 3) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "p2p-wlan", 8) == 0))
                    {
                      if (ioctl (sock, SIOCGIFFLAGS, &ifreq) >= 0)
                        {
                          if (ifreq.ifr_flags & IFF_UP && 
                              !(ifreq.ifr_flags & IFF_LOOPBACK))
                            {
                              if (index == max_count)
                                {
                                  found = OFC_TRUE ;
                                }
                              ++max_count ;
                            }
                        }
                    }
                }
	      if (!found)
		pifreq++ ;
	    }

	  if (found) 
	    {
	      if (pinaddr != OFC_NULL)
		{
		  ofc_strncpy (ifreq.ifr_name, pifreq->ifr_name, IFNAMSIZ) ;
		  if (ioctl (sock, SIOCGIFADDR, &ifreq) >= 0)
		    {
		      pAddrInet = (struct sockaddr_in *) &(ifreq.ifr_addr) ;
		      pinaddr->u.ipv4.addr = 
			OFC_NET_NTOL (&pAddrInet->sin_addr.s_addr, 0) ;
		    }
		}
	      if (pmask != OFC_NULL)
		{
		  ofc_strncpy (ifreq.ifr_name, pifreq->ifr_name, IFNAMSIZ) ;
		  if (ioctl (sock, SIOCGIFNETMASK, &ifreq) >= 0)
		    {
		      pAddrInet = (struct sockaddr_in *) &(ifreq.ifr_addr) ;
		      pmask->u.ipv4.addr =
			OFC_NET_NTOL (&pAddrInet->sin_addr.s_addr, 0) ;
		    }
		}
	      if (pbcast != OFC_NULL)
		{
		  ofc_strncpy (ifreq.ifr_name, pifreq->ifr_name, IFNAMSIZ) ;
		  if (ioctl (sock, SIOCGIFBRDADDR, &ifreq) >= 0)
		    {
		      pAddrInet = (struct sockaddr_in *) &(ifreq.ifr_addr) ;
		      pbcast->u.ipv4.addr = 
			OFC_NET_NTOL (&pAddrInet->sin_addr.s_addr, 0) ;
		    }
		}
	    }
	}
      ofc_free (ifconf.ifc_ifcu.ifcu_buf) ;

      close(sock);
    }
}

OFC_CORE_LIB OFC_VOID
ofc_net_interface_wins_impl(OFC_INT index, OFC_INT *num_wins,
                            OFC_IPADDR **winslist) {
  /*
   * This is not provided by the platform
   */
  if (num_wins != OFC_NULL)
    *num_wins = 0 ;
  if (winslist != OFC_NULL)
    *winslist = OFC_NULL ;
}

#if defined(OFC_KERBEROS)
OFC_VOID ofc_net_resolve_svc(OFC_CCHAR *svc, OFC_UINT *count, OFC_CHAR ***dc)
{
  OFC_CHAR *ret = OFC_NULL;

  if (res_init() == 0)
    {
      unsigned char answer[PACKETSZ];
      int len = res_search(svc, C_IN, T_SRV, answer, sizeof(answer));
      if (len >= 0)
	{
	  ns_msg handle;
	  ns_rr rr;

	  ns_initparse(answer, len, &handle);
          *count = ns_msg_count(handle, ns_s_an);
          *dc = ofc_malloc(*count * (sizeof(OFC_CHAR *) * *count));
	  for (int i = 0; i < ns_msg_count(handle, ns_s_an) ; i++)
	    {
	      if (ns_parserr(&handle, ns_s_an, i, &rr) >= 0 &&
		  ns_rr_type(rr) == T_SRV)
		{
		  char dname[MAXCDNAME];
		  // decompress domain name
		  if (dn_expand(ns_msg_base(handle),
				ns_msg_end(handle),
				ns_rr_rdata(rr) + 3 * NS_INT16SZ,
				dname,
				sizeof(dname)) >= 0)
		    {
                      (*dc)[i] = ofc_strdup(dname);
		    }
		}
	    }
	}
      else
	{
	  ofc_log(OFC_LOG_WARN, "Could not resolve search for kerberos\n");
	}
    }
  else
    {
      ofc_log(OFC_LOG_WARN, "Could Not Init Resolver Library for getting Domain DC\n");
    }
}
#endif

//#if defined(ANDROID_MULTINETWORKING)
#if 0
OFC_VOID ofc_net_resolve_dns_name_impl(OFC_LPCSTR name,
                                       OFC_UINT16 *num_addrs,
                                       OFC_IPADDR *ip)
{
  net_handle_t network;
  struct addrinfo *res;
  struct addrinfo *p;
  struct addrinfo hints;
  int ret;

  OFC_INT i;
  OFC_INT j;
  OFC_IPADDR temp;

  if (g_network_handle == 0L)
    {
      ofc_log(OFC_LOG_FATAL, "Network Handle Not Set\n");
    }
  else
    {
      ofc_memset((OFC_VOID *) &hints, 0, sizeof(hints));

#if defined(OFC_DISCOVER_IPV6)
#if defined(OFC_DISCOVER_IPV4)
      hints.ai_family = AF_UNSPEC;
#else
      hints.ai_family = AF_INET6 ;
#endif

#else
#if defined(OFC_DISCOVER_IPV4)
      hints.ai_family = AF_INET ;
#else
#error "Neither IPv4 nor IPv6 Configured"
#endif
#endif
      hints.ai_socktype = 0;
      hints.ai_flags = AI_ADDRCONFIG;

      if (ofc_pton(name, &temp) != 0)
        hints.ai_flags |= AI_NUMERICHOST;

      res = NULL;
      ret = android_getaddrinfofornetwork((net_handle_t) g_network_handle,
                                          name, NULL, &hints, &res);

      if (ret != 0)
        *num_addrs = 0;
      else
        {
          for (i = 0, p = res; p != NULL && i < *num_addrs; i++, p = p->ai_next)
            {
              if (p->ai_family == AF_INET)
                {
                  struct sockaddr_in *sa;
                  sa = (struct sockaddr_in *) p->ai_addr;

                  ip[i].ip_version = OFC_FAMILY_IP;
                  ip[i].u.ipv4.addr = OFC_NET_NTOL (&sa->sin_addr.s_addr, 0);
                }
              else if (p->ai_family == AF_INET6)
                {
                  struct sockaddr_in6 *sa6;
                  sa6 = (struct sockaddr_in6 *) p->ai_addr;

                  ip[i].ip_version = OFC_FAMILY_IPV6;
                  for (j = 0; j < 16; j++)
                    {
                      ip[i].u.ipv6._s6_addr[j] =
                        sa6->sin6_addr.s6_addr[j];
                    }
                  ip[i].u.ipv6.scope = sa6->sin6_scope_id;
                }
              OFC_CHAR ip_str[IPSTR_LEN];
              ofc_ntop(&ip[i], ip_str, IPSTR_LEN);
              ofc_printf("Resolving %s to %s\n", name, ip_str);
            }
          freeaddrinfo(res);
          *num_addrs = i;
        }
    }
}
#else
OFC_VOID ofc_net_resolve_dns_name_impl(OFC_LPCSTR name,
                                       OFC_UINT16 *num_addrs,
                                       OFC_IPADDR *ip)
{
  struct hostent *hentry ;
  OFC_INT i ;

  hentry = gethostbyname (name) ;
  i = 0 ;

  if (hentry != NULL)
    {
      for (i = 0 ; (hentry->h_addr_list[i] != OFC_NULL) && i <  *num_addrs ; 
	   i++)
	{
          OFC_CHAR dst[80];

	  ip[i].ip_version = OFC_FAMILY_IP ;
	  ip[i].u.ipv4.addr = OFC_NET_NTOL (hentry->h_addr_list[i], 0) ;
	}
    }
  *num_addrs = i ;
}
#endif

OFC_CORE_LIB OFC_VOID
ofc_net_set_handle_impl(OFC_UINT64 network_handle)
{
  g_network_handle = network_handle;
}
/** \} */
