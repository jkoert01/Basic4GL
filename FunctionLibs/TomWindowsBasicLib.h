//---------------------------------------------------------------------------
// Created 22-Jun-2003: Thomas Mulgrew (tmulgrew@slingshot.co.nz)
// Copyright (C) Thomas Mulgrew
// Modified by Jon Snape (2007) for the linux-Basic4gl project
/*  Windows specific function and constant library
*/


#ifndef TomWindowsBasicLibH
#define TomWindowsBasicLibH
//---------------------------------------------------------------------------
#include "../Compiler/TomComp.h"
#include "../Routines/EmbeddedFiles.h"
// contains some windows specific functions that need to be used (mainly wrapped SDL functions)
#include "JonWindows.h"  

void InitTomWindowsBasicLib (TomBasicCompiler& comp, FileOpener *_files);
void ShutDownTomWindowsBasicLib ();
#endif
