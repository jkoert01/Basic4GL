/*  Main.cpp

    Copyright (C) 2004, Tom Mulgrew (tmulgrew@slingshot.co.nz)

    Simplified, cut down Basic4GL compiler
        * Compiles to console application
        * No UI
        * No OpenGL or other libraries
        * Only two functions (print & printr)

	Should be quite portable.
*/

#include <fstream>
#include <iostream>
#include "Compiler/TomComp.h"
#include "FunctionLibs/TomStdBasicLib.h"
#include "FunctionLibs/TomTrigBasicLib.h"
#include "FunctionLibs/TomFileIOBasicLib.h"
#include "FunctionLibs/TomWindowsBasicLib.h"
#include "FunctionLibs/GLBasicLib_gl.h"
#include "FunctionLibs/GLBasicLib_glu.h"
#include "FunctionLibs/DavyFunctionLib.h"

using namespace std;

// Source File
char* srcFile;
char buffer[1024];

void WrapPrint(TomVM& vm) {cout << vm.GetStringParam(1).c_str();}
void WrapPrintr(TomVM& vm) {cout << vm.GetStringParam(1).c_str() << endl;}

void startCompiler () {
	cout << "TomBasicCore BASIC compiler and virtual machine (C) Tom Mulgrew 2003-2004" << endl;
	cout << "Ported to linux by Jon Snape (Supermonkey) 2006"<< endl;
	cout << "OpenGL implimentation for linux port added by Davy Wybiral 2006"<< endl;
	cout << endl;

	// Create compiler and virtual machine
	TomVM vm;
	TomBasicCompiler comp(vm);
	FileOpener files;

	// Register function wrappers
	comp.AddFunction("print",  WrapPrint,  compParamTypeList()<<VTP_STRING, false, false, VTP_INT);
	comp.AddFunction("printr", WrapPrintr, compParamTypeList()<<VTP_STRING, false, false, VTP_INT);

	// Register other functions
	InitGLBasicLib_gl(comp);
	InitGLBasicLib_glu(comp);
	InitTomStdBasicLib(comp);
	InitTomTrigBasicLib(comp);
	InitTomFileIOBasicLib(comp, &files);
	InitTomWindowsBasicLib(comp, &files);
	InitDavyFunctionLib(comp);

	// Open File, read sourcecode
	cout<<"Opening file "<<srcFile<<"..."<<endl;
	ifstream file(srcFile);
	while(!file.eof()){
		file.getline(buffer,1023);
		comp.Parser().SourceCode().push_back(buffer);
	}

	// Compile program
	cout<<"Compiling..."<<endl;
	comp.ClearError();
	comp.Compile();
	if (comp.Error()) {
		cout << endl << "COMPILE ERROR!: " << comp.GetError().c_str() << endl;
		return;
	}

	// Run program
	cout<<"Running..."<<endl;
	vm.Reset();
	do {
		processEvents();
		vm.Continue(1000);
	} while (!(vm.Error() || vm.Done()));

	// Check for virtual machine error
	if(vm.Error()) cout << endl << "RUNTIME ERROR!: " << vm.GetError().c_str() << endl;
	else cout << endl << "Done!" << endl;
}

int main (int argc, char* argv[]) {
	// Set srcFile & catch argument errors
	if(argc==2) {srcFile = argv[1];}
	else {cout << "Incorrect number of arguments!" << endl; return 0;}
	// InitSDL(Width, Height, Title)
	InitSDL(640, 480, srcFile);
	InitGL (640, 480);
	startCompiler();
	return 0;
};
