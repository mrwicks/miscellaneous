/*
 * .tlsServer (ip, port)
 * .tlsAccept (ip, port)
 * .tlsRecv (ip, port)
 * .tlsSend (ip, port)
 *
 * https://chris-wood.github.io/2016/05/06/OpenSSL-DTLS.html
 *
 */

#include "ipClass.h"

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

class tlsClass:public ipClass
{
private:
  static SSL_CTX *m_ctx;
  static int mi_Ref;
  SSL *m_ssl;

public:
  tlsClass ();
  ~tlsClass ();

  // TCP
  bool tlsServer (const struct sockaddr_in6 &in6, const char* szCertFile, const char* szKeyFile, int iBacklog=SOMAXCONN);
  bool tlsServer (const char* szCertFile, const char* szKeyFile, const std::string &strIp, unsigned short usPort, int iBacklog=SOMAXCONN, int iScope=0);
  bool tlsServer (const char* szCertFile, const char* szKeyFile, const std::string &strIp, unsigned short usPort, int iBacklog, const std::string &strScope);

  /*
  bool tcpClient ();
  bool tcpClient ();
  bool tcpClient ();
  */
  
  bool accept (ipClass *pClass, struct sockaddr_in6 *pcli_addr=NULL, int flags=0);
  void showCerts (void);
  
  // UDP - 2do
  void dltsServer ();

  // TCP or UDP
  int recv (void *vBuffer, int iLen);
  int send (const void *vBuffer, int iLen);
  int send (const std::string &str);
  bool close (void);
};

SSL_CTX * tlsClass::m_ctx = NULL;
int tlsClass::mi_Ref = 0;

bool tlsClass::tlsServer (const struct sockaddr_in6 &in6, const char* szCertFile, const char* szKeyFile, int iBacklog)
{
  bool bRet = true;
      
  // LoadCertificates (SSL_CTX* ctx, const char* CertFile, const char* szKeyFile)
  /* set the local certificate from szCertFile */
  if (SSL_CTX_use_certificate_file (m_ctx, szCertFile, SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_fp (stderr);
    bRet = false;
  }
  else
  {
    /* set the private key from szKeyFile (may be the same as szCertFile) */
    if (SSL_CTX_use_PrivateKey_file (m_ctx, szKeyFile, SSL_FILETYPE_PEM) <= 0)
    {
      ERR_print_errors_fp (stderr);
      bRet = false;
    }
    else
    {
      /* verify private key */
      if (!SSL_CTX_check_private_key (m_ctx))
      {
        fprintf (stderr, "Private key does not match the public certificate\n");
        bRet = false;
      }
      else
      {
        bRet = this->tcpServer (in6, iBacklog);
      }
    }
  }

  return bRet;
}

bool tlsClass::tlsServer (const char* szCertFile, const char* szKeyFile, const std::string &strIp, unsigned short usPort, int iBacklog, int iScope)
{
  struct sockaddr_in6 in6;
  bool bRet;

  bRet = this->setup_sockaddr (&in6, strIp, usPort, iScope);

  if (bRet == true)
  {
    bRet = this->tlsServer (in6, szCertFile, szKeyFile, iBacklog);
  }

  return bRet;
}

bool tlsClass::tlsServer (const char* szCertFile, const char* szKeyFile, const std::string &strIp, unsigned short usPort, int iBacklog, const std::string &strScope)
{
  int iScope = ipClass::name_to_scope (strScope);

  return this->tlsServer (szCertFile, szKeyFile, strIp, usPort, iBacklog, iScope);
}

bool tlsClass::accept (ipClass *pClass, struct sockaddr_in6 *pcli_addr, int flags)
{
  socklen_t addrlen=0;
  int iRet;
  bool bVal;

  if (pcli_addr != NULL)
  {
    addrlen = sizeof (*pcli_addr);
  }
  
  iRet = accept4 (mi_Fd, (struct sockaddr*)pcli_addr, &addrlen, flags);  /* accept connection as usual */

  if (iRet == -1)
  {
    ::perror ("accept");
    bVal = false;
  }
  else
  {
    m_ssl = SSL_new (m_ctx);           /* get new SSL state with context */
    SSL_set_fd (m_ssl, iRet);        /* set connection socket to SSL state */
    if (SSL_accept (m_ssl) <= 0)
    {
      ERR_print_errors_fp (stderr);
      bVal = false;
    }
    else
    {
      mi_Fd = iRet;
      bVal = true;
    }
  }

  return bVal;
}

void tlsClass::showCerts (void)
{
  X509 *cert;
  char *line;
 
  cert = SSL_get_peer_certificate (m_ssl); /* Get certificates (if available) */
  if ( cert != NULL )
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
}

tlsClass::tlsClass ()
{
  const SSL_METHOD *method;
  m_ssl = NULL;

  SSL_library_init ();
  if (m_ctx == NULL) // WONG - in constructor stoopid!
  {                                    /*************************************/
    OpenSSL_add_all_algorithms ();     /* load & register all cryptos, etc. */
    SSL_load_error_strings ();         /* load all error messages           */
    method = TLSv1_2_server_method (); /* create new server-method instance */
    m_ctx = SSL_CTX_new (method);      /* create new context from method    */
    if (m_ctx == NULL)                 /*************************************/
    {
      ERR_print_errors_fp (stderr);
      bRet = false;
    }
  }
  mi_Ref++;
}

tlsClass::~tlsClass ()
{
  mi_Ref--;

  SSL_shutdown (m_ssl);

  if (mi_Ref == 0)
  {
    SSL_CTX_free (m_ctx);
    EVP_cleanup ();
  }

  if (mi_Ref < 0)
  {
    fprintf (stderr, "mi_Ref (reference count) below 0??");
    abort ();
  }

  // base class destructor will be called
}
