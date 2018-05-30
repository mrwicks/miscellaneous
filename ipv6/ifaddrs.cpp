#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>

// struct ifaddrs
// {
//   struct ifaddrs  *ifa_next;    /* Next item in list */
//   char            *ifa_name;    /* Name of interface */
//   unsigned int     ifa_flags;   /* Flags from SIOCGIFFLAGS */
//   struct sockaddr *ifa_addr;    /* Address of interface */
//   struct sockaddr *ifa_netmask; /* Netmask of interface */
//   union {
//     struct sockaddr *ifu_broadaddr;
//     /* Broadcast address of interface */
//     struct sockaddr *ifu_dstaddr;
//                                     /* Point-to-point destination address */
//   } ifa_ifu;
// #define              ifa_broadaddr ifa_ifu.ifu_broadaddr
// #define              ifa_dstaddr   ifa_ifu.ifu_dstaddr
//                void            *ifa_data;    /* Address-specific data */
/// };

// getifaddrs
// getnameinfo

// structures
// sockaddr
// sockaddr_in *
// sockaddr_in6 *
// sockaddr_storage <- replaces socaddr

// from man ip (7)
//
// struct in_addr
// {
//   uint32_t       s_addr;     /* address in network byte order */
// };
//
// struct sockaddr_in
// {
//   sa_family_t    sin_family; /* address family: AF_INET */
//   in_port_t      sin_port;   /* port in network byte order */
//   struct in_addr sin_addr;   /* internet address */
// };
//
// from man ipv6 (7)
//
// struct in6_addr
// {
//   unsigned char   s6_addr[16];   /* IPv6 address */
// };
//
// struct sockaddr_in6
// {
//   sa_family_t     sin6_family;   /* AF_INET6 */
//   in_port_t       sin6_port;     /* port number */
//   uint32_t        sin6_flowinfo; /* IPv6 flow information */
//   struct in6_addr sin6_addr;     /* IPv6 address */
//   uint32_t        sin6_scope_id; /* Scope ID (new in 2.4) : this is ZONE id */
// };
//
// from man rtnetlink (7)
// rtnl_link_stats


bool isLinkLocal (struct sockaddr_in6 *inSockAddr6)
{
  if (inSockAddr6->sin6_family == AF_INET6 &&
      inSockAddr6->sin6_addr.s6_addr[0] == 0xfe &&
      inSockAddr6->sin6_addr.s6_addr[1] == 0x80)
  {
    return true;
  }
  return false;
}

/*
bool convertToIpV6 (struct sockaddr *inSockAddr, struct sockaddr_in6 *inSockAddr6)
{
  bool bConverted = false;
  
  if (((struct sockaddr_in *)inSockAddr)->sin_family == AF_INET6)
  {
    bConverted = true;
    inSockAddr6->sin6_addr = ((struct sockaddr_in6 *)inSockAddr)->sin6_addr;
  }
  if (((struct sockaddr_in *)inSockAddr)->sin_family == AF_INET)
  {
    size_t ip4Iter = 0;
    uint32_t ip4addr = ntohl (((struct sockaddr_in *)inSockAddr)->sin_addr.s_addr);
    
    bConverted = true;
    
    for (size_t iter = 0 ; iter < sizeof (inSockAddr6->sin6_addr) ; iter++)
    {
      switch (iter)
      {
      case 0:
      case 1:
        inSockAddr6->sin6_addr.s6_addr[iter] = 0xff;
        break;
      case sizeof (sizeof (inSockAddr6->sin6_addr)) - 4 :
      case sizeof (sizeof (inSockAddr6->sin6_addr)) - 3 :
      case sizeof (sizeof (inSockAddr6->sin6_addr)) - 2 :
      case sizeof (sizeof (inSockAddr6->sin6_addr)) - 1 :
        inSockAddr6->sin6_addr.s6_addr[iter] = ip4addr >> (8*ip4Iter);
        ip4Iter++;
        break;
      default:
        inSockAddr6->sin6_addr.s6_addr[iter] = 0x00;
        break;
      }
    }
  }

  return bConverted;
}
*/

void getifaddrs_example (void)
{
  struct ifaddrs *ifaddr;

  if (getifaddrs(&ifaddr) == -1)
  {
    perror ("getifaddrs");
  }
  else
  {
    char szHostName[NI_MAXHOST];
    struct ifaddrs *ifaddrIter;
    int iRet;

    // Walk through linked list, keeping head pointer so it can be freed later
    for (ifaddrIter = ifaddr; ifaddrIter != NULL ; ifaddrIter = ifaddrIter->ifa_next)
    {
      if (ifaddrIter->ifa_addr != NULL)
      {
        int iFamily;

        union
        {
          struct sockaddr_in *inSockAddr;
          struct sockaddr_in6 *inSockAddr6;
          struct trnl_link_stats *stats;
        };

        iFamily = ifaddrIter->ifa_addr->sa_family;
        switch (iFamily)
        {
        case AF_PACKET:
          stats = (struct trnl_link_stats *) ifaddrIter->ifa_data;
          printf ("AF_PACKET %s", ifaddrIter->ifa_name);
          break;

        case AF_INET6:
          inSockAddr6 = (struct sockaddr_in6 *)ifaddrIter->ifa_addr;
          printf ("INET6 %5s ", ifaddrIter->ifa_name);
          iRet = getnameinfo(ifaddrIter->ifa_addr,
                             sizeof(struct sockaddr_in6),
                             szHostName, sizeof (szHostName),
                             NULL, 0, NI_NUMERICHOST);
          if (iRet != 0)
          {
            printf("getnameinfo() failed: %s\n", gai_strerror(iRet));
            exit(EXIT_FAILURE);
          }

          printf ("<%30s>  ", szHostName);
          printf ("scopeId (zone): %d", inSockAddr6->sin6_scope_id);
          if (isLinkLocal (inSockAddr6) == true)
          {
            printf ("  (link local)");
          }
          break;

        case AF_INET:
          printf ("INET4 %5s ", ifaddrIter->ifa_name);
          inSockAddr = (struct sockaddr_in *)ifaddrIter->ifa_addr;
          iRet = getnameinfo(ifaddrIter->ifa_addr,
                             sizeof(struct sockaddr_in6),
                             szHostName, sizeof (szHostName),
                             NULL, 0, NI_NUMERICHOST);
          if (iRet != 0)
          {
            printf("getnameinfo() failed: %s\n", gai_strerror(iRet));
            exit(EXIT_FAILURE);
          }

          printf("<%30s>", szHostName);

          /*
          if (1)
          {
            struct sockaddr_in6 ipv6address;
            convertToIpV6 (ifaddrIter->ifa_addr, &ipv6address);
            
            iRet = getnameinfo((sockaddr *)&ipv6address,
                               sizeof(struct sockaddr_in6),
                               szHostName, sizeof (szHostName),
                               NULL, 0, NI_NUMERICHOST);
            if (iRet != 0)
            {
              printf("getnameinfo() failed: %s\n", gai_strerror(iRet));
              exit(EXIT_FAILURE);
            }
            
            printf("<%30s>", szHostName);
          }
          */
          break;
        }

        printf ("\n");
      }
    }

    freeifaddrs(ifaddr);
  }
}

int main (int argc, char **argv)
{
  getifaddrs_example ();
  
  return 0;
}
