#ifndef __IP_CLASS__
#define __IP_CLASS__

#include <sys/types.h>
#include <sys/socket.h>
#include <string>

class ipClass
{
private:
  int mi_Fd;

public:
  static struct in6_addr *sockaddr_in_to_6 (struct in_addr *pin, struct in6_addr *pin6);
  static std::string inet_ntop (const struct in6_addr &in6);
  static bool inet_pton (const std::string &src, struct in6_addr *pbuf);
  static unsigned short str_to_port (const std::string &strPort);

  ipClass ();
  ~ipClass ();

  // TCP
  bool tcpServer (const struct sockaddr_in6 &in6, int iBacklog=SOMAXCONN);
  bool tcpServer (const std::string &strIp, unsigned short usPort, int iBacklog=SOMAXCONN, int iScope=0);
  bool tcpServer (const std::string &strIp, unsigned short usPort, int iBacklog, const std::string &strScope);

  bool tcpClient (const struct sockaddr_in6 &in6);
  bool tcpClient (const std::string &strIp, unsigned short usPort, int iScope=0);
  bool tcpClient (const std::string &strIp, unsigned short usPort, const std::string &strScope);

  bool accept (ipClass *pClass, struct sockaddr_in6 *pcli_addr=NULL, int flags=0);

  // UDP
  bool udpServer (const struct sockaddr_in6 &in6);
  bool udpServer (const std::string &strIp, unsigned short usPort, int iScope=0);
  bool udpServer (const std::string &strIp, unsigned short usPort, const std::string &strScope);

  bool udpClient (void); // convenience functions below for setting up a destination sockaddr_in6
  bool udpClient (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, int iScope=0);
  bool udpClient (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, const std::string &strScope);

  // TCP or UDP
  int name_to_scope (const std::string &strScope);

  bool setup_sockaddr (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, int iScope=0);
  bool setup_sockaddr (struct sockaddr_in6 *pin6, const std::string &strIp, unsigned short usPort, const std::string &strScope);

  int recv (void *vBuffer, int iLen, int iFlags=0, sockaddr_in6 *pin6=NULL);
  int send (const void *vBuffer, int iLen, int iFlags=0, const sockaddr_in6 *pout6=NULL);
  int send (const std::string &str, int iFlags=0, const sockaddr_in6 *pout6=NULL);
  bool close (void);
};

#endif //__IP_CLASS__
