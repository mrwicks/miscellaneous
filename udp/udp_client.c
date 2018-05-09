#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main (int argc, char **argv)
{
  int iFd;
  int iPort = 4000;
  struct sockaddr_in serveraddr;
  const char *szMessage = "hello";

  if (argc >= 2)
  {
    iPort = atoi (argv[1]);
  }
  if (argc >= 3)
  {
    szMessage = argv[2];
  }
  
  iFd = socket (AF_INET, SOCK_DGRAM, 0);
  if (iFd < 0)
  {
    perror("socket");
    return 1;
  }
  
  memset( &serveraddr, 0, sizeof (serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons (iPort);              
  serveraddr.sin_addr.s_addr = htonl( 0x7f000001 );  

  printf ("UDP client sending to port %d\n", iPort);

  if (sendto (iFd, szMessage, strlen (szMessage), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 )
  {
    perror ("sendto");
  }
  printf ("message sent\n");
  
  close( iFd );
}
