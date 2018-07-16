#include "ipClass.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <string.h>
#include <arpa/inet.h>

#include <string>

#include <errno.h>
#include <resolv.h>

// https://stackoverflow.com/questions/34349217/get-subject-and-issuer-information-from-the-x509

struct in6_addr *ipClass::sockaddr_in_to_6 (struct in_addr *pin, struct in6_addr *pout)
{
  // as IPV6 IPV4 address is stored as 0000:0000:0000:0000:0000:FFFF:AABB:CCDD
  // where IPV4 address is AA.BB.CC.DD
  pout->s6_addr[0x0c] = ( (struct in6_addr *)pin)->s6_addr[0x00];
  pout->s6_addr[0x0d] = ( (struct in6_addr *)pin)->s6_addr[0x01];
  pout->s6_addr[0x0e] = ( (struct in6_addr *)pin)->s6_addr[0x02];
  pout->s6_addr[0x0f] = ( (struct in6_addr *)pin)->s6_addr[0x03];

  pout->s6_addr[0x00] = 0x00;
  pout->s6_addr[0x01] = 0x00;
  pout->s6_addr[0x02] = 0x00;
  pout->s6_addr[0x03] = 0x00;
  pout->s6_addr[0x04] = 0x00;
  pout->s6_addr[0x05] = 0x00;
  pout->s6_addr[0x06] = 0x00;
  pout->s6_addr[0x06] = 0x00;
  pout->s6_addr[0x07] = 0x00;
  pout->s6_addr[0x08] = 0x00;
  pout->s6_addr[0x09] = 0x00;

  pout->s6_addr[0x0a] = 0xFF;
  pout->s6_addr[0x0b] = 0xFF;

  return pout;
}

std::string ipClass::inet_ntop (const struct in6_addr &in6)
{
  char dst[INET6_ADDRSTRLEN];
  std::string str ("");

  if (::inet_ntop (AF_INET6, (const char *)&in6, dst, INET6_ADDRSTRLEN) == NULL)
  {
    perror ("inet_ntop");
  }
  else
  {
    str = dst;
  }
  return str;
}

bool ipClass::inet_pton (const std::string &src, struct in6_addr *pin6)
{
  int af = AF_INET;
  int iRet;
  const char *szPtr;

  // determine if this is in an IPV4 or IPV6 format
  for (szPtr = src.c_str () ; *szPtr != '\0' && af == AF_INET ; szPtr++)
  {
    switch (*szPtr)
    {
    case '0' :
    case '1' :
    case '2' :
    case '3' :
    case '4' :
    case '5' :
    case '6' :
    case '7' :
    case '8' :
    case '9' :
    case '.' :
      break;
    case 'a' :
    case 'b' :
    case 'c' :
    case 'd' :
    case 'e' :
    case 'f' :
    case 'A' :
    case 'B' :
    case 'C' :
    case 'D' :
    case 'E' :
    case 'F' :
    case ':' :
      af = AF_INET6;
      break;
    }
  }

  iRet = ::inet_pton (af, src.c_str (), (char *)pin6); // WTF is it with casting and the BSD socket interface??
  if (iRet == 1 && af == AF_INET)
  {
    // convert from IPV4 to IPV6
    ipClass::sockaddr_in_to_6 ( (struct in_addr *)pin6, pin6);
  }

  return iRet==1 ? true:false;
}

unsigned short ipClass::str_to_port (const std::string &strPort)
{
  unsigned long long ull;
  char *szEnd;
  unsigned short usPort = 0;

  ull = ::strtoull (strPort.c_str(), &szEnd, 0);

  if (*szEnd != '\0')
  {
    ::fprintf (stderr, "%s isn't all digits..\n", strPort.c_str());
  }
  else if (ull > 0xffff)
  {
    ::fprintf (stderr, "%s is larger than 16 bits..\n", strPort.c_str());
  }
  else
  {
    usPort = (unsigned short)ull;
  }

  return usPort;
}

