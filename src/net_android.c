/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "ofc/core.h"
#include "ofc/types.h"
#include "ofc/config.h"
#include "ofc/libc.h"
#include "ofc/heap.h"
#include "ofc/net.h"
#include "ofc/net_internal.h"
#include "ofc/framework.h"

/**
 * \defgroup net_android Android Network Implementation
 */

/** \{ */

OFC_VOID ofc_net_init_impl(OFC_VOID) {
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
                  ofc_printf("Got interface %s\n", ifreq.ifr_name);
                  if ((ofc_strncmp(ifreq.ifr_name, "wlan", 4) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "eth", 4) == 0) ||
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
                          ofc_printf ("%d\n", err) ;
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
                  ofc_printf("in addr interface %s\n", ifreq.ifr_name);
                  if ((ofc_strncmp(ifreq.ifr_name, "wlan", 4) == 0) ||
                      (ofc_strncmp(ifreq.ifr_name, "eth", 4) == 0) ||
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
	  ip[i].ip_version = OFC_FAMILY_IP ;
	  ip[i].u.ipv4.addr = OFC_NET_NTOL (hentry->h_addr_list[i], 0) ;
	}
    }
  *num_addrs = i ;
}

/** \} */
