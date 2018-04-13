#include <vector>
#include <iostream>
#include <set>

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
  std::set<int> mset_fileDescriptors;
  int mi_PollFd;
  int mi_Ready;

  bool removeOrClose (int iFd, bool bClose);

public:
  epoller ();
  static void makeFileDescriptorNonBlocking (int iFd);
  static void makeFileDescriptorBlocking (int iFd);
  bool add (int iFd, void *vPtr=NULL);
  epoll_event wait (int iTimeout = -1);
  bool remove (int iFd);
  bool close (int iFd);
  ~epoller ();  
};

bool epoller::removeOrClose (int iFd, bool bClose)
{
  bool bFound = false;
  std::set<int>::iterator iter;

  // remove from the list of monitored descriptors
  iter = mset_fileDescriptors.find (iFd);
  if (iter != mset_fileDescriptors.end())
  {
    // remove it from monitoring, explicitly
    epoll_ctl (mi_PollFd, EPOLL_CTL_DEL, iFd, &mv_event[0]);

    if (bClose == true)
    {
      if (fileno (stdin) != iFd)
      {
        // close - but not if it's stdin
        if (::close (iFd) == -1)
        {
          perror ("close");
          exit (1);
        }
        else
        {
          makeFileDescriptorBlocking (iFd);
        }
      }
      else
      {
        makeFileDescriptorBlocking (iFd);
      }
    }
    bFound = true;
    
    // remove this from the list of monitored file descriptors
    mset_fileDescriptors.erase (iter);
    
    // shrink the list of events by one
    mv_event.erase (mv_event.end());
  }
  return bFound;
}

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

void epoller::makeFileDescriptorBlocking (int iFd)
{
  int iFlags;
  int iNewFd;

  iFlags = fcntl (iFd, F_GETFL, 0);
  if (iFlags == -1)
  {
    perror ("fcntl");
    exit (1);
  }

  iFlags &= ~O_NONBLOCK;
  iNewFd = fcntl (iFd, F_SETFL, iFlags);
  if (iNewFd == -1)
  {
    perror ("fcntl");
    exit (1);
  }
}

bool epoller::add (int iFd, void *vPtr)
{
  struct epoll_event event;
  int iRet;
  bool bNew = true;

  if (mset_fileDescriptors.find(iFd) != mset_fileDescriptors.end())
  {
    bNew = false;
    std::cout << "Duplicate\n";
  }
  else
  {
    makeFileDescriptorNonBlocking (iFd);

    if (vPtr == NULL)
    {
      event.data.fd = iFd;
    }
    else
    {
      event.data.ptr = vPtr;
    }
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
    mset_fileDescriptors.insert (iFd);
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

bool epoller::remove (int iFd)
{
  return removeOrClose (iFd, false);
}

bool epoller::close (int iFd)
{
  return removeOrClose (iFd, true);
}

epoller::~epoller ()
{
  int iStdin = fileno (stdin);

  for (std::set<int>::iterator iter = mset_fileDescriptors.begin() ; iter != mset_fileDescriptors.end() ; iter++)
  {
    if (iStdin != *iter) // don't want to close stdin, if we've used it
    {
      if (::close (*iter) == -1)
      {
        perror ("close");
        exit (1);
      }
      else
      {
        makeFileDescriptorBlocking (*iter);
      }
    }
  }
    
  // close the epoll fd
  if (::close (mi_PollFd) == -1)
  {
    perror ("close");
    exit (1);
  }
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