ipClass::ipClass ()
{
  mi_Fd = -1;
}

ipClass::~ipClass ()
{
  this->close ();
}

bool ipClass::tcpServer (const struct sockaddr_in6 &in6, int iBacklog)
{
  bool bRet = false;

  mi_Fd = ::socket (AF_INET6, SOCK_STREAM, 0);
  if (mi_Fd < 0)
  {
    perror ("socket");
  }
  else
  {
    if (::bind (mi_Fd, (const struct sockaddr *)&in6, sizeof (in6)) != 0)
    {
      ::perror ("bind");
      ::close (mi_Fd);
      mi_Fd = -1;
    }
    else
    {
      if (::listen (mi_Fd, iBacklog) != 0) // RBW: 5 - magic number
      {
        ::perror ("listen");
        ::close (mi_Fd);
        mi_Fd = -1;
      }
      else
      {
        bRet = true;
      }
    }
  }

  return bRet;
}

bool ipClass::tcpServer (const std::string &strIp, unsigned short usPort, int iBacklog, int iScope)
{
  struct sockaddr_in6 in6;
  bool bRet;

  bRet = this->setup_sockaddr (&in6, strIp, usPort, iScope);

  if (bRet == true)
  {
    bRet = this->tcpServer (in6, iBacklog);
  }

  return bRet;
}

bool ipClass::tcpServer (const std::string &strIp, unsigned short usPort, int iBacklog, const std::string &strScope)
{
  int iScope = ipClass::name_to_scope (strScope);

  return this->tcpServer (strIp, usPort, iBacklog, iScope);
}

bool ipClass::tcpClient (const struct sockaddr_in6 &in6)
{
  int sockfd;
  bool bRet;

  sockfd = ::socket (AF_INET6, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    ::perror ("socket");
    bRet = false;
  }
  else
  {
    if (::connect (sockfd, (const struct sockaddr *) &in6, sizeof(in6)) < 0)
    {
      ::perror ("ERROR connecting");
      ::close (sockfd);
      bRet = false;
    }
    else
    {
      mi_Fd = sockfd;
      bRet = true;
    }
  }

  return bRet;
}

bool ipClass::tcpClient (const std::string &strIp, unsigned short usPort, int iScope)
{
  struct sockaddr_in6 in6;
  bool bRet;

  bRet = this->setup_sockaddr (&in6, strIp, usPort, iScope);

  if (bRet == true)
  {
    bRet = this->tcpClient (in6);
  }
  return bRet;
}

bool ipClass::tcpClient (const std::string &strIp, unsigned short usPort, const std::string &strScope)
{
  int iScope = name_to_scope (strScope);

  return this->tcpClient (strIp, usPort, iScope);
}

bool ipClass::accept (ipClass *pClass, struct sockaddr_in6 *paddr, int flags)
{
  socklen_t addrlen=0;
  int iRet;
  bool bVal = false;

  if (mi_Fd != -1)
  {
    if (paddr != NULL)
    {
      addrlen = sizeof (*paddr);
    }

    iRet = ::accept4 (mi_Fd, (struct sockaddr *)paddr, &addrlen, flags);
    if (iRet == -1)
    {
      ::perror ("accept");
    }
    else
    {
      pClass->mi_Fd = iRet;
      bVal = true;
    }
  }

  return bVal;
}

bool ipClass::udpServer (const struct sockaddr_in6 &in6)
{
  bool bRet = false;

  mi_Fd = ::socket (AF_INET6, SOCK_DGRAM, 0);

  if (mi_Fd < 0)
  {
    ::perror ("socket");
  }
  else
  {
    if (::bind (mi_Fd, (const struct sockaddr *)&in6, sizeof (in6)) != 0)
    {
      ::perror ("bind");
      ::close (mi_Fd);
      mi_Fd = -1;
    }
    else
    {
      bRet = true;
    }
  }

  return bRet;
}

