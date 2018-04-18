#ifndef __EPOLL_EXAMPLE_H__
#define __EPOLL_EXAMPLE_H__

#include <vector>
#include <set>

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
  bool add (int iFd, const epoll_data &epData);

  bool add (int iFd, void *vPtr)      {epoll_data epd; epd.ptr = vPtr;   return add (iFd, epd);}
  bool add (int iFd)                  {epoll_data epd; epd.fd  = iFd;    return add (iFd, epd);}
  bool add (int iFd, uint32_t u32Val) {epoll_data epd; epd.u32 = u32Val; return add (iFd, epd);}
  bool add (int iFd, uint64_t u64Val) {epoll_data epd; epd.u64 = u64Val; return add (iFd, epd);}

  epoll_event wait (int iTimeout = -1);
  bool remove (int iFd) {return removeOrClose (iFd, false);}
  bool close (int iFd)  {return removeOrClose (iFd, true);}

  ~epoller ();  
};


#endif //__EPOLL_EXAMPLE_H__
