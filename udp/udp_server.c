#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/ip.h> 
#include <netdb.h>

int main (int argc, char **argv)
{
  int iPort = 4000;
  int iFd;
  struct sockaddr_in serveraddr;

  iFd = socket (AF_INET, SOCK_DGRAM, 0);
  if ( iFd < 0 )
  {
    perror ("socket");
    return 1;
  }

  if (argc >= 2)
  {
    iPort = atoi (argv[1]);
  }
  printf ("Server running on port %d\n", iPort);
  
  memset (&serveraddr, 0, sizeof (serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons (iPort);
  serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);

  if (bind (iFd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
  {
    perror ("bind");
    return 1;
  }

  for ( ;; )
  {
    char buffer[200];
    int iLength;
    struct sockaddr src_addr;
    socklen_t addrlen;
    struct sockaddr_in *in4 = (struct sockaddr_in *)&src_addr;
    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&src_addr;
    char hostname [NI_MAXHOST] = "";
    int iError = 0;

    addrlen = sizeof (src_addr);
    iLength = recvfrom (iFd, buffer, sizeof (buffer)-1, 0, &src_addr, &addrlen);
    if (iLength < 0)
    {
      perror ("recvfrom");
      break;
    }

    buffer[iLength] = '\0';
    printf("%d bytes: '%s': ", iLength, buffer);

    
    iError = getnameinfo (&src_addr, addrlen, hostname, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (iError != 0)
    {
      fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(iError));
      hostname[0] = '\0';
    }

    switch (src_addr.sa_family)
    {
    case AF_INET:
      printf ("IPV4, port %d, IP %s\n", in4->sin_port, hostname);
      break;
    case AF_INET6:
      printf ("IPV6, port %d, IP %s\n", in6->sin6_port, hostname);
      break;
    default:
      printf ("What family is this?\n");
    }
  }

  close (iFd);
}