bool ipClass::udpServer (const std::string &strIp, unsigned short usPort, int iScope)
{
  struct sockaddr_in6 in6;
  bool bRet;

  bRet = this->setup_sockaddr (&in6, strIp, usPort, iScope);

  if (bRet == true)
  {
    bRet = this->udpServer (in6);
  }
  return bRet;
}

bool ipClass::udpServer (const std::string &strIp, unsigned short usPort, const std::string &strScope)
{
  int iScope = name_to_scope (strScope);

  return this->udpServer (strIp, usPort, iScope);
}

bool ipClass::udpClient (void)
{
  bool bRet = false;
  int sockfd;

  sockfd = ::socket (AF_INET6, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    ::perror ("socket");
  }
  else
  {
    mi_Fd = sockfd;
    bRet = true;
  }

  return bRet;
}

bool ipClass::udpClient (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, int iScope)
{
  bool bRet = true;

  if (pin6 != NULL)
  {
    bRet = this->setup_sockaddr (pin6, strIp, usPort, iScope);
  }

  if (bRet == true)
  {
    bRet = this->udpClient ();
  }

  return bRet;
}

bool ipClass::udpClient (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, const std::string &strScope)
{
  int iScope = name_to_scope (strScope);

  return this->udpClient (pin6, strIp, usPort, iScope);
}

int ipClass::name_to_scope (const std::string &strScope)
{
  struct ifreq ifreqVal;
  int iScope;

  ::memset (&ifreqVal, 0, sizeof (ifreqVal));
  ::strncpy (ifreqVal.ifr_name, strScope.c_str(), sizeof (ifreqVal.ifr_name));
  if (::ioctl (mi_Fd, SIOCGIFINDEX, &ifreqVal) == 0)
  {
    iScope = ifreqVal.ifr_ifindex;
  }
  else
  {
    ::perror ("ioctl");
    iScope = -1;
  }

  return iScope;
}

bool ipClass::setup_sockaddr (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, int iScope)
{
  sockaddr_in6 tmp = {0};
  bool bRet = true;

  *pin6 = tmp;
  pin6->sin6_family = AF_INET6;
  pin6->sin6_port = htons (usPort);
  pin6->sin6_flowinfo = 0;
  if (strIp == "")
  {
    pin6->sin6_addr = in6addr_any;
  }
  else
  {
    if (this->inet_pton (strIp, &pin6->sin6_addr) == false)
    {
      ::perror ("inet_pton");
      bRet = false;
    }
    pin6->sin6_port = htons (usPort);
  }
  pin6->sin6_scope_id = iScope;

  return bRet;
}

bool ipClass::setup_sockaddr (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, const std::string &strScope)
{
  int iScope = name_to_scope (strScope);

  return this->setup_sockaddr (pin6, strIp, usPort, iScope);
}

int ipClass::recv (void *vBuffer, int iLen, int iFlags, sockaddr_in6 *pin6)
{
  int iRet;

  if (pin6 == NULL)
  {
    iRet = (int)::recv (mi_Fd, vBuffer, (size_t)iLen, iFlags);
    if (iRet == -1)
    {
      ::perror ("recv");
    }
  }
  else
  {
    socklen_t addrlen = sizeof (*pin6);

    iRet = (int)::recvfrom (mi_Fd, vBuffer, (size_t)iLen, iFlags, (struct sockaddr *)pin6, &addrlen);
    if (iRet == -1)
    {
      ::perror ("recvfrom");
    }
  }
  return iRet;
}

int ipClass::send (const void *vBuffer, int iLen, int iFlags, const sockaddr_in6 *pout6)
{
  int iRet;

  if (pout6 == NULL)
  {
    iRet = (int)::send (mi_Fd, vBuffer, (size_t)iLen, iFlags);
  }
  else
  {
    iRet = (int)::sendto (mi_Fd, vBuffer, (size_t)iLen, iFlags, (const sockaddr *)pout6, sizeof (*pout6));
  }

  return iRet;
}

