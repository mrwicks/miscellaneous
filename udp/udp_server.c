#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

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

    iLength = recvfrom (iFd, buffer, sizeof (buffer)-1, 0, NULL, 0);
    if ( iLength < 0 )
    {
      perror ("recvfrom");
      break;
    }

    buffer[iLength] = '\0';
    printf("%d bytes: '%s'\n", iLength, buffer);
  }

  close (iFd);
}
