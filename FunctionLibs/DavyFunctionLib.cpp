#include "DavyFunctionLib.h"
#include <iostream>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/freeglut.h>
#include <GL/Glext.h>

using namespace std;


//####### SDL & Window functions below #######//

// Kill program
void endProgram(int code) {SDL_Quit();	exit(code);}

// Process keypresses
void keyDown(SDL_keysym* keysym) {
	switch(keysym->sym) {
		case SDLK_ESCAPE:
			endProgram(0);
			break;
	}
}

// Process events
void processEvents() {
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_KEYDOWN:	keyDown(&event.key.keysym);	break;
			case SDL_QUIT: endProgram(0);	break;
		}
	}
}

// Set initial perspective
void InitGL(int width, int height) {
	glClearColor(0, 0, 0, 0);
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glEnable(GL_DEPTH_TEST);
	gluPerspective(60.0, (float)width/height, 1.0, 1024.0);
	glMatrixMode(GL_MODELVIEW);
}

// Setup SDL window
void InitSDL(int width, int height, char title[]){
	const SDL_VideoInfo* info = NULL;
	int bpp = 0;
	if(SDL_Init(SDL_INIT_VIDEO)< 0){cout << "Video initialization failed!" << endl;	endProgram(1);}
	info = SDL_GetVideoInfo();
	if(!info) {cout << "Video query failed!" << endl; endProgram(1);}
	bpp = info->vfmt->BitsPerPixel;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
	if(SDL_SetVideoMode(width, height, bpp, SDL_OPENGL)==0) {cout << "Video mode set failed!" << endl; endProgram(1);}
	SDL_WM_SetCaption(title,"linB4GL");
}

//####### Basic4GL function wraps below #######//

// Load texture using SDL_image, return GLuint
GLuint LoadTexture(const char filename [], bool mipmap){
	glPushAttrib (GL_ALL_ATTRIB_BITS);
	GLuint texture;
	SDL_Surface* imgFile = IMG_Load(filename);
	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D,texture);
	if(mipmap){
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, imgFile->w, imgFile->h, GL_RGB, GL_UNSIGNED_BYTE, imgFile->pixels);
	} else {
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D (GL_TEXTURE_2D, 0, 3, imgFile->w, imgFile->h, 0, GL_RGB, GL_UNSIGNED_BYTE, imgFile->pixels);
	}
	glPopAttrib ();
	return texture;
}

void WrapLoadTexture(TomVM& vm){
	vm.Reg().IntVal() = LoadTexture(vm.GetStringParam(1).c_str(), false);
}

void WrapLoadMipMapTexture(TomVM& vm){
	vm.Reg().IntVal() = LoadTexture(vm.GetStringParam(1).c_str(), true);
}

void WrapglGenTexture(TomVM& vm) {
	GLuint texture;
	glGenTextures (1, &texture);
	vm.Reg().IntVal() = texture;
}

void WrapglDeleteTexture(TomVM& vm) {
	GLuint texture = vm.GetIntParam(1);
	glDeleteTextures(1, &texture);
}

/*void WrapglMultiTexCoord2f(TomVM& vm) {
	if (glMultiTexCoord2f != NULL)
		glMultiTexCoord2f(vm.GetIntParam (3), vm.GetRealParam(2), vm.GetRealParam(1));
}

void WrapglMultiTexCoord2d(TomVM& vm) {
	if (glMultiTexCoord2d != NULL)
		glMultiTexCoord2d (vm.GetIntParam(3), vm.GetRealParam(2), vm.GetRealParam(1));
}

void WrapglActiveTexture(TomVM& vm) {
	if (glActiveTexture != NULL)
		glActiveTexture (vm.GetIntParam(1));
}
*/

void WrapglGetString(TomVM& vm) {
	vm.RegString() = (char*)glGetString(vm.GetIntParam(1));
}

// SwapBuffers
void WrapSwapBuffers(TomVM& vm){
	SDL_GL_SwapBuffers();
}