int ipClass::send (const std::string &str, int iFlags, const sockaddr_in6 *pout6)
{
  return this->send ((void *)str.c_str(), (int)str.size(), iFlags, pout6);
}


bool ipClass::close (void)
{
  if (mi_Fd != -1)
  {
    if (::close (mi_Fd) != 0)
    {
      ::perror ("close");
    }
    else
    {
      mi_Fd = -1;
    }
  }

  return (mi_Fd == -1) ? true:false;
}

/*------ all this is just example code ------*/
int tcpS (int argc, char **argv)
{
  unsigned short usPort=7777;
  char szBuffer[1000];
  ipClass ipServer;
  int n;
  std::string strIpAddr = "";

  if (argc >= 2)
  {
    strIpAddr = argv[1];
  }
  if (argc >= 3)
  {
    usPort = ipClass::str_to_port (argv[2]);
  }

  printf ("\nIPv6 TCP Server Started on port %d...\n", usPort);

  if (ipServer.tcpServer (strIpAddr, usPort) == false)
  {
    perror ("ipServer.tcpServer");
    exit (1);
  }

  for (;;)
  {
    ipClass ipClient;

    if (ipServer.accept (&ipClient) != true)
    {
      perror ("accept");
    }

    for (;;)
    {
      //Sockets Layer Call: recv ()
      n = ipClient.recv (szBuffer, sizeof (szBuffer)-1);
      if (n <= 0)
      {
        perror ("ERROR reading from socket");
        break;
      }
      szBuffer[n] = '\0';

      printf ("Message from client: %s\n", szBuffer);

      // reverse it
      for (int iIter = 0 ; iIter < n/2 ; iIter++)
      {
        unsigned char ucTmp;

        ucTmp = szBuffer[iIter];
        szBuffer[iIter] = szBuffer[(n-1)-iIter];
        szBuffer[(n-1)-iIter] = ucTmp;
      }

      printf ("Message returned:    %s\n", szBuffer);

      //Sockets Layer Call: send ()
      n = ipClient.send (szBuffer, n);
      if (n < 0)
      {
        perror ("ERROR writing to socket");
        break;
      }
    }
  }

  return 0;
}

int tcpC (int argc, char **argv)
{
  ipClass ipClient;
  std::string strServer = "127.0.0.1";
  unsigned short usPort = 7777;

  if (argc >= 2)
  {
    strServer = argv[1];
  }
  if (argc >= 3)
  {
    usPort = ipClass::str_to_port (argv[2]);
  }

  ipClient.tcpClient (strServer, usPort);

  for (int iIter = 3 ; iIter < argc ; iIter++)
  {
    std::string str;
    char szBuffer[1000];
    int iLen;

    str = argv[iIter];
    iLen = str.size ();

    ipClient.send (str.c_str(), iLen);
    iLen = ipClient.recv (szBuffer, sizeof (szBuffer)-1);
    szBuffer[iLen] = '\0';

    printf ("Sent: %s\n", str.c_str());
    printf ("Recv: %s\n", szBuffer);
    printf ("\n");
  }

  return 0;
}

