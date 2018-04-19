// quick and dirty program to set the terminal size to the maximum size, which is often a problem when
// connecting to an embedded platform

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <stdlib.h>

void set_window_size (int cols, int rows)
{
  struct winsize win;

  win.ws_row = rows;
  win.ws_col = cols;
  

  if (ioctl (STDIN_FILENO, TIOCSWINSZ, (char *) &win))
  {
    perror ("ioctl");
  }
}

struct termios clearTermIosFlags (int iFlags)
{
  struct termios oldt;
  struct termios newt;

  tcgetattr( STDIN_FILENO, &oldt);

  // set to new settings, turn off echo AND canonical mode
  newt = oldt;
  newt.c_lflag &= ~(ICANON) & ~(ECHO);          

  tcsetattr (STDIN_FILENO, TCSANOW, &newt);

  return oldt;
}

void setTermIos (struct termios newt)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &newt);
}

void setCursorPos (int iCol, int iRow)
{
  printf ("\e[%d;%df", iRow, iCol);
  fflush (stdout);
}

void getCursorPos (int *piCol, int *piRow)
{
  char *szPtr = NULL;
  char szRow[10] = "";
  char szCol[10] = "";
  int iMode;

  printf ("\e[6n");
  fflush (stdout);

  // read back the cursor position, returns as \e[<ROW>;<COLUMN>R
  for ( iMode = 0 ;; )
  {
    int iChar = getchar();

    switch (iMode)
    {
    case 0:
      if (iChar == '[')
      {
        szPtr = szRow;
        iMode = 1;
        // start reading Row
      }        
      break;
    case 1:
      if (iChar == ';')
      {
        *szPtr = '\0';
        szPtr = szCol;
        iMode = 2;
        // start reading Column
      }
      else
      {
        *szPtr = (char) iChar;
        szPtr++;
      }
      break;
    case 2:
      if (iChar == 'R')
      {
        *szPtr = '\0';
      }
      else
      {
        *szPtr = (char) iChar;
        szPtr++;
      }
      break;
    }
    
    if (iChar == 'R')
    {
      break;
    }
  }

  *piRow = atoi (szRow);
  *piCol = atoi (szCol);
}

int main (int argc, char **argv)
{
  struct termios oldt;
  int iMaxCol;
  int iMaxRow;
  int iCol=0;
  int iRow=0;

  // turn off canonical and echo
  oldt = clearTermIosFlags (ICANON | ECHO);

  if (isatty(STDOUT_FILENO))
  {
    // get current position
    getCursorPos (&iCol, &iRow);

    // force [x,y] position to furthest bottom and right of the screen, by using ridiculous numbers
    setCursorPos (9999, 9999);
  
    // query ACTUAL cursor position which will be the bottom right of the screen
    getCursorPos (&iMaxCol, &iMaxRow);

    // set the window size to the maximum column, and row value
    set_window_size (iMaxCol, iMaxRow);

    // reset the cursor position to where it was right after the user hit <return> to run this program
    setCursorPos (iCol, iRow);

    // report to the user what the size of the screen is
    printf ("Terminal size is %d columns and %d rows\n", iMaxCol, iMaxRow);

    // turn back on ICANON and ECHO
    setTermIos (oldt);
  }
  else
  {
    printf ("This doesn't appear to be connected to a terminal, no change\n");

    // turn back on ICANON and ECHO
    setTermIos (oldt);
  }

  return 0;
}
