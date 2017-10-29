// compile with one of the following:
// ----------------------------------
// gcc -g -Wall -rdynamic /tmp/jjjj.c -ldl
// gcc -g -Wall -rdynamic /tmp/jjjj.c -ldl -pthread
//
// This library over-rides malloc(3), realloc(3), calloc(3), and the free(3)
// functions in order to detect memory leaks, and memory over-runs.  All
// allocations and frees are reported to stdout.
//
// This should be light enough to leave in a final build, without the printf
//
// This is just a quick mockup, I'll add some polish later - probably add
// the ability to get a linked list of all allocated blocks in the system and
// also selectively alloc printf's to be turned on and off.  The goal here is
// just to eliminate any memory leaks in a system, and to quickly identify
// when allocated memory is not being freed.
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

#define __USE_GNU 1
#include <dlfcn.h>

static __thread int gi_hookDisabled=0;
static __thread int gi_allocCount=0;

static void * (*orgMalloc)  (size_t size)              = NULL;
static void   (*orgFree)    (void *ptr)                = NULL;
static void * (*orgCalloc)  (size_t nmeb, size_t size) = NULL;
static void * (*orgRealloc) (void *ptr, size_t size)   = NULL;

void static init (void) __attribute__((constructor)); // initialize this library

static void init (void)
{
  // only realloc and free are actually used, but I keep pointers to all
  orgMalloc  = dlsym (RTLD_NEXT, "malloc");
  orgFree    = dlsym (RTLD_NEXT, "free");
  orgCalloc  = dlsym (RTLD_NEXT, "calloc");
  orgRealloc = dlsym (RTLD_NEXT, "realloc");
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

#define REALLOC 0
#define MALLOC  1
#define CALLOC  2
static void *internalRealloc (void *vPtr, size_t size, size_t nmemb, unsigned char type)
{
  unsigned long long *ullPtr = (unsigned long long *)vPtr;
  unsigned long long longs;
  unsigned char *ucPtr;
  size_t adjSize;
  size_t s;
  char *szCaller = NULL;

  // align on a unsigned long long
  adjSize = size*nmemb;

  // NOTE: In this implementation, a size of 0 can be allocated
  //       This is POSIX compliant.  If the memory that is allocated
  //       is modified, it will be detected on free.

  if (adjSize % sizeof(unsigned long long))
  {
    adjSize += sizeof(unsigned long long) - (adjSize % sizeof(unsigned long long));
  }

  ullPtr = (unsigned long long *)orgRealloc (ullPtr==NULL ? NULL:ullPtr-3, adjSize + (5*sizeof(unsigned long long)));
  if (ullPtr == NULL)
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
      printf ("realloc (%p, %zu) = %p, %d\n", ullPtr+2, size, ullPtr, gi_allocCount);
      break;
    case MALLOC:
      printf ("malloc (%zu) = %p, %d\n", size, ullPtr+2, gi_allocCount);
      break;
    case CALLOC:
      printf ("calloc (%zu, %zu) = %p, %d\n", nmemb, size, ullPtr+2, gi_allocCount);
      break;
    }
    szCaller = trace (4, 1);
    gi_hookDisabled = 0;
  }

  longs = (unsigned long long) adjSize/sizeof(unsigned long long);
  ullPtr[0]       = size;
  ullPtr[1]       = (unsigned long long)szCaller;
  ullPtr[2]       = 0xDEADBEEFCACAFECEULL;
  ullPtr[3+longs] = 0xCACAFECEDEADBEEFULL;
  ullPtr[4+longs] = 0xCACAFECEDEADBEEFULL;

  ullPtr += 3;
  ucPtr = (unsigned char *) ullPtr;
  for (s = size ; ((unsigned long long) (&ucPtr[s])) % (sizeof (unsigned long long)); s++)
  {
    ucPtr[s] = (unsigned char) (((unsigned long long) (ucPtr+s)) & 0xFF);
  }

  return ullPtr;
}

static void internalFree (void *vPtr, int iLen)
{
  unsigned long long *ullPtr = (unsigned long long *)vPtr;
  unsigned long long longs;
  unsigned char *ucPtr;
  size_t adjSize;
  size_t s;
  size_t size;
  char *szCaller=NULL;
  char *szAllocator=NULL;

  ullPtr -= 3;

  size = (size_t)ullPtr[0];
  szAllocator = (char *)ullPtr[1];

  adjSize = size;
  if (adjSize % sizeof(unsigned long long))
  {
    adjSize += sizeof(unsigned long long) - (adjSize % sizeof(unsigned long long));
  }
  longs = (unsigned long long) adjSize/sizeof(unsigned long long);

  //  assert (ullPtr[1]       == 0xDEADBEEFCACAFECEULL);
  assert (ullPtr[2]       == 0xDEADBEEFCACAFECEULL);
  assert (ullPtr[3+longs] == 0xCACAFECEDEADBEEFULL);
  assert (ullPtr[4+longs] == 0xCACAFECEDEADBEEFULL);

  ucPtr = (unsigned char *) &ullPtr[3];
  for (s = size ; ((unsigned long long) (&ucPtr[s])) % (sizeof (unsigned long long)); s++)
  {
    assert (ucPtr[s] == (unsigned char) (((unsigned long long) (ucPtr+s)) & 0xFF));
  }

  orgFree (ullPtr);

  if (!gi_hookDisabled)
  {
    gi_hookDisabled = 1;
    szCaller = trace (iLen, 1);
    printf ("free (%p) (%s=>%s), %d\n", ullPtr, szAllocator, szCaller, gi_allocCount);
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
    static __thread int fakePtr[1024];
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
  internalFree (vPtr, 4);
}
