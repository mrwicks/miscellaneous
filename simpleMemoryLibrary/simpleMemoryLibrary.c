// compile with one of the following:
// ----------------------------------
// gcc -g -Wall -rdynamic ./simpleMemoryLibrary.c -ldl
// gcc -g -Wall -rdynamic ./simpleMemoryLibrary.c -ldl -pthread
//
// NOTE: It's not safe to allocate from one thread and free from another
//       with this library, currently.  I'll eventually implement a mutex
//       to take care of this.
//
// This library over-rides malloc(3), realloc(3), calloc(3), and the free(3)
// functions in order to detect memory leaks, and memory over-runs.  All
// allocations and frees are reported to stdout.
//
// This should be light enough to leave in a final build.
//
// Additional functions available:
//   void showAllocations (FILE *fp) - shows what's currently allocated
//   int getAllocCount (void) - get the # of allocations
//   size_t getUsage (void) - total amount of memory allocated
//
// Note that printf(3) makes use of malloc(3) which requires the need to 
// NOT track memory used internally by glibc while in these functions.

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/queue.h>

#define __USE_GNU 1
#include <dlfcn.h>
#include <pthread.h>

LIST_HEAD (listHead, memoryHeader) gp_listHead;
struct memoryHeader
{
  char *szAllocator;
  size_t size;
  pthread_t threadId;
  LIST_ENTRY (memoryHeader) doubleLinkedList;
  unsigned long long ullFixedValues[2];
};

struct memoryCap
{
  unsigned long long ullFixedValues[2];
};

pthread_mutex_t g_mutex;

static int gi_allocCount=0;
static __thread int gi_hookDisabled=0;
static void * (*gp_orgMalloc)  (size_t size)              = NULL;
static void   (*gp_orgFree)    (void *ptr)                = NULL;
static void * (*gp_orgCalloc)  (size_t nmeb, size_t size) = NULL;
static void * (*gp_orgRealloc) (void *ptr, size_t size)   = NULL;

void static init (void) __attribute__((constructor)); // initialize this library
void static end  (void) __attribute__((destructor));  // check for any outstanding allocs

#define MUTEX_INIT(mp)                                    \
do                                                        \
{                                                         \
  if (pthread_mutex_init(mp,NULL) != 0)                   \
  {                                                       \
    perror("pthread_mutex_init");                         \
    exit(EXIT_FAILURE);                                   \
  }                                                       \
} while (0)

#define MUTEX_LOCK(mp)                                    \
do                                                        \
{                                                         \
  if (pthread_mutex_lock(mp) != 0)                        \
  {                                                       \
    perror("pthread_mutex_lock");                         \
    exit(EXIT_FAILURE);                                   \
  }                                                       \
} while (0)

#define MUTEX_UNLOCK(mp)                                  \
do                                                        \
{                                                         \
  if (pthread_mutex_unlock(mp) != 0)                      \
  {                                                       \
    perror("pthread_mutex_unlock");                       \
    exit(EXIT_FAILURE);                                   \
  }                                                       \
} while (0)

#define MUTEX_DESTROY(mp)                                 \
do                                                        \
{                                                         \
  if (pthread_mutex_destroy(mp) != 0)                     \
  {                                                       \
    perror("pthread_mutex_destroy");                      \
    exit(EXIT_FAILURE);                                   \
  }                                                       \
} while (0)

static void init (void)
{
  // only realloc and free are actually used, but I keep pointers to all
  gp_orgMalloc  = dlsym (RTLD_NEXT, "malloc");
  gp_orgFree    = dlsym (RTLD_NEXT, "free");
  gp_orgCalloc  = dlsym (RTLD_NEXT, "calloc");
  gp_orgRealloc = dlsym (RTLD_NEXT, "realloc");
  MUTEX_INIT (&g_mutex);

  LIST_INIT (&gp_listHead);
}

int getAllocCount (void)
{
  return gi_allocCount;
}

size_t getUsage (void)
{
  struct memoryHeader *ml;
  size_t size=0;
 
  MUTEX_LOCK (&g_mutex);
  for (ml = gp_listHead.lh_first ;
       ml != NULL ;
       ml = ml->doubleLinkedList.le_next)
  {
    if (ml->szAllocator != NULL)
    {
      size += ml->size;
    }
  }
  MUTEX_UNLOCK (&g_mutex);

  return size;
}

