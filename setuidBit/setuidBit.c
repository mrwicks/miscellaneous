#include <stdio.h> // needed for printf

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

void change_to_user (const char *szUserName)
{
  struct passwd *pw;

  pw = getpwnam(szUserName);
  if (pw != NULL)
  {
    uid_t uid = pw->pw_uid;

    // note, executable needs to be owned by use your are setuid'ing to
    // and you need to execute chmod a+s {executable} for this.
    printf ("UID of user %s is %d\n", szUserName, (int)uid);
    if (setuid (uid) != 0)
    {
      perror ("setuid");
    }
    else
    {
      printf ("UID is now %d\n", (int)uid);
    }
  }
  else
  {
    perror ("getpwnam");
  }
}


int main (int argc, char **argv)
{
  int iIter;
  
  if (argc == 1)
  {
    printf ("Give me a user name\n");
    return 1;
  }

  for (iIter = 1 ; iIter < argc ; iIter++)
  {
    change_to_user (argv[iIter]);
  }
  return 0;
}
