#ifndef _DavyFunctionLibH
#define _DavyFunctionLibH
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include "../Compiler/TomComp.h"
#include "../VM/TomVM.h"


// Registration function
void endProgram(int code);
void keyDown(SDL_keysym* keysym);
void processEvents();
void InitGL(int width, int height);
void InitSDL(int width, int height, char title[]);
void InitDavyFunctionLib(TomBasicCompiler& comp);
#endif

