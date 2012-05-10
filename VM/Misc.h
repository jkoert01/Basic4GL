//---------------------------------------------------------------------------
/*  Created 08-Oct-2003: Thomas Mulgrew

    Miscellaneous routines, macros e.t.c.
    Mainly created for resolving differences between compilers
*/

#ifndef MiscH
#define MiscH
//---------------------------------------------------------------------------

#ifndef _MSC_VER
// Visual C++ declares tolower in <ctype.h>
// Borland C++ declares tolower in <string>
#include <string>
inline char tolower (char c) { return std::tolower (c); }
inline char toupper (char c) { return std::toupper (c); }

#pragma warning( disable : W8026 )


#else

// Suppressing the following warning stops Visual C++ spewing out loads of garbage about
// truncating symbols to 255 characters in the debug information.
#pragma warning( disable : 4786 ) 
#pragma warning( disable : 4800 ) 

#endif

#endif
