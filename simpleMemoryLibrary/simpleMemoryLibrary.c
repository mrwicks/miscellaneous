// compile with one of the following:
// ----------------------------------
// gcc -g -Wall -rdynamic ./simpleMemoryLibrary.c -ldl
// gcc -g -Wall -rdynamic ./simpleMemoryLibrary.c -ldl -pthread
//
// This library over-rides malloc(3), realloc(3), calloc(3), and the free(3)
// functions in order to detect memory leaks, and memory over-runs.  All
// allocations and frees are reported to stdout.
//
// This should be light enough to leave in a final build.
//
// Additional functions available:
//   showAllocations (void) - shows what's currently allocated
//
// Note that printf(3) makes use of malloc(3) which requires the need NOT
// to track memory used internally by glibc.

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

__thread LIST_HEAD (listHead, memoryHeader) gp_listHead;
struct memoryHeader
{
  char *szAllocator;
  size_t size;
  LIST_ENTRY (memoryHeader) doubleLinkedList;
  unsigned long long ullFixedValues[2];
};

struct memoryCap
{
  unsigned long long ullFixedValues[2];
};

static __thread int gi_hookDisabled=0;
static __thread int gi_allocCount=0;
static void * (*orgMalloc)  (size_t size)              = NULL;
static void   (*orgFree)    (void *ptr)                = NULL;
static void * (*orgCalloc)  (size_t nmeb, size_t size) = NULL;
static void * (*orgRealloc) (void *ptr, size_t size)   = NULL;

void static init (void) __attribute__((constructor)); // initialize this library
void static end  (void) __attribute__((destructor));  // check for any outstanding allocs

static void init (void)
{
  // only realloc and free are actually used, but I keep pointers to all
  orgMalloc  = dlsym (RTLD_NEXT, "malloc");
  orgFree    = dlsym (RTLD_NEXT, "free");
  orgCalloc  = dlsym (RTLD_NEXT, "calloc");
  orgRealloc = dlsym (RTLD_NEXT, "realloc");

  LIST_INIT (&gp_listHead);
}

static void end (void)
{
  if (gi_allocCount != 0)
  {
    struct memoryHeader *ml;

    printf ("%d allocations detected on exit\n", gi_allocCount);
    for (ml = gp_listHead.lh_first ; ml != NULL ; ml = ml->doubleLinkedList.le_next)
    {
      if (ml->szAllocator != NULL)
      {
	printf ("  Not freed: %p size of %zu, allocated by \"%s\"\n",
		ml->ullFixedValues+2, ml->size, ml->szAllocator);
      }
    }
  }
}

void showAllocations (void)
{
  struct memoryHeader *ml;
  int iLen;
  
  iLen = printf ("%d blocks allocated\n", gi_allocCount);
  for (ml = gp_listHead.lh_first ; ml != NULL ; ml = ml->doubleLinkedList.le_next)
  {
    if (ml == gp_listHead.lh_first)
    {
      // create an underline.
      while (iLen-- > 0)
      {
	printf ("-");
      }
      printf ("\n");
    }

    if (ml->szAllocator != NULL)
    {
      printf ("  Address %p size of %zu, allocated by \"%s\"\n",
	      ml->ullFixedValues+2, ml->size, ml->szAllocator);
    }
  }

  if (gi_allocCount != 0)
  {
    printf ("\n");
  }
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
  for (s = size ; ((unsigned long long) (ucPtr + s)) % (sizeof (unsigned long long)); s++)
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
    LIST_REMOVE (mHead, doubleLinkedList);
  }
  mHead = (struct memoryHeader *)orgRealloc (mHead, adjSize);
  if (mHead == NULL)
  {
    return NULL;
  }

  if (!gi_hookDisabled)
  {
    gi_hookDisabled = 1;

    if (vPtr == NULL)
    {
      gi_allocCount++;
    }

    switch (type)
    {
    case REALLOC:
      printf ("realloc (%p, %zu) = %p, %d\n", vPtr, size, &mHead->ullFixedValues[2], gi_allocCount);
      break;
    case MALLOC:
      printf ("malloc (%zu) = %p, %d\n", size, &mHead->ullFixedValues[2], gi_allocCount);
      break;
    case CALLOC:
      printf ("calloc (%zu, %zu) = %p, %d\n", nmemb, size, &mHead->ullFixedValues[2], gi_allocCount);
      break;
    }
    szCaller = trace (4, 1);
    gi_hookDisabled = 0;
  }

  // save the allocator and size, fill up any unused bytes at the end of
  // the allocation with essentially address == data, and place guard bands
  // on the memory at the bottom and top of memory
  mHead->szAllocator = szCaller;
  mHead->size = size;
  LIST_INSERT_HEAD(&gp_listHead, mHead, doubleLinkedList);
  mHead->ullFixedValues[0] = 0xDEADBEEFCACAFECEULL;
  mHead->ullFixedValues[1] = 0xDEADBEEFCACAFECEULL;

  // point to usable memory
  ucPtr = ((unsigned char *)mHead) + (sizeof (struct memoryHeader));
  for (s = size ; ((unsigned long long) (ucPtr + s)) % (sizeof (unsigned long long)); s++)
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

  // verify not over-runs in data
  mHead = verifyIntegrity (vPtr);

  szAllocator = mHead->szAllocator;

  LIST_REMOVE (mHead, doubleLinkedList);
  orgFree (mHead);

  if (!gi_hookDisabled)
  {
    gi_hookDisabled = 1;
    szCaller = trace (iLen, 1);
    printf ("free (%p) (allocated by \"%s\" freed by \"%s\"), %d\n", vPtr, szAllocator, szCaller, gi_allocCount);
    gi_allocCount--;

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

int getAllocCount (void)
{
  return gi_allocCount;
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

  if (orgCalloc == NULL)
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

