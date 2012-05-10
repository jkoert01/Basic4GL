//---------------------------------------------------------------------------
/*  Created 7-Dec-2003: Thomas Mulgrew

    Debugger types and routines
*/
#ifndef vmDebuggerH
#define vmDebuggerH
//---------------------------------------------------------------------------
#include "vmCode.h"
#include <list>
#include <map>

////////////////////////////////////////////////////////////////////////////////
// Breakpoint types

struct vmPatchedBreakPt {
    unsigned int    m_offset;                                       // Op-Code offset in program
    vmOpCode        m_replacedOpCode;                               // For active breakpoints: The op-code that has been replaced with the OP_BREAKPT.
};

struct vmTempBreakPt {
    unsigned int    m_offset;
};

struct vmUserBreakPt {
    unsigned int    m_offset;                                       // Note: Set to 0xffff if line is invalid.
};

typedef std::list<vmPatchedBreakPt> vmPatchedBreakPtList;
typedef std::list<vmTempBreakPt>    vmTempBreakPtList;
typedef std::map<unsigned int, vmUserBreakPt> vmUserBreakPts;       // Maps line number to breakpoint structure

inline vmTempBreakPt TempBreakPt (int offset) {
    vmTempBreakPt bp;
    bp.m_offset = offset;
    return bp;
}

#endif
