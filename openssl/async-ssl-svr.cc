#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <poll.h>
#include <sys/epoll.h>
#include <signal.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>
#include <stdio.h>

// g++ -Wall -o async-ssl-svr async-ssl-svr.cc  -L/usr/lib -lssl -lcrypto

#define log(...) do { printf ("%s:%d: ", __FUNCTION__, __LINE__); printf(__VA_ARGS__); printf ("\n"); fflush(stdout); } while(0)
#define ASSERT(x, ...) if(!(x)) do { log( __VA_ARGS__); printf ("\n"); exit(1); } while(0)


int gb_stop = 0; // this is temporary

class ipClass
{
  class fdClass
  {
  private:
    enum
    {
      SSL_DISABLED     = 1<<0,
      TCP_CONNECTED    = 1<<1,
      SSL_STARTED      = 1<<2,
      SSL_CONNECTED    = 1<<3
    };
    int  mi_state;
    int  mi_Fd;
    SSL *m_ssl;

  public:
    fdClass (int iFd, int iEvents, int iEpollFd, bool bSslDisabled);
    ~fdClass ();
    bool handleHandshake (int iEpollFd, SSL_CTX* sslCtx);
    bool handleDataRead (void);
    void handleAccept (int iEpollFd, int iListenFd, bool bSslDisabled);
    bool handleRead (int iEpollFd, SSL_CTX* sslCtx, fdClass *fd);
    bool disableSsl (void);
  };

  int mi_EpollFd;
  SSL_CTX* m_sslCtx;
  fdClass *mi_li;

public:
  ipClass (unsigned short usPort, const std::string strPublicPem="", const std::string strPrivatePem="");
  ~ipClass ();
  void addFdToEpoll (int iEpollFd);
  int createServer (unsigned short usPort);
  void handleEvents (int iWaitMs);
  bool disableSsl (void);
};


ipClass::fdClass::fdClass (int iFd, int iEvents, int iEpollFd, bool bSslDisabled)
{
  struct epoll_event ev;
  int r;

  mi_Fd           = iFd;
  m_ssl           = NULL;

  if (bSslDisabled)
  {
    mi_state = SSL_DISABLED;
  }
  else
  {
    mi_state = 0;
  }

  ev.events = iEvents;
  ev.data.ptr = this;
  log ("adding fd %d events %d", mi_Fd, ev.events);
  r = epoll_ctl (iEpollFd, EPOLL_CTL_ADD, mi_Fd, &ev);
  ASSERT (r==0, "epoll_ctl add failed %d %s", errno, strerror (errno));
}

ipClass::fdClass::~fdClass ()
{
  log ("deleting fd %d", mi_Fd);
  close (mi_Fd);

  if (mi_state != SSL_DISABLED && m_ssl != NULL)
  {
    SSL_shutdown (m_ssl);
    SSL_free (m_ssl);
  }
}

bool ipClass::fdClass::handleHandshake (int iEpollFd, SSL_CTX* sslCtx)
{
  bool bRet = false;
  int iRet;
  int iError;

  if ((mi_state & TCP_CONNECTED) == 0)
  {
    log ("tcp connected fd %d", mi_Fd);
    mi_state |= TCP_CONNECTED;
    ASSERT (m_ssl == NULL, "SSL shouldn't be connected, but is");
  }

  if (mi_state != SSL_DISABLED)
  {
    //if (m_ssl == NULL)
    if ((mi_state & SSL_STARTED) == 0)
    {
      m_ssl = SSL_new (sslCtx);
      ASSERT (m_ssl != NULL, "SSL_new failed");
      iRet = SSL_set_fd (m_ssl, mi_Fd);
      ASSERT (iRet!=0, "SSL_set_fd failed");
      log ("SSL_set_accept_state for fd %d", mi_Fd);
      SSL_set_accept_state (m_ssl);
      mi_state |= SSL_STARTED;
    }
    iRet = SSL_do_handshake (m_ssl); // this will block - it should be safe to make this non blocking..
    if (iRet == 1)
    {
      mi_state = TCP_CONNECTED | SSL_STARTED | SSL_CONNECTED;
      log ("ssl connected fd %d", mi_Fd);
      return bRet;
    }

    iError = SSL_get_error (m_ssl, iRet);
    if (iError != SSL_ERROR_WANT_WRITE || iError != SSL_ERROR_WANT_READ)
    {
      log ("SSL_do_handshake return %d error %d errno %d msg %s", iRet, iError, errno, strerror (errno));
      ERR_print_errors_fp (stdout);
      bRet = true;
    }
  }

  return bRet;
}

