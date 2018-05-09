#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <sys/types.h>
#include <netdb.h>


#include <ctype.h>
#define DUMP_MEM(x) dumpMem (#x, (void *)&(x), sizeof (x))
static void dumpMem (const char *szNote, const void *vData, unsigned int len)
{
  unsigned int i,j;
  const unsigned char *ucData = (unsigned char *)vData;
  
  if (len)
  {
    printf ("\n");
    i = printf ("%s: Mem Dump\n", szNote);
    for (j = 0 ; j < i ; j++)
    {
      printf ("-");
    }
    printf ("\n");

    for (i = 0 ; i < len ; i+=16)
    {
      for (j = i ; j < i+16 ; j++)
      {
        if (j < len)
        {
          printf ("%02x ", ucData[j]);
        }
        else
        {
          printf ("-- ");
        }
      }

      printf ("| ");

      for (j = i ; j < i+16 ; j++)
      {
        if (j < len)
        {
          printf ("%c",  isprint (ucData[j]) ? ucData[j] : '.');
        }
      }

      printf ("\n");
    }
  }
  else
  {
    printf ("Mem Dump length == 0 for \"%s\"\n", szNote);
  }
}

#define DUMP(x) printf ("%s: %08x\n", #x, (unsigned int)x);

int getIpv4Address (const char *szHostName, struct in_addr *inAddr)
{
  struct addrinfo *addrInfoRes;
  struct addrinfo *addrInfoIter;
  int iError;

  iError = getaddrinfo (szHostName, NULL, NULL, &addrInfoRes);
  if (iError != 0)
  {
    fprintf (stderr, "error with getaddrinfo: %s\n", gai_strerror(iError));
    exit (-1);
  }

  iError = -1;
  for (addrInfoIter = addrInfoRes ; addrInfoIter != NULL ; addrInfoIter = addrInfoIter->ai_next)
  {
#if 1
    printf ("\n=====================================\n");
    DUMP_MEM (addrInfoIter->ai_flags);
    DUMP_MEM (addrInfoIter->ai_family);
    DUMP_MEM (addrInfoIter->ai_socktype);

    DUMP (SOCK_STREAM);
    DUMP (SOCK_DGRAM);
    DUMP (SOCK_SEQPACKET);
    DUMP (SOCK_RAW);
    DUMP (SOCK_RDM);
    DUMP (SOCK_PACKET);
    DUMP (SOCK_NONBLOCK);
    DUMP (SOCK_CLOEXEC );
    
    DUMP_MEM (addrInfoIter->ai_protocol);
    DUMP_MEM (addrInfoIter->ai_addrlen);
    DUMP_MEM (addrInfoIter->ai_addr);
    DUMP_MEM (addrInfoIter->ai_canonname);
#endif
    
    if (addrInfoRes->ai_family == AF_INET)
    {
      iError = 0;
      // (struct addrinfo *) contains s_addr which is a POINTER to a struct sockaddr
      // which can be cast to a struct sockaddr_in *
      // Why unions aren't used, is never explained.
      *inAddr = ((struct sockaddr_in *)addrInfoRes->ai_addr)->sin_addr;
      printf ("found\n");
//      break;
    }
  }
  freeaddrinfo (addrInfoRes);
  return iError;
}

int main (int argc, char **argv)
{
  int iFd;
  int iPort = 4000;
  struct sockaddr_in serveraddr;
  const char *szMessage = "hello";
  const char *szHostName = "127.0.0.1";

  if (argc >= 2)
  {
    iPort = atoi (argv[1]);
  }
  if (argc >= 3)
  {
    szHostName = argv[2];
  }
  if (argc >= 4)
  {
    szMessage = argv[3];
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
  //serveraddr.sin_addr.s_addr = htonl (0x7f000001);
  getIpv4Address (szHostName, &(serveraddr.sin_addr));

  printf ("UDP client sending to port %d\n", iPort);

  if (sendto (iFd, szMessage, strlen (szMessage), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 )
  {
    perror ("sendto");
  }
  printf ("message sent\n");
  
  close( iFd );
}
