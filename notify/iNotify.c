#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

void get_event (int iFd, const char *szDir)
{
  ssize_t len;
  char buffer[sizeof (struct inotify_event) + FILENAME_MAX] __attribute__ ( (aligned (__alignof__ (struct inotify_event))));
  struct inotify_event *pevent;
  char *ptr;

  // multiple events can be returned on a single read..
  len = read (iFd, buffer, sizeof (buffer));

  for (ptr = buffer ; ptr < &buffer[len] ; ptr += sizeof (struct inotify_event) + pevent->len)
  {
    pevent = (struct inotify_event *) ptr;
    if (pevent->len)
    {
      printf ("%s/%s\n", szDir, pevent->name);
    }
    else
    {
      // event on directory, not file within directory
      printf ("%s\n", szDir);
    }
  }
}

int main (int argc, char **argv)
{
  char *szDir;
  int iFd;
  int iWd; // watch descriptor

  if (argc != 2)
  {
    fprintf (stderr, "Supply a single directory to watch\n");
    return 1;
  }

  szDir = realpath (argv[1], NULL);

  iFd = inotify_init ();
  if (iFd < 0)
  {
    perror ("inotify_init");
    return 1;
  }

  iWd = inotify_add_watch (iFd, szDir, IN_CLOSE_WRITE);
  if (iWd < 0)
  {
    perror ("inotify_add_watch");
    return 1;
  }

  for (;;)
  {
    get_event (iFd, szDir);
    fflush (stdout);
  }
  free (szDir);
  
  return 0;
}