bool ipClass::fdClass::handleDataRead (void)
{
  bool bRet = false;
  char szBuf[4096];
  int iBytesRead;
  static int iCount = 0;

  if (mi_state != SSL_DISABLED)
  {
    iBytesRead = SSL_read (m_ssl, szBuf, sizeof szBuf);
  }
  else
  {
    iBytesRead = (int)read (mi_Fd, szBuf, sizeof szBuf);
  }

  szBuf[iBytesRead] = '\0';
  log ("RBW: buf = [%d:%s]", iBytesRead, szBuf);

  iCount++;
  if (iBytesRead > 0)
  {
    int iBufSize;
    int iBytesWrite;

    //const char* cont = "HTTP/1.1 200 OK\r\nConnection: Close\r\n\r\n";
    const char *cont =
      "HTTP/1.1 200 OK\r\n"
      "Content-type: text/html\r\n"
      "\r\n"
      "<html>\n"
      "<body>\n"
      "<h1>So, this works, if you added a security exception to your web browser</h1>\n"
      "<h2>Or.... are using a genuine certificate.</h2>\n"
      "This has been reloaded %d times"
      "</body>\n"
      "</html>\n";

    iBufSize = snprintf (szBuf, sizeof szBuf, cont, iCount);

    if (mi_state != SSL_DISABLED)
    {
      iBytesWrite = SSL_write (m_ssl, szBuf, iBufSize);
      log ("SSL_write %d bytes", iBytesWrite);
    }
    else
    {
      iBytesWrite = send(mi_Fd, szBuf, iBufSize, 0);
    }
    bRet = true;
    return bRet;
  }

  if (mi_state != SSL_DISABLED)
  {
    int iSslError;

    iSslError = SSL_get_error (m_ssl, iBytesRead);
    if (iBytesRead < 0 && iSslError != SSL_ERROR_WANT_READ)
    {
      log ("SSL_read return %d error %d errno %d msg %s", iBytesRead, iSslError, errno, strerror (errno));
      bRet = true;
      return bRet;
    }
    else if (iBytesRead == 0)
    {
      if (iSslError == SSL_ERROR_ZERO_RETURN)
      {
        log ("SSL has been shutdown.");
      }
      else
      {
        log ("Connection has been aborted.");
      }
      bRet = true;
      return bRet;
    }
  }
  else
  {
    if (iBytesRead == 0)
    {
      bRet = true;
    }
    else if (iBytesRead <= 0)
    {
      // error
      bRet = true;
    }
  }

  return bRet;
}

void ipClass::fdClass::handleAccept (int iEpollFd, int iListenFd, bool bSslDisabled)
{
  struct sockaddr_in raddr;
  socklen_t rsz = sizeof (raddr);
  int cfd;

  cfd = accept4 (iListenFd, (struct sockaddr *)&raddr, &rsz, SOCK_CLOEXEC);

  if (cfd >= 0)
  {
    sockaddr_in peer;
    sockaddr_in local;
    socklen_t alen;
    int r;

    alen = sizeof (peer);
    r = getpeername (cfd, (sockaddr*)&peer, &alen);
    if (r < 0)
    {
      log ("get peer name failed %d %s", errno, strerror (errno));
      close (r);
    }
    else
    {
      r = getsockname (cfd, (sockaddr*)&local, &alen);
      if (r < 0)
      {
        log ("getsockname failed %d %s", errno, strerror (errno));
        close (r);
      }
      else
      {
        new fdClass (cfd, EPOLLERR | EPOLLHUP | EPOLLIN, iEpollFd, bSslDisabled);
      }
    }
  }
}

