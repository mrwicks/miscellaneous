#include <vector>
#include <iostream>

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <stdio.h>


class epoller
{
private:
  std::vector<struct epoll_event> mv_event;
  std::vector<int> mvi_fileDescriptors;
  int mi_PollFd;
  int mi_Ready;

public:
  epoller ();
  static void makeFileDescriptorNonBlocking (int iFd);
  bool add (int iFd);
  epoll_event wait (int iTimeout = -1);
  bool close (int iFd);
  ~epoller ();  
};

epoller::epoller ()
{
  mi_PollFd = epoll_create1 (0);
  if (mi_PollFd == -1)
  {
    perror ("epoll_create1");
    exit (1);
  }
  mi_Ready = 0;
}

void epoller::makeFileDescriptorNonBlocking (int iFd)
{
  int iFlags;
  int iNewFd;

  iFlags = fcntl (iFd, F_GETFL, 0);
  if (iFlags == -1)
  {
    perror ("fcntl");
    exit (1);
  }

  iFlags |= O_NONBLOCK;
  iNewFd = fcntl (iFd, F_SETFL, iFlags);
  if (iNewFd == -1)
  {
    perror ("fcntl");
    exit (1);
  }
}

bool epoller::add (int iFd)
{
  struct epoll_event event;
  int iRet;
  bool bNew = true;

  for(std::vector<int>::iterator iter = mvi_fileDescriptors.begin(); iter != mvi_fileDescriptors.end(); iter++)
  {
    if (*iter == iFd)
    {
      bNew = false;
      std::cout << "Duplicate\n";
      break;
    }
  }

  if (bNew == true)
  {
    makeFileDescriptorNonBlocking (iFd);
    
    event.data.fd = iFd;
    event.events = EPOLLIN | EPOLLET;
    iRet = epoll_ctl (mi_PollFd, EPOLL_CTL_ADD, iFd, &event);
    
    if (iRet == -1)
    {
      perror ("epoll_ctl");
      exit (1);
    }
    
    // make room for returned events, data within event is irrelevant
    mv_event.push_back (event);

    // keep a record of what file descriptors we're monitoring
    mvi_fileDescriptors.push_back (iFd);
  }

  return bNew;
}

epoll_event epoller::wait (int iTimeout)
{
  int iRet;
  epoll_event epEvent;

  epEvent.events = 0;
  epEvent.data.ptr = NULL;

  if (mi_Ready == 0)
  {
    // if no file descriptor to process
    iRet = epoll_wait (mi_PollFd, &(mv_event[0]), mv_event.size(), iTimeout);

    if (iRet == -1 && errno == EINTR)
    {
      std::cout << "Signal trapped\n";
    }
    else if (iRet > 0)
    {
      mi_Ready = iRet;
    }
  }

  if (mi_Ready > 0)
  {
    // with already have file descriptors to process
    mi_Ready--;
    epEvent = mv_event[mi_Ready];
  }

  return epEvent;
}

bool epoller::close (int iFd)
{
  bool bFound = false;
  
  // remove from the list of monitored descriptors
  for(std::vector<int>::iterator iter = mvi_fileDescriptors.begin(); iter != mvi_fileDescriptors.end(); iter++)
  {
    if (*iter == iFd)
    {
      if (fileno (stdin) != iFd)
      {
        // close - but not if it's stdin
        // NOTE: it's still be polled, there's no way to remove it from the epoll_wait that I know of
        ::close (iFd);
      }
      bFound = true;

      // remove this from the list of monitored file descriptors
      mvi_fileDescriptors.erase (iter);

      // shrink the list of events by one
      mv_event.erase (mv_event.end());
      break;
    }
  }
  return bFound;
}

epoller::~epoller ()
{
  int iStdin = fileno (stdin);

  // close all file descriptors
  for(std::vector<int>::iterator iter = mvi_fileDescriptors.begin(); iter != mvi_fileDescriptors.end(); iter++)
  {
    if (iStdin != *iter) // don't want to close stdin, if we've used it
    {
      ::close (*iter);
    }
  }
  
  // close the epoll fd
  ::close (mi_PollFd);
}

int main (int argc, char **argv)
{
  epoller ep;

  ep.add (fileno(stdin));
  for (int i = 0 ; i < 3 ; i++)
  {
    struct epoll_event event;
    int iRet;
    unsigned char var;
    
    event = ep.wait ();
    for ( ;; )
    {
      iRet = read (event.data.fd, &var, sizeof (var));
      if (iRet == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      {
        break;
      }
      std::cout << iRet << ":" << var << " ";
    }
  }

  return 0;
}