int udpS (int argc, char **argv)
{
  unsigned short usPort=7777;
  char szBuffer[1000];
  ipClass ipServer;
  int n;
  std::string strIpAddr = "";
  sockaddr_in6 clientAddr;

  if (argc >= 2)
  {
    strIpAddr = argv[1];
  }
  if (argc >= 3)
  {
    usPort = ipClass::str_to_port (argv[2]);
  }

  printf ("\nIPv6 UDP Server Started on port %d...\n", usPort);

  if (ipServer.udpServer (strIpAddr, usPort) == false)
  {
    perror ("ipServer.udpServer");
    exit (1);
  }

  for (;;)
  {
    std::string strInAddr;

    //Sockets Layer Call: recv ()
    n = ipServer.recv (szBuffer, sizeof (szBuffer)-1, 0, &clientAddr);
    if (n <= 0)
    {
      perror ("ERROR reading from socket");
      break;
    }
    szBuffer[n] = '\0';
    strInAddr = ipServer.inet_ntop (clientAddr.sin6_addr);

    printf ("Message from client: %s [%s]:%d\n", szBuffer, strInAddr.c_str(), htons (clientAddr.sin6_port));

    // reverse it
    for (int iIter = 0 ; iIter < n/2 ; iIter++)
    {
      unsigned char ucTmp;

      ucTmp = szBuffer[iIter];
      szBuffer[iIter] = szBuffer[(n-1)-iIter];
      szBuffer[(n-1)-iIter] = ucTmp;
    }

    printf ("\n");

    printf ("Message returned:    %s\n", szBuffer);

    //Sockets Layer Call: send ()
    n = ipServer.send (szBuffer, n, 0, &clientAddr);
    if (n < 0)
    {
      perror ("ERROR writing to socket");
      break;
    }
  }

  return 0;
}

int udpC (int argc, char **argv)
{
  ipClass ipClient;
  std::string strServer = "127.0.0.1";
  unsigned short usPort = 7777;
  sockaddr_in6 serverAddr;

  if (argc >= 2)
  {
    strServer = argv[1];
  }
  if (argc >= 3)
  {
    usPort = ipClass::str_to_port (argv[2]);
  }

  ipClient.udpClient (&serverAddr, strServer, usPort);

  for (int iIter = 3 ; iIter < argc ; iIter++)
  {
    std::string str;
    char szBuffer[1000];
    int iLen;
    sockaddr_in6 recvFromAddr;
    std::string strInAddr;

    str = argv[iIter];

    ipClient.send (str, 0, &serverAddr);
    iLen = ipClient.recv (szBuffer, sizeof (szBuffer)-1, 0, &recvFromAddr);
    szBuffer[iLen] = '\0';

    strInAddr = ipClient.inet_ntop (recvFromAddr.sin6_addr);
    printf ("Sent: %s\n", str.c_str());
    printf ("Recv: %s [%s]:%d\n", szBuffer, strInAddr.c_str(), htons(recvFromAddr.sin6_port));
    printf ("\n");
  }

  return 0;
}

int main (int argc, char **argv)
{
  int iRet;
  char *szProgName = argv[0];

  for (char *szPtr = argv[0]; *szPtr != '\0' ; szPtr++)
  {
    if (*szPtr == '/')
    {
      szProgName = szPtr+1;
    }
  }

  if (strcmp (szProgName, "tcpS") == 0)
  {
    iRet = tcpS (argc, argv);
  }
  else if (strcmp (szProgName, "tcpC") == 0)
  {
    iRet = tcpC (argc, argv);
  }
  else if (strcmp (szProgName, "udpS") == 0)
  {
    iRet = udpS (argc, argv);
  }
  else if (strcmp (szProgName, "udpC") == 0)
  {
    iRet = udpC (argc, argv);
  }
  else
  {
    printf ("Invalid program name %s for this program\n", szProgName);
    printf ("The program runs different code depending on it's name\n");
    printf ("\n");
    printf ("Execute the following to make symbolic links:\n");
    printf ("  ln -s %s tcpS\n", szProgName);
    printf ("  ln -s %s tcpC\n", szProgName);
    printf ("  ln -s %s udpS\n", szProgName);
    printf ("  ln -s %s udpC\n", szProgName);
    printf ("\n");
    printf ("Usage is:\n");
    printf ("  tcpS <IP_address> <port>\n");
    printf ("  tcpC <IP_address> <port>\n");
    printf ("  updS <IP_address> <port>\n");
    printf ("  udpC <IP_address> <port>\n");
    printf ("\n");
    printf ("You can use the InAny interface (0.0.0.0) if you specify IP_address to ""\n");
    printf ("\n");
    iRet = 1;
  }

  return iRet;
}