void showAllocations (FILE *fp)
{
  struct memoryHeader *ml;
  int iCount=0;
  
  MUTEX_LOCK (&g_mutex);
  for (ml = gp_listHead.lh_first ;
       ml != NULL ;
       ml = ml->doubleLinkedList.le_next)
  {
    if (ml->szAllocator != NULL)
    {
      if (iCount++==0)
      {
	// create a header
	fprintf (fp, "\n");
	int iLen = fprintf (fp, "%d block%s remain%s allocated\n",
			    gi_allocCount, gi_allocCount > 1 ? "s":"" ,
			    gi_allocCount > 1 ? "":"s");
	while (--iLen > 0)
	{
	  fprintf (fp, "-");
	}
	fprintf (fp, "\n");
      }
      fprintf (fp, "  Address %p size of %zu, allocated by \"%s\"\n",
	       ml->ullFixedValues+2, ml->size, ml->szAllocator);
    }
  }

  if (iCount != 0)
  {
    fprintf (fp, "\n");
  }
  MUTEX_UNLOCK (&g_mutex);
}

static void end (void)
{
  showAllocations (stderr);
  MUTEX_DESTROY (&g_mutex);
}

static char *trace (int iLen, unsigned ucGetPtr)
{
  int nptrs;
  char **strings;
  void *buffer[iLen];
  char *szPtr=NULL;
 
  nptrs = backtrace(buffer, iLen);
  
  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL)
  {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  if (ucGetPtr == 0)
  {
    int j;
    for (j = 0; j < nptrs; j++)
    {
      printf("TRACE> %s\n", strings[j]);
    }
  }
  else if (nptrs == iLen)
  {
    szPtr = strdup (strings[nptrs-1]);
  }

  free(strings);

  return szPtr;
}

static struct memoryHeader *verifyIntegrity (void *vPtr)
{
  struct memoryHeader *mHead;
  struct memoryCap    *mCap;
  unsigned char *ucPtr;
  size_t s;
  size_t size;

  // adjust pointer to the actual start of allocation
  mHead = (struct memoryHeader *)(vPtr - sizeof(struct memoryHeader));

  size = mHead->size;

  assert (mHead->ullFixedValues[0] == 0xDEADBEEFCACAFECEULL);
  assert (mHead->ullFixedValues[1] == 0xDEADBEEFCACAFECEULL);

  ucPtr = ((unsigned char *) vPtr);
  for (s = size ;
       ((unsigned long long) (ucPtr + s)) % (sizeof (unsigned long long));
       s++)
  {
    assert (ucPtr[s] == (unsigned char) (((unsigned long long) (ucPtr+s)) & 0xFF));
  }
  mCap = (struct memoryCap *)(ucPtr + s);

  assert (mCap->ullFixedValues[0]   == 0xCACAFECEDEADBEEFULL);
  assert (mCap->ullFixedValues[1]   == 0xCACAFECEDEADBEEFULL);

  return mHead;
}

