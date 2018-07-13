// This was taken from:
// --------------------
// https://aticleworld.com/ssl-server-client-using-openssl-in-c/
// gcc -Wall -o server server.c -L/usr/lib -lssl -lcrypto
// sudo ./server <portnum>

// this MIGHT be based off from:
// https://wiki.openssl.org/index.php/Simple_TLS_Server
//
// If you want to get a real certificate, go through these instructions.
//   https://certbot.eff.org/docs/intro.html
// Note, this is much easier to do if you have Apache running, and
// you need a dynamic name service such as: https://www.dynu.com

#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"

#include <sys/epoll.h>

#define FAIL    -1

#define USE_FUNCTIONS 0

void instructionsForPem (void)
{
  printf ("\n");
  printf ("\n");
  printf ("Did you forget to create your mycert.pem file?\n");
  printf ("\n");
  printf ("Run: openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout mycert.pem -out mycert.pem\n");
  printf ("\n");
  printf ("If you haven't, but that's my best guess of what has gone wrong..\n");
  printf ("\n");
  printf ("\n");
}

void addEpoll (int iPollFd, int iNewFd)
{
  struct epoll_event event;

  event.events = EPOLLIN | EPOLLET;
  event.data.fd = iNewFd;
  if (epoll_ctl (iPollFd, EPOLL_CTL_ADD, iNewFd, &event))
  {
    perror ("epoll_ctl");
    abort ();
  }
}

int main (int argc, char **argv)
{
  SSL_CTX *ctx;
  int server;
  int portnum;
  const char *szPemPublic = "mycert.pem";
  const char *szPemPrivate = "mycert.pem";
  const SSL_METHOD *method;
  int iPollFd;
  
  if (argc != 2)
  {
    printf ("Usage: %s <portnum>\n", argv[0]);
    exit (0);
  }

  portnum = atoi (argv[1]);

  if (portnum < 1024)
  {
    if (getuid () != 0)
    {
      printf ("This program must be run as root/sudo user since your port # (%d) is < 1024\n", portnum);
      exit (1);
    }
  }

  iPollFd = epoll_create1 (0);
  if (iPollFd == -1)
  {
    perror ("epoll_create1");
    abort ();
  }
  SSL_library_init ();               /* Initialize the SSL library */

  // InitServerCTX ();
  OpenSSL_add_all_algorithms ();     /* load & register all cryptos, etc. */
  SSL_load_error_strings ();         /* load all error messages */
  method = TLSv1_2_server_method (); /* create new server-method instance */
  ctx = SSL_CTX_new (method);        /* create new context from method */
  if (ctx == NULL)
  {
    ERR_print_errors_fp (stderr);
    abort ();
  }

  /* set the local certificate from CertFile */
  if (SSL_CTX_use_certificate_file (ctx, szPemPublic, SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_fp (stderr);
    instructionsForPem ();
    abort ();
  }

  /* set the private key from KeyFile (may be the same as CertFile) */
  if (SSL_CTX_use_PrivateKey_file (ctx, szPemPrivate, SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_fp (stderr);
    instructionsForPem ();
    abort ();
  }

  /* verify private key */
  if (!SSL_CTX_check_private_key (ctx))
  {
    fprintf (stderr, "Private key does not match the public certificate\n");
    abort ();
  }

  struct sockaddr_in addr;
 
  server = socket (PF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
  addEpoll (iPollFd, server);
  
  bzero (&addr, sizeof (addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (portnum);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind (server, (struct sockaddr*)&addr, sizeof (addr)) != 0)
  {
    perror ("can't bind port");
    abort ();
  }
  if (listen (server, 10) != 0)
  {
    perror ("Can't configure listening port");
    abort ();
  }

  for (;;)
  {
    struct sockaddr_in addr;
    socklen_t len = sizeof (addr);
    SSL *ssl;
    char buf[1024] = {0};   
    int sd, bytes;
    struct epoll_event event;

    // this is my attempt to run HTTPS.. This is sort of the minimal header that seems
    // to work.  \r is absolutely necessary.
    const char *szHttpServerResponse =
      "HTTP/1.1 200 OK\r\n"
      "Content-type: text/html\r\n"
      "\r\n"
      "<html>\n"
      "<body>\n"
      "<h1>So, this works, if you added a security exception to your web browser</h1>\n"
      "</body>\n"
      "</html>\n";
    int client;

    epoll_wait (iPollFd, &event, 1, -1);
    
    client = accept (server, (struct sockaddr*)&addr, &len);  /* accept connection as usual */
    addEpoll (iPollFd, client);    
    
    printf ("Connection: %s:%d:%d\n", inet_ntoa (addr.sin_addr), ntohs (addr.sin_port), client);
    ssl = SSL_new (ctx);           /* get new SSL state with context */
    SSL_set_fd (ssl, client);      /* set connection socket to SSL state */
    if (SSL_accept (ssl) == FAIL)  /* do SSL-protocol accept */
    {
      ERR_print_errors_fp (stderr);
    }
    else
    {
      X509 *cert;
      char *line;
      
      cert = SSL_get_peer_certificate (ssl); /* Get certificates (if available) */
      if (cert != NULL)
      {
        printf ("Server certificates:\n");
        line = X509_NAME_oneline (X509_get_subject_name (cert), 0, 0);
        printf ("Subject: %s\n", line);
        free (line);
        line = X509_NAME_oneline (X509_get_issuer_name (cert), 0, 0);
        printf ("Issuer: %s\n", line);
        free (line);
        X509_free (cert);
      }
      else
      {
        printf ("No certificates.\n");
      }

      //ShowCerts (ssl);                         /* get any certificates */
      bytes = SSL_read (ssl, buf, sizeof (buf)); /* get request */
      buf[bytes] = '\0';
 
      printf ("Client msg:\n[%s]\n", buf);
 
      if (bytes > 0)
      {
        printf ("Reply with:\n[%s]\n", szHttpServerResponse);
        SSL_write (ssl, szHttpServerResponse, strlen (szHttpServerResponse));
      }
      else
      {
        ERR_print_errors_fp (stderr);
      }            
    }
    sd = SSL_get_fd (ssl); /* get socket connection */
    SSL_free (ssl);        /* release SSL state */
    close (sd);            /* close connection */
    // break;
  }
  close (server);      /* close server socket */
  
  ERR_free_strings (); /* free memory from SSL_load_error_strings */
  EVP_cleanup ();      /* free memory from OpenSSL_add_all_algorithms */
  SSL_CTX_free (ctx);  /* release context */

  return 0;
}