bool ipClass::fdClass::handleRead (int iEpollFd, SSL_CTX* sslCtx, fdClass *iFd)
{
  bool bRet = false;
  
  if (mi_state != SSL_DISABLED)
  {
    if (iFd == this)
    {
      handleAccept (iEpollFd, mi_Fd, false);
    }
    else if (mi_state == (TCP_CONNECTED | SSL_STARTED | SSL_CONNECTED))
    {
      bRet = handleDataRead ();
    }
    else
    {
      bRet = handleHandshake (iEpollFd, sslCtx);
    }
  }
  else
  {
    if (iFd == this)
    {
      handleAccept (iEpollFd, mi_Fd, true);
    }
    else
    {
      bRet = handleDataRead ();
    }
  }

  return bRet;
}

bool ipClass::fdClass::disableSsl (void)
{
  bool bRet;
  
  if (mi_state == 0 || mi_state == SSL_DISABLED)
  {
    mi_state = SSL_DISABLED;
    bRet = true;
  }
  else
  {
    // can't disable if we're already connected
    bRet = false;
  }

  return bRet;
}


ipClass::ipClass (unsigned short usPort, const std::string strPublicPem, const std::string strPrivatePem)
{
  bool bSslDisabled;
  int iListenFd;

  if (strPublicPem != "")
  {
    int r;
    bSslDisabled = false;

    SSL_load_error_strings ();
    r = SSL_library_init ();
    ASSERT (r==1, "SSL_library_init failed");

    m_sslCtx = SSL_CTX_new (SSLv23_method ());
    ASSERT (m_sslCtx != NULL, "SSL_CTX_new failed");

    r = SSL_CTX_use_certificate_file (m_sslCtx, strPublicPem.c_str (), SSL_FILETYPE_PEM);
    ASSERT (r==1, "SSL_CTX_use_certificate_file %s failed", strPublicPem.c_str ());

    r = SSL_CTX_use_PrivateKey_file (m_sslCtx, strPrivatePem.c_str (), SSL_FILETYPE_PEM);
    ASSERT (r==1, "SSL_CTX_use_PrivateKey_file %s failed", strPrivatePem.c_str ());

    r = SSL_CTX_check_private_key (m_sslCtx);
    ASSERT (r!=0, "SSL_CTX_check_private_key failed");

    log ("SSL inited");
  }
  else
  {
    bSslDisabled = true;
    m_sslCtx = NULL;
  }

  mi_EpollFd = epoll_create1 (EPOLL_CLOEXEC);

  iListenFd = createServer (usPort);
  mi_li = new fdClass (iListenFd, EPOLLERR | EPOLLHUP | EPOLLIN, mi_EpollFd, bSslDisabled);
}

ipClass::~ipClass ()
{
  delete mi_li;

  ::close (mi_EpollFd);
  if (m_sslCtx != NULL)
  {
    SSL_CTX_free (m_sslCtx);
  }
  ERR_free_strings ();
}

void ipClass::addFdToEpoll (int iEpollFd)
{
  struct epoll_event ev;
  int iRet;

  ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
  ev.data.ptr = this;
  iRet = epoll_ctl (iEpollFd, EPOLL_CTL_ADD, mi_EpollFd, &ev);
  log ("New master %d (%d)", iRet, mi_EpollFd);
}

