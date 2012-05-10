//---------------------------------------------------------------------------
/*  Created 7-Sep-2003: Thomas Mulgrew

    Code format and storage.
*/                                                 

#ifndef vmCodeH
#define vmCodeH
//---------------------------------------------------------------------------

#include "vmData.h"

#ifndef byte
typedef unsigned char byte;
#endif

////////////////////////////////////////////////////////////////////////////////
// vmOpCode
//
// Enumerated opcodes

enum vmOpCode {

    // General
    OP_NOP = 0x0,           // No operation
    OP_END,                 // End program
    OP_LOAD_CONST,          // Load constant into reg
    OP_LOAD_VAR,            // Load address of variable into reg
    OP_DEREF,               // Dereference reg (load value at [reg] into reg)
    OP_ADD_CONST,           // Add constant to reg. Used to address into a structure
    OP_ARRAY_INDEX,         // IN: reg = array index, reg2 = array address. OUT: reg = element address.
    OP_PUSH,                // Push reg to stack
    OP_POP,                 // Pop stack into reg2
    OP_SAVE,                // Save int, real or string in reg into [reg2]
    OP_COPY,                // Copy structure at [reg2] into [reg]. Instruction value points to data type.
    OP_DECLARE,             // Allocate a variable
    OP_TIMESHARE,           // Perform a timesharing break
    OP_FREE_TEMP,           // Free temporary data
    OP_ALLOC,               // Allocate variable memory
    OP_DATA_READ,           // Read program data into data at [reg]. Instruction contains target data type.
    OP_DATA_RESET,          // Reset program data pointer

    // Flow control
    OP_JUMP = 0x40,         // Unconditional jump
    OP_JUMP_TRUE,           // Jump if reg <> 0
    OP_JUMP_FALSE,          // Jump if reg == 0
    OP_CALL_FUNC,           // Call external function
    OP_CALL_OPERATOR_FUNC,  // Call external operator function
    OP_CALL,                // Call VM function
    OP_RETURN,              // Return from VM function

    // Operations
    // Mathematical
    OP_OP_NEG = 0x60,
    OP_OP_PLUS,         // (Doubles as string concatenation)
    OP_OP_MINUS,
    OP_OP_TIMES,
    OP_OP_DIV,
    OP_OP_MOD,

    // Logical
    OP_OP_NOT = 0x80,
    OP_OP_EQUAL,
    OP_OP_NOT_EQUAL,
    OP_OP_GREATER,
    OP_OP_GREATER_EQUAL,
    OP_OP_LESS,
    OP_OP_LESS_EQUAL,
    OP_OP_AND,
    OP_OP_OR,
    OP_OP_XOR,

    // Conversion
    OP_CONV_INT_REAL = 0xa0,    // Convert integer in reg to real
    OP_CONV_INT_STRING,         // Convert integer in reg to string
    OP_CONV_REAL_STRING,        // Convert real in reg to string
    OP_CONV_REAL_INT,
    OP_CONV_INT_REAL2,          // Convert integer in reg2 to real
    OP_CONV_INT_STRING2,        // Convert integer in reg2 to string
    OP_CONV_REAL_STRING2,       // Convert real in reg2 to string
    OP_CONV_REAL_INT2,

    // Misc routine
    OP_RUN = 0xc0,              // Restart program. Reinitialises variables, display, state e.t.c

    // Debugging
    OP_BREAKPT = 0xe0           // Breakpoint
};

std::string vmOpCodeName (vmOpCode code);

////////////////////////////////////////////////////////////////////////////////
// vmInstruction
#pragma pack (push, 1)
struct vmInstruction {

    // Note:    vmInstruction size = 12 bytes.
    //          Ordering of member fields is important, as the two 4 byte members
    //          are first (and hence aligned to a 4 byte boundary).
    //          m_sourceChar is next, and aligned to a 2 byte boundary.
    //          Single byte fields are last, as their alignment is unimportant.
    vmValue         m_value;                // Value
    unsigned int    m_sourceLine;           // For debugging
    unsigned short  m_sourceChar;
    byte            m_opCode;               // (vmOpCode)
    char            m_type;                 // (vmBasicVarType)

    vmInstruction () {
        m_opCode        = 0;
        m_type          = 0;
        m_sourceLine    = 0;
        m_sourceChar    = 0;
        m_value         = 0;
    }
    vmInstruction (const vmInstruction& i) {
        m_opCode        = i.m_opCode;
        m_type          = i.m_type;
        m_sourceChar    = i.m_sourceChar;
        m_sourceLine    = i.m_sourceLine;
        m_value         = i.m_value;
    }
    vmInstruction (byte opCode, byte type, vmValue val, int sourceLine = 0, int sourceChar = 0) {
        m_opCode        = opCode;
        m_type          = type;
        m_value         = val;
        m_sourceLine    = sourceLine;
        m_sourceChar    = sourceChar;
    }

    // Streaming
#ifdef VM_STATE_STREAMING
    void StreamOut (std::ostream& stream);
    void StreamIn (std::istream& stream);
#endif
};
#pragma pack (pop)

#endif
