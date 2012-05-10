//---------------------------------------------------------------------------
/*  Created 1-Sep-2003: Thomas Mulgrew

    'TomVM' Virtual Machine.
*/

#ifndef TomVMH
#define TomVMH
//---------------------------------------------------------------------------
#include "vmVariables.h"
#include "vmCode.h"
#include "HasErrorState.h"
#include "vmFunction.h"
#include "vmDebugger.h"
//#include "EmbeddedFiles.h"

#define VM_MAXSTACKCALLS 1000000        // 1,000,000 stack calls (4 meg stack space)
#define VM_MAXDATA       100000000      // 100,000,000 variables (.4 gig of memory)
#define VM_DATATOSTRINGMAXCHARS 4000

////////////////////////////////////////////////////////////////////////////////
// VM state
//
// Used to save virtual machine state.
// (Debugger uses this to run small sections of code, without interrupting the
// main program.)

struct vmState {

    // Instruction pointer
    unsigned int ip;

    // Registers
    vmValue         reg, reg2;
    std::string     regString, reg2String;

    // Stacks
    unsigned int    stackTop, callStackTop;

    // Top of program
    unsigned int    codeSize;

    // Top of variable data
    unsigned int    dataSize, tempDataStart;

    // Error state
    bool            error;
    std::string     errorString;

    // Other state
    bool            paused;
};

////////////////////////////////////////////////////////////////////////////////
// TomVM
//
// Virtual machine

class TomVM : public HasErrorState {

    ////////////////////////////////////
    // Data

    // Variables, data and data types
    vmTypeLibrary               m_dataTypes;
    vmData                      m_data;
    vmVariables                 m_variables;
	std::vector<vmString>       m_stringConstants;      // Constant strings declared in program
    vmStore<vmString>           m_strings;
    std::list<vmResources *>    m_resources;

    // Program data
    vmProgramData               m_programData;          // General purpose program data (e.g declared with "DATA" keyword in BASIC)
    unsigned int                m_programDataOffset;

    // Registers
    vmValue                     m_reg,                  // Register values (when int or float)
                                m_reg2;
    std::string                 m_regString,            // Register values when string
                                m_reg2String;

    // Runtime stacks
    vmValueStack                m_stack;                // Used for expression evaluation
    std::vector<unsigned int>   m_callStack;            // Stores gosub return addresses

    ////////////////////////////////////
    // Code

    // Instructions
    std::vector<vmInstruction>  m_code;
    vmValTypeSet                m_typeSet;

    // Instruction pointer
    unsigned int                m_ip;

    // Debugging
    vmPatchedBreakPtList        m_patchedBreakPts;      // Patched in breakpoints
    vmTempBreakPtList           m_tempBreakPts;         // Temporary breakpoints, generated for stepping over a line
    vmUserBreakPts              m_userBreakPts;         // User specified breakpoints. Stored as a map from line number to breakpoint

    bool                        m_paused,               // Set to true when program hits a breakpoint. (Or can be set by caller.)
                                m_breakPtsPatched;      // Set to true if breakpoints are patched and in synchronisation with compiled code

    // Internal methods
    void BlockCopy          (int sourceIndex, int destIndex, int size);
    void CopyStructure      (int sourceIndex, int destIndex, vmValType& type);
    void CopyArray          (int sourceIndex, int destIndex, vmValType& type);
    void CopyField          (int sourceIndex, int destIndex, vmValType& type);
    bool CopyData           (int sourceIndex, int destIndex, vmValType type);
    bool PopArrayDimensions (vmValType& type);
    bool ValidateTypeSize   (vmValType& type);
    bool ReadProgramData    (vmBasicValType type);
    void PatchInBreakPt (unsigned int offset);
    void InternalPatchOut ();
    void InternalPatchIn ();
    void PatchOut () {
        if (m_breakPtsPatched)
            InternalPatchOut ();
    }
    unsigned int CalcBreakPtOffset (unsigned int line);
    void Deref (vmValue& val, vmValType& type);

public:
    TomVM (int maxDataSize = VM_MAXDATA);

    // General
    void New ();                                        // New program
    void Clr ();                                        // Clear variables
    void Reset ();
    void Continue (unsigned int steps = 0xffffffff);    // Continue execution from last position
    bool Done () {
        assert (m_ip < m_code.size ());
        return m_code [m_ip].m_opCode == OP_END;        // Reached end of program?
    }
    void GetIPInSourceCode (int& line, int& col) {
        assert (m_ip < m_code.size ());
        line    = m_code [m_ip].m_sourceLine;
        col     = m_code [m_ip].m_sourceChar;
    }

    // External functions
    std::vector<vmFunction>     m_functions;
    std::vector<vmFunction>     m_operatorFunctions;
        // m_functions are standard functions where the parameters are pushed
        // to the stack.
        // m_operatorFunctions are generally used for language extension, and
        // perform the job of either a unary or binary operator.
        // That is, they perform Reg2 operator Reg1, and place the result in
        // Reg1.

    // Initialisation functions
    std::vector<vmFunction>     m_initFunctions;

    // IP and registers
    int             IP ()           { return m_ip; }
    vmValue&        Reg ()          { return m_reg; }
    vmValue&        Reg2 ()         { return m_reg2; }
    std::string&    RegString ()    { return m_regString; }
    std::string&    Reg2String ()   { return m_reg2String; }
    vmValueStack&   Stack ()        { return m_stack; }

