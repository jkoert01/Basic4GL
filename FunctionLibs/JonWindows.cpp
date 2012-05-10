#include "JonWindows.h"

long GetTickCount(){
     return SDL_GetTicks();
}

void Sleep(long ms){
   SDL_Delay(ms);
}