int ipClass::createServer (unsigned short usPort)
{
  int iFd;
  struct sockaddr_in addr;
  int iRet;

  iFd = socket (AF_INET, SOCK_STREAM|SOCK_CLOEXEC, 0);

  memset (&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons (usPort);
  addr.sin_addr.s_addr = INADDR_ANY;

  iRet = ::bind (iFd, (struct sockaddr *)&addr, sizeof (struct sockaddr));
  ASSERT (iRet==0, "bind to 0.0.0.0:%d failed %d %s", usPort, errno, strerror (errno));

  iRet = ::listen (iFd, 20);
  ASSERT (iRet==0, "listen failed %d %s", errno, strerror (errno));
  log ("fd %d listening at %d", iFd, usPort);
  return iFd;
}

void ipClass::handleEvents (int iWaitMs)
{
  const int kMaxEvents = 20;
  struct epoll_event activeEvs[kMaxEvents];
  fdClass *fdToDelete[kMaxEvents];
  int iEventCount;
  int iEventsToDelete=0;

  iEventCount = epoll_wait (mi_EpollFd, activeEvs, kMaxEvents, iWaitMs);
  log ("epoll gives %d events", iEventCount);

  for (int i = 0 ; i < iEventCount ; i++)
  {
    fdClass *fdh;
    int events;

    fdh = (fdClass*)activeEvs[i].data.ptr;
    events = activeEvs[i].events;

    // writes only happen as a result of a read, and I don't bother
    // to do blocking.
    if (events & (EPOLLIN | EPOLLERR | EPOLLHUP))
    {
      if (fdh->handleRead (mi_EpollFd, m_sslCtx, mi_li) == true)
      {
        // this particular listener needs to be deleted // RBW
        fdToDelete[iEventsToDelete++] = fdh;
      }
    }
  }

  // make certain listener is only deleted once.  There appears to be a
  // (rare) race condition when the same event can be triggered twice.
  for (int i = 0 ; i < iEventsToDelete ; i++)
  {
    for (int j = i+1 ; j < iEventsToDelete ; j++)
    {
      // don't delete twice
      if (fdToDelete[i] == fdToDelete[j])
      {
        fdToDelete[j] = NULL;
      }
    }

    if (fdToDelete[i] != NULL)
    {
      delete fdToDelete[i];
    }
  }
}

bool ipClass::disableSsl (void)
{
  bool bRet;

  if (mi_li->disableSsl () == true)
  {
    SSL_CTX_free (m_sslCtx);
    m_sslCtx = NULL;
    bRet = true;
  }
  else
  {
    // connection has already been made.
    bRet = false;
  }

  return bRet;
}

void handleInterrupt (int sig)
{
  gb_stop = true;
}

int main (int argc, char **argv)
{
  int iMainEpoll;
  unsigned short usPort = 8000; // typically this is 443 (HTTPS) but I have a real server running there.

  if (argc == 2)
  {
    usPort = atoi(argv[1]);
  }
  std::cout << "Using port: " << usPort << "\n";

  signal (SIGINT, handleInterrupt);

  iMainEpoll = epoll_create1 (EPOLL_CLOEXEC);

  ipClass s0(usPort+0, "server.pem", "server.pem");
  ipClass s1(usPort+1, "server.pem", "server.pem");
  ipClass s2(usPort+2, "server.pem", "server.pem");
  ipClass s3(usPort+3, "server.pem", "server.pem");
  ipClass s4(usPort+4, "server.pem", "server.pem");
  ipClass s5(usPort+5, "server.pem", "server.pem");
  ipClass s6(usPort+6, "server.pem", "server.pem");
  ipClass s7(usPort+7, "server.pem", "server.pem");
  ipClass s8(usPort+8, "server.pem", "server.pem");
  ipClass s9(usPort+9, "server.pem", "server.pem");

  s0.disableSsl ();
  s2.disableSsl ();
  s4.disableSsl ();
  s6.disableSsl ();
  s8.disableSsl ();

  s0.addFdToEpoll (iMainEpoll);
  s1.addFdToEpoll (iMainEpoll);
  s2.addFdToEpoll (iMainEpoll);
  s3.addFdToEpoll (iMainEpoll);
  s4.addFdToEpoll (iMainEpoll);
  s5.addFdToEpoll (iMainEpoll);
  s6.addFdToEpoll (iMainEpoll);
  s7.addFdToEpoll (iMainEpoll);
  s8.addFdToEpoll (iMainEpoll);
  s9.addFdToEpoll (iMainEpoll);

  while (!gb_stop)
  {
    const int iMaxEvents = 20;
    struct epoll_event ev[iMaxEvents];
    int iEventCount;
    
    iEventCount = epoll_wait (iMainEpoll, ev, iMaxEvents, -1);
    for (int i = 0 ; i < iEventCount ; i++)
    {
      ipClass *x = (ipClass *)ev[i].data.ptr;
      x->handleEvents (0);
    }
  }
  return 0;
}