#define REALLOC 0
#define MALLOC  1
#define CALLOC  2
static void *internalRealloc (void *vPtr, size_t size, size_t nmemb, unsigned char type)
{
  struct memoryHeader *mHead = NULL;
  struct memoryCap    *mCap = NULL;
  unsigned char *ucPtr;
  size_t adjSize;
  size_t s;
  char *szCaller = NULL;

  // NOTE: In this implementation, a size of 0 can be allocated
  //       This is POSIX compliant.  If the memory that is allocated
  //       is modified, it will be detected on free.

  // align on a unsigned long long
  adjSize = size*nmemb;
  if (adjSize % sizeof(unsigned long long))
  {
    adjSize += sizeof(unsigned long long) - (adjSize % sizeof(unsigned long long));
  }
  adjSize += sizeof (struct memoryHeader) + sizeof (struct memoryCap);

  // adjust pointer to the actual start of allocation
  if (vPtr != NULL)
  {
    // we have to remove this from the linked list because we are reallocating
    // the memory - which may move it.  Verify the integrity of the memory as
    // well.
    mHead = verifyIntegrity (vPtr);
    MUTEX_LOCK (&g_mutex);
    LIST_REMOVE (mHead, doubleLinkedList);
    MUTEX_UNLOCK (&g_mutex);
    if (mHead->szAllocator)
    {
      // delete string to who allocated it
      int iHookState = gi_hookDisabled;
      gi_hookDisabled = 1;
      free (mHead->szAllocator);
      gi_hookDisabled = iHookState;
    }
  }
  mHead = (struct memoryHeader *)gp_orgRealloc (mHead, adjSize);
  if (mHead == NULL)
  {
    return NULL;
  }

  if (!gi_hookDisabled)
  {
    gi_hookDisabled = 1;

    szCaller = trace (4, 1);
    switch (type)
    {
    case REALLOC:
      MUTEX_LOCK (&g_mutex);
      gi_allocCount += (vPtr==NULL) ? 1 : 0;
      MUTEX_UNLOCK (&g_mutex);
      printf ("realloc (%p, %zu) = %p, allocated by %s, %d\n",
	      vPtr, size, &mHead->ullFixedValues[2], szCaller, gi_allocCount);
      break;
    case MALLOC:
      MUTEX_LOCK (&g_mutex);
      gi_allocCount++;
      MUTEX_UNLOCK (&g_mutex);
      printf ("malloc (%zu) = %p, allocated by %s, %d\n",
	      size, &mHead->ullFixedValues[2], szCaller, gi_allocCount);
      break;
    case CALLOC:
      MUTEX_LOCK (&g_mutex);
      gi_allocCount++;
      MUTEX_UNLOCK (&g_mutex);
      printf ("calloc (%zu, %zu) = %p, allocated by %s, %d\n",
	      nmemb, size, &mHead->ullFixedValues[2], szCaller, gi_allocCount);
      break;
    }
    gi_hookDisabled = 0;
  }

  // save the allocator and size, fill up any unused bytes at the end of
  // the allocation with essentially address == data, and place guard bands
  // on the memory at the bottom and top of memory
  mHead->szAllocator = szCaller;
  mHead->size = size;
  mHead->threadId = pthread_self ();
  MUTEX_LOCK (&g_mutex);
  LIST_INSERT_HEAD(&gp_listHead, mHead, doubleLinkedList);
  MUTEX_UNLOCK (&g_mutex);
  mHead->ullFixedValues[0] = 0xDEADBEEFCACAFECEULL;
  mHead->ullFixedValues[1] = 0xDEADBEEFCACAFECEULL;

  // point to usable memory
  ucPtr = ((unsigned char *)mHead) + (sizeof (struct memoryHeader));
  for (s = size ;
       ((unsigned long long) (ucPtr + s)) % (sizeof (unsigned long long));
       s++)
  {
    ucPtr[s] = (unsigned char) (((unsigned long long) (ucPtr+s)) & 0xFF);
  }

  // fill up the cap
  mCap = (struct memoryCap *)(ucPtr + s);
  mCap->ullFixedValues[0] = 0xCACAFECEDEADBEEFULL;
  mCap->ullFixedValues[1] = 0xCACAFECEDEADBEEFULL;

  return ucPtr;
}

static void internalFree (void *vPtr, int iLen)
{
  struct memoryHeader *mHead;
  char *szCaller=NULL;
  char *szAllocator=NULL;

  // verify no over-runs in data
  mHead = verifyIntegrity (vPtr);

  szAllocator = mHead->szAllocator;

  MUTEX_LOCK (&g_mutex);
  LIST_REMOVE (mHead, doubleLinkedList);
  MUTEX_UNLOCK (&g_mutex);
  gp_orgFree (mHead);

  if (!gi_hookDisabled)
  {
    gi_hookDisabled = 1;
    szCaller = trace (iLen, 1);
    printf ("free (%p) (allocated by \"%s\" freed by \"%s\"), %d\n",
	    vPtr, szAllocator, szCaller, gi_allocCount);
    MUTEX_LOCK (&g_mutex);
    gi_allocCount--;
    MUTEX_UNLOCK (&g_mutex);

    if (szCaller != NULL)
    {
      free (szCaller);
    }
    if (szAllocator != NULL)
    {
      free (szAllocator);
    }
    gi_hookDisabled = 0;  
  }
}

void *malloc (size_t size)
{
  return internalRealloc (NULL, size, 1, MALLOC);
}

void *realloc(void *vPtr, size_t size)
{
  if (size == 0 && vPtr != NULL)
  {
    // you can free memory with realloc, if you pass 0 size, with a
    // non NULL pointer.
    internalFree (vPtr, 4);
    return NULL;
  }
  else
  {
    return internalRealloc (vPtr, size, 1, REALLOC);
  }
}

void *calloc(size_t nmemb, size_t size)
{
  void *vPtr;

  if (gp_orgCalloc == NULL)
  {
    // This is a hack when compiling with -pthread.  Calloc(3) is used
    // by dlsym(3) when -pthread is used.  Since we need dlsym(3) to
    // get the pointer to the allocation functions, I just return
    // a 4KB space statically allocated.  Calloc(3) does not free this
    // memory anyhow.  The amount of memory used is actually small
    // but I use 4KB for any future changes in glibc
    static __thread unsigned long long fakePtr[512];
    vPtr = fakePtr;
    assert (size <= sizeof(fakePtr));
  }
  else
  {
    vPtr = internalRealloc (NULL, nmemb, size, CALLOC);
  }
  
  bzero (vPtr, sizeof(size));
  return vPtr;
}

void free(void *vPtr)
{
  // a pointer value of NULL is legal under POSIX, oddly
  if (vPtr != NULL)
  {
    internalFree (vPtr, 4);
  }
}