// Register all functions
void InitDavyFunctionLib(TomBasicCompiler& comp) {

	compParamTypeList noParam;

	// Register constants
	comp.AddConstant ("GL_ACTIVE_TEXTURE",          GL_ACTIVE_TEXTURE_ARB);
	comp.AddConstant ("GL_CLIENT_ACTIVE_TEXTURE",   GL_CLIENT_ACTIVE_TEXTURE_ARB);
	comp.AddConstant ("GL_MAX_TEXTURE_UNITS",       GL_MAX_TEXTURE_UNITS_ARB);
	comp.AddConstant ("GL_TEXTURE0",                GL_TEXTURE0_ARB);
	comp.AddConstant ("GL_TEXTURE1",                GL_TEXTURE1_ARB);
	comp.AddConstant ("GL_TEXTURE2",                GL_TEXTURE2_ARB);
	comp.AddConstant ("GL_TEXTURE3",                GL_TEXTURE3_ARB);
	comp.AddConstant ("GL_TEXTURE4",                GL_TEXTURE4_ARB);
	comp.AddConstant ("GL_TEXTURE5",                GL_TEXTURE5_ARB);
	comp.AddConstant ("GL_TEXTURE6",                GL_TEXTURE6_ARB);
	comp.AddConstant ("GL_TEXTURE7",                GL_TEXTURE7_ARB);
	comp.AddConstant ("GL_TEXTURE8",                GL_TEXTURE8_ARB);
	comp.AddConstant ("GL_TEXTURE9",                GL_TEXTURE9_ARB);
	comp.AddConstant ("GL_TEXTURE10",               GL_TEXTURE10_ARB);
	comp.AddConstant ("GL_TEXTURE11",               GL_TEXTURE11_ARB);
	comp.AddConstant ("GL_TEXTURE12",               GL_TEXTURE12_ARB);
	comp.AddConstant ("GL_TEXTURE13",               GL_TEXTURE13_ARB);
	comp.AddConstant ("GL_TEXTURE14",               GL_TEXTURE14_ARB);
	comp.AddConstant ("GL_TEXTURE15",               GL_TEXTURE15_ARB);
	comp.AddConstant ("GL_TEXTURE16",               GL_TEXTURE16_ARB);
	comp.AddConstant ("GL_TEXTURE17",               GL_TEXTURE17_ARB);
	comp.AddConstant ("GL_TEXTURE18",               GL_TEXTURE18_ARB);
	comp.AddConstant ("GL_TEXTURE19",               GL_TEXTURE19_ARB);
	comp.AddConstant ("GL_TEXTURE20",               GL_TEXTURE20_ARB);
	comp.AddConstant ("GL_TEXTURE21",               GL_TEXTURE21_ARB);
	comp.AddConstant ("GL_TEXTURE22",               GL_TEXTURE22_ARB);
	comp.AddConstant ("GL_TEXTURE23",               GL_TEXTURE23_ARB);
	comp.AddConstant ("GL_TEXTURE24",               GL_TEXTURE24_ARB);
	comp.AddConstant ("GL_TEXTURE25",               GL_TEXTURE25_ARB);
	comp.AddConstant ("GL_TEXTURE26",               GL_TEXTURE26_ARB);
	comp.AddConstant ("GL_TEXTURE27",               GL_TEXTURE27_ARB);
	comp.AddConstant ("GL_TEXTURE28",               GL_TEXTURE28_ARB);
	comp.AddConstant ("GL_TEXTURE29",               GL_TEXTURE29_ARB);
	comp.AddConstant ("GL_TEXTURE30",               GL_TEXTURE30_ARB);
	comp.AddConstant ("GL_TEXTURE31",               GL_TEXTURE31_ARB);
	comp.AddConstant ("GL_ACTIVE_TEXTURE_ARB",          GL_ACTIVE_TEXTURE_ARB);
	comp.AddConstant ("GL_CLIENT_ACTIVE_TEXTURE_ARB",   GL_CLIENT_ACTIVE_TEXTURE_ARB);
	comp.AddConstant ("GL_MAX_TEXTURE_UNITS_ARB",       GL_MAX_TEXTURE_UNITS_ARB);
	comp.AddConstant ("GL_TEXTURE0_ARB",                GL_TEXTURE0_ARB);
	comp.AddConstant ("GL_TEXTURE1_ARB",                GL_TEXTURE1_ARB);
	comp.AddConstant ("GL_TEXTURE2_ARB",                GL_TEXTURE2_ARB);
	comp.AddConstant ("GL_TEXTURE3_ARB",                GL_TEXTURE3_ARB);
	comp.AddConstant ("GL_TEXTURE4_ARB",                GL_TEXTURE4_ARB);
	comp.AddConstant ("GL_TEXTURE5_ARB",                GL_TEXTURE5_ARB);
	comp.AddConstant ("GL_TEXTURE6_ARB",                GL_TEXTURE6_ARB);
	comp.AddConstant ("GL_TEXTURE7_ARB",                GL_TEXTURE7_ARB);
	comp.AddConstant ("GL_TEXTURE8_ARB",                GL_TEXTURE8_ARB);
	comp.AddConstant ("GL_TEXTURE9_ARB",                GL_TEXTURE9_ARB);
	comp.AddConstant ("GL_TEXTURE10_ARB",               GL_TEXTURE10_ARB);
	comp.AddConstant ("GL_TEXTURE11_ARB",               GL_TEXTURE11_ARB);
	comp.AddConstant ("GL_TEXTURE12_ARB",               GL_TEXTURE12_ARB);
	comp.AddConstant ("GL_TEXTURE13_ARB",               GL_TEXTURE13_ARB);
	comp.AddConstant ("GL_TEXTURE14_ARB",               GL_TEXTURE14_ARB);
	comp.AddConstant ("GL_TEXTURE15_ARB",               GL_TEXTURE15_ARB);
	comp.AddConstant ("GL_TEXTURE16_ARB",               GL_TEXTURE16_ARB);
	comp.AddConstant ("GL_TEXTURE17_ARB",               GL_TEXTURE17_ARB);
	comp.AddConstant ("GL_TEXTURE18_ARB",               GL_TEXTURE18_ARB);
	comp.AddConstant ("GL_TEXTURE19_ARB",               GL_TEXTURE19_ARB);
	comp.AddConstant ("GL_TEXTURE20_ARB",               GL_TEXTURE20_ARB);
	comp.AddConstant ("GL_TEXTURE21_ARB",               GL_TEXTURE21_ARB);
	comp.AddConstant ("GL_TEXTURE22_ARB",               GL_TEXTURE22_ARB);
	comp.AddConstant ("GL_TEXTURE23_ARB",               GL_TEXTURE23_ARB);
	comp.AddConstant ("GL_TEXTURE24_ARB",               GL_TEXTURE24_ARB);
	comp.AddConstant ("GL_TEXTURE25_ARB",               GL_TEXTURE25_ARB);
	comp.AddConstant ("GL_TEXTURE26_ARB",               GL_TEXTURE26_ARB);
	comp.AddConstant ("GL_TEXTURE27_ARB",               GL_TEXTURE27_ARB);
	comp.AddConstant ("GL_TEXTURE28_ARB",               GL_TEXTURE28_ARB);
	comp.AddConstant ("GL_TEXTURE29_ARB",               GL_TEXTURE29_ARB);
	comp.AddConstant ("GL_TEXTURE30_ARB",               GL_TEXTURE30_ARB);
	comp.AddConstant ("GL_TEXTURE31_ARB",               GL_TEXTURE31_ARB);
	comp.AddConstant ("GL_COMBINE",        GL_COMBINE_EXT);
	comp.AddConstant ("GL_COMBINE_RGB",    GL_COMBINE_RGB_EXT);
	comp.AddConstant ("GL_COMBINE_ALPHA",  GL_COMBINE_ALPHA_EXT);
	comp.AddConstant ("GL_RGB_SCALE",      GL_RGB_SCALE_EXT);
	comp.AddConstant ("GL_ADD_SIGNED",     GL_ADD_SIGNED_EXT);
	comp.AddConstant ("GL_INTERPOLATE",    GL_INTERPOLATE_EXT);
	comp.AddConstant ("GL_CONSTANT",       GL_CONSTANT_EXT);
	comp.AddConstant ("GL_PRIMARY_COLOR",  GL_PRIMARY_COLOR_EXT);
	comp.AddConstant ("GL_PREVIOUS",       GL_PREVIOUS_EXT);
	comp.AddConstant ("GL_SOURCE0_RGB",    GL_SOURCE0_RGB_EXT);
	comp.AddConstant ("GL_SOURCE1_RGB",    GL_SOURCE1_RGB_EXT);
	comp.AddConstant ("GL_SOURCE2_RGB",    GL_SOURCE2_RGB_EXT);
	comp.AddConstant ("GL_SOURCE0_ALPHA",  GL_SOURCE0_ALPHA_EXT);
	comp.AddConstant ("GL_SOURCE1_ALPHA",  GL_SOURCE1_ALPHA_EXT);
	comp.AddConstant ("GL_SOURCE2_ALPHA",  GL_SOURCE2_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND0_RGB",   GL_OPERAND0_RGB_EXT);
	comp.AddConstant ("GL_OPERAND1_RGB",   GL_OPERAND1_RGB_EXT);
	comp.AddConstant ("GL_OPERAND2_RGB",   GL_OPERAND2_RGB_EXT);
	comp.AddConstant ("GL_OPERAND0_ALPHA", GL_OPERAND0_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND1_ALPHA", GL_OPERAND1_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND2_ALPHA", GL_OPERAND2_ALPHA_EXT);
	comp.AddConstant ("GL_COMBINE_EXT",        GL_COMBINE_EXT);
	comp.AddConstant ("GL_COMBINE_RGB_EXT",    GL_COMBINE_RGB_EXT);
	comp.AddConstant ("GL_COMBINE_ALPHA_EXT",  GL_COMBINE_ALPHA_EXT);
	comp.AddConstant ("GL_RGB_SCALE_EXT",      GL_RGB_SCALE_EXT);
	comp.AddConstant ("GL_ADD_SIGNED_EXT",     GL_ADD_SIGNED_EXT);
	comp.AddConstant ("GL_INTERPOLATE_EXT",    GL_INTERPOLATE_EXT);
	comp.AddConstant ("GL_CONSTANT_EXT",       GL_CONSTANT_EXT);
	comp.AddConstant ("GL_PRIMARY_COLOR_EXT",  GL_PRIMARY_COLOR_EXT);
	comp.AddConstant ("GL_PREVIOUS_EXT",       GL_PREVIOUS_EXT);
	comp.AddConstant ("GL_SOURCE0_RGB_EXT",    GL_SOURCE0_RGB_EXT);
	comp.AddConstant ("GL_SOURCE1_RGB_EXT",    GL_SOURCE1_RGB_EXT);
	comp.AddConstant ("GL_SOURCE2_RGB_EXT",    GL_SOURCE2_RGB_EXT);
	comp.AddConstant ("GL_SOURCE0_ALPHA_EXT",  GL_SOURCE0_ALPHA_EXT);
	comp.AddConstant ("GL_SOURCE1_ALPHA_EXT",  GL_SOURCE1_ALPHA_EXT);
	comp.AddConstant ("GL_SOURCE2_ALPHA_EXT",  GL_SOURCE2_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND0_RGB_EXT",   GL_OPERAND0_RGB_EXT);
	comp.AddConstant ("GL_OPERAND1_RGB_EXT",   GL_OPERAND1_RGB_EXT);
	comp.AddConstant ("GL_OPERAND2_RGB_EXT",   GL_OPERAND2_RGB_EXT);
	comp.AddConstant ("GL_OPERAND0_ALPHA_EXT", GL_OPERAND0_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND1_ALPHA_EXT", GL_OPERAND1_ALPHA_EXT);
	comp.AddConstant ("GL_OPERAND2_ALPHA_EXT", GL_OPERAND2_ALPHA_EXT);

	// Register functions
	comp.AddFunction ("loadtexture", WrapLoadTexture, compParamTypeList()<<VTP_STRING, true,  true,  VTP_INT);
	comp.AddFunction ("loadmipmaptexture", WrapLoadMipMapTexture, compParamTypeList()<<VTP_STRING, true,  true,  VTP_INT);
	comp.AddFunction ("glgentexture", WrapglGenTexture, noParam, true, true, VTP_INT);
	comp.AddFunction ("gldeletetexture", WrapglDeleteTexture, compParamTypeList()<<VTP_INT, true, false, VTP_INT);
//	comp.AddFunction ("glmultitexcoord2f", WrapglMultiTexCoord2f, compParamTypeList()<<VTP_INT<<VTP_REAL<<VTP_REAL, true, false, VTP_INT);
//	comp.AddFunction ("glmultitexcoord2d", WrapglMultiTexCoord2d, compParamTypeList()<<VTP_INT<<VTP_REAL<<VTP_REAL, true, false, VTP_INT);
//	comp.AddFunction ("glactivetexture", WrapglActiveTexture, compParamTypeList () << VTP_INT, true, false, VTP_INT);
	comp.AddFunction ("glgetstring", WrapglGetString, compParamTypeList()<<VTP_INT, true, true, VTP_STRING);
	comp.AddFunction ("swapbuffers", WrapSwapBuffers, noParam, true,  false,  VTP_INT);
}