    // Variables, data and data types
    vmTypeLibrary&  DataTypes ()    { return m_dataTypes; }
    vmData&         Data ()         { return m_data; }
    vmVariables&    Variables ()    { return m_variables; }
    vmProgramData&  ProgramData ()  { return m_programData; }

    // Debugging
    bool            Paused ()           { return m_paused; }
    void            Pause ()            { m_paused = true; }
    bool            BreakPtsPatched ()  { return m_breakPtsPatched; }
    bool IsUserBreakPt (int line)               { return m_userBreakPts.find (line) != m_userBreakPts.end (); }
    vmUserBreakPt& GetUserBreakPt (int line)    { assert (IsUserBreakPt (line)); return m_userBreakPts [line]; }
    void SetUserBreakPt (int line, vmUserBreakPt& bp) {
        PatchOut ();
        bp.m_offset = CalcBreakPtOffset (line);
        m_userBreakPts [line] = bp;
    }
    void ClearUserBreakPt (int line) {
        PatchOut ();
        m_userBreakPts.erase (m_userBreakPts.find (line));
    }
    void ClearUserBreakPts () {
        PatchOut ();
        m_userBreakPts.clear ();
    }
    void ClearTempBreakPts () {
        PatchOut ();
        m_tempBreakPts.clear ();
    }
    void PatchIn () {
        if (!m_breakPtsPatched)
            InternalPatchIn ();
    }
    void AddStepBreakPts (bool stepInto);               // Add temporary breakpoints to catch execution after stepping over the current line
    bool AddStepOutBreakPt ();                          // Add breakpoint to step out of gosub
    std::vector<unsigned int>& CallStack () { return m_callStack; }
    vmState GetState ();
    void SetState (vmState& state);
    void GotoInstruction (unsigned int offset) {
        assert (offset < InstructionCount ());
        m_ip = offset;
    }
    std::string RegToString (vmValType& type);

    // Building raw VM instructions
    unsigned int InstructionCount ()        { return m_code.size (); }
    void AddInstruction (vmInstruction i)   { PatchOut (); m_code.push_back (i); }
    void RollbackProgram (int size) {
        assert (size >= 0);
        assert (size <= InstructionCount());
        while (size < InstructionCount ())
            m_code.pop_back ();
    }
    vmInstruction& Instruction (unsigned int index)  {
        assert (index < m_code.size ());
        PatchOut ();
        return m_code [index];
    }
    void RemoveLastInstruction () {
        m_code.pop_back ();
    }
    int StoreType (vmValType& type)         { return m_typeSet.GetIndex (type); }
    vmValType& GetStoredType (int index)    { return m_typeSet.GetValType (index); }

    // Program data
    void StoreProgramData (vmBasicValType t, vmValue v) {
        vmProgramDataElement d;
        d.Type () = t;
        d.Value () = v;
        m_programData.push_back (d);
    }

    // Constant strings
    int StoreStringConstant (vmString str);
    std::vector<vmString>& StringConstants () { return m_stringConstants; }

    // External functions
    int AddFunction (vmFunction func)   {
        int result = FunctionCount ();
        m_functions.push_back (func);
        return result;
    }
    int FunctionCount ()                { return m_functions.size (); }
    int AddOperatorFunction (vmFunction func) {
        int result = OperatorFunctionCount ();
        m_operatorFunctions.push_back (func);
        return result;
    }
    int OperatorFunctionCount ()        { return m_operatorFunctions.size (); }

    // Called by external functions
    vmValue& GetParam (int index) {
        // Read param from param stack.
        // Index 1 is TOS
        // Index 2 is TOS - 1
        // ...
        assert (index > 0);
        assert (index <= m_stack.Size ());
        return m_stack [m_stack.Size () - index];
    }
    vmInt GetIntParam (int index)               { return GetParam (index).IntVal ();  }
    vmReal GetRealParam (int index)             { return GetParam (index).RealVal (); }
    std::string& GetStringParam (int index)     { return m_strings.Value (GetIntParam (index)); }

    // Reference params (called by external functions)
    bool CheckNullRefParam (int index) {

        // Return true if param is not a null reference
        // Otherwise return false and set a virtual machine error
        bool result = GetIntParam (index) > 0;
        if (!result)
            FunctionError ("Unset pointer");
        return result;
    }
    vmValue& GetRefParam (int index) {

        // Get reference parameter.
        // Returns a reference to the actual vmValue object
        int ptr = GetIntParam (index);
        assert (ptr > 0);
        assert (m_data.IndexValid (ptr));
        return m_data.Data () [ptr];
    }

    void FunctionError (std::string name)       { SetError ((std::string) "Function error: " + name); }
    void MiscError (std::string name)           { SetError (name); }

    // Initialisation functions
    void AddInitFunc (vmFunction func) { m_initFunctions.push_back (func); }

    // Resources
    void AddResources (vmResources& resources) { m_resources.push_back (&resources); }
    void ClearResources ();

    // Displaying data
    std::string BasicValToString (vmValue val, vmBasicValType type, bool constant);
    std::string ValToString (vmValue val, vmValType type, int& maxChars);
    std::string VarToString (vmVariable& v, int maxChars);

    // Streaming
#ifdef VM_STATE_STREAMING
    void StreamOut (std::ostream& stream);
    void StreamIn (std::istream& stream);
#endif
};

#endif
