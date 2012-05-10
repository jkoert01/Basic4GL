/* A replacement for windows.h, just offers some basic stuff
   that isn't directly portable to linux or has a different 
   function name (I'm lazy and it saves me changing stuff) */

#ifndef WINDOWSH
#define WINDOWSH

#define BYTE  unsigned char
#define DWORD  unsigned int
#define WORD  unsigned short int
#include <SDL/SDL.h>

long GetTickCount();
void Sleep(long ms);

#endif
