//---------------------------------------------------------------------------
/*  Created 1-Sep-2003: Thomas Mulgrew

    'TomVM' Virtual Machine.                          
*/


#pragma hdrstop

#include "TomVM.h"

//---------------------------------------------------------------------------

#ifndef _MSC_VER
#pragma package(smart_init)
#endif

const char *blankString = "";

std::string streamHeader = "Basic4GL stream";
int         streamVersion = 1;

////////////////////////////////////////////////////////////////////////////////
// TomVM

TomVM::TomVM (int maxDataSize)
        :   m_data              (maxDataSize),
            m_variables         (m_data, m_dataTypes),
            m_strings           (blankString),
            m_stack             (m_strings) {
    New ();
}

void TomVM::New () {

    // Clear variables, data and data types
    Clr ();
    m_variables.Clear ();
    m_dataTypes.Clear ();
    m_programData.clear ();

    // Clear string constants
    m_stringConstants.clear ();

    // Deallocate code
    m_code.clear ();
    m_typeSet.Clear ();
    m_ip = 0;
    m_paused = false;

    // Clear breakpoints
    m_patchedBreakPts.clear ();
    m_tempBreakPts.clear ();
    m_breakPtsPatched = false;
}

void TomVM::Clr () {
    m_variables.Deallocate ();          // Deallocate variables
    m_data.Clear ();                    // Deallocate variable data
    m_strings.Clear ();                 // Clear strings
    m_stack.Clear ();                   // Clear runtime stacks
    m_callStack.clear ();

    // Clear resources
    ClearResources ();

    // Init registers
    m_reg.IntVal ()     = 0;
    m_reg2.IntVal ()    = 0;
    m_regString         = "";
    m_reg2String        = "";
    m_programDataOffset = 0;
}

void TomVM::ClearResources () {

    // Clear resources
    for (   std::list<vmResources *>::iterator j = m_resources.begin ();
            j != m_resources.end ();
            j++)
        (*j)->Clear ();
}

void TomVM::Reset () {

    // Clear error state
    ClearError ();

    // Deallocate variables
    Clr ();

    // Call registered initialisation functions
    for (int i = 0; i < m_initFunctions.size (); i++)
        m_initFunctions [i] (*this);

    // Move to start of program
    m_ip = 0;
    m_paused = false;
}

char    *ErrNotImplemented          = "Opcode not implemented",
        *ErrInvalid                 = "Invalid opcode",
        *ErrUnDIMmedVariable        = "UnDIMmed variable",
        *ErrBadArrayIndex           = "Array index out of range",
        *ErrReDIMmedVariable        = "ReDIMmed variable",
        *ErrBadDeclaration          = "Variable declaration error",
        *ErrVariableTooBig          = "Variable is too big",
        *ErrOutOfMemory             = "Ran out of variable memory",
        *ErrBadOperator             = "Operator cannot be applied to data type",
        *ErrReturnWithoutGosub      = "Return without gosub",
        *ErrStackError              = "Stack error",
        *ErrUnsetPointer            = "Unset pointer",
        *ErrStackOverflow           = "Stack overflow",
        *ErrArraySizeMismatch       = "Array sizes are different",
        *ErrZeroLengthArray         = "Array size must be 0 or greater",
        *ErrOutOfData               = "Out of DATA",
        *ErrDataIsString            = "Expected to READ a number, got a text string instead";

void TomVM::Continue (unsigned int steps) {
    ClearError ();
    m_paused = false;

    ////////////////////////////////////////////////////////////////////////////
    // Virtual machine main loop
    vmInstruction *instruction;
    unsigned int stepCount = 0;
    vmValue temp;
    unsigned int tempI;

    goto step;

nextStep:

    // Proceed to next instruction
    m_ip++;

step:

    // Count steps
    if (++stepCount > steps)
        return;

    instruction = &m_code [m_ip];
    switch (instruction->m_opCode) {
    case OP_NOP:                goto nextStep;
    case OP_END:                break;
    case OP_LOAD_CONST:

        // Load value
        if (instruction->m_type == VTP_STRING) {
            assert (instruction->m_value.IntVal () >= 0);
            assert (instruction->m_value.IntVal () < m_stringConstants.size ());
            RegString () = m_stringConstants [instruction->m_value.IntVal ()];
        }
        else
            Reg () = instruction->m_value;
        goto nextStep;

    case OP_LOAD_VAR: {

        // Load variable.
        // Instruction contains index of variable.
        assert (m_variables.IndexValid (instruction->m_value.IntVal ()));
        vmVariable& var = m_variables.Variables () [instruction->m_value.IntVal ()];
        if (var.Allocated ()) {
            // Load address of variable's data into register
            m_reg.IntVal () = var.m_dataIndex;
            goto nextStep;
        }
        SetError (ErrUnDIMmedVariable);
        break;
    }
    case OP_DEREF: {

        // Dereference reg.
        if (m_reg.IntVal () != 0) {
            assert (m_data.IndexValid (m_reg.IntVal ()));
            vmValue& val = m_data.Data () [m_reg.IntVal ()];        // Find value that reg points to
            switch (instruction->m_type) {
            case VTP_INT:
            case VTP_REAL:
                m_reg = val;
                goto nextStep;
            case VTP_STRING:
                assert (m_strings.IndexValid (val.IntVal ()));
                m_regString = m_strings.Value (val.IntVal ());
                goto nextStep;
            }
            assert (false);
        }
        SetError (ErrUnsetPointer);
        break;
    }
    case OP_ADD_CONST:
        // Check pointer
        if (m_reg.IntVal () != 0) {
            m_reg.IntVal () += instruction->m_value.IntVal ();
            goto nextStep;
        }
        SetError (ErrUnsetPointer);
        break;

    case OP_ARRAY_INDEX:

        if (m_reg2.IntVal () != 0) {
            // Input:   m_reg2 = Array address
            //          m_reg  = Array index
            // Output:  m_reg  = Element address
            assert (m_data.IndexValid (m_reg2.IntVal ()));
            assert (m_data.IndexValid (m_reg2.IntVal () + 1));

            // m_reg2 points to array header (2 values)
            // First value is highest element (i.e number of elements + 1)
            // Second value is size of array element.
            // Array data immediately follows header
            if (m_reg.IntVal () >= 0 && m_reg.IntVal () < m_data.Data () [m_reg2.IntVal ()].IntVal ()) {
                assert (m_data.Data () [m_reg2.IntVal () + 1].IntVal () >= 0);
                m_reg.IntVal () = m_reg2.IntVal () + 2 + m_reg.IntVal () * m_data.Data () [m_reg2.IntVal () + 1].IntVal ();
                goto nextStep;
            }
            SetError (ErrBadArrayIndex);
            break;
        }
        SetError (ErrUnsetPointer);
        break;

    case OP_PUSH:

        // Push register to stack
        if (instruction->m_type == VTP_STRING)  m_stack.PushString (RegString ());
        else                                    m_stack.Push (Reg ());
        goto nextStep;

    case OP_POP:

        // Pop reg2 from stack
        if (instruction->m_type == VTP_STRING)  m_stack.PopString (Reg2String ());
        else                                    m_stack.Pop (Reg2 ());
        goto nextStep;

    case OP_SAVE: {

        // Save reg into [reg2]
        if (m_reg2.IntVal () > 0) {
            assert (m_data.IndexValid (m_reg2.IntVal ()));
            vmValue& dest = m_data.Data () [m_reg2.IntVal ()];
            switch (instruction->m_type) {
            case VTP_INT:
            case VTP_REAL:
                dest = m_reg;
                goto nextStep;
            case VTP_STRING:

                // Allocate string space if necessary
                if (dest.IntVal () == 0)
                    dest.IntVal () = m_strings.Alloc ();

                // Copy string value
                m_strings.Value (dest.IntVal ()) = m_regString;
                goto nextStep;
            }
            assert (false);
        }
        SetError (ErrUnsetPointer);
        break;
    }

    case OP_COPY: {

        // Copy data
        if (CopyData (  m_reg.IntVal (),
                        m_reg2.IntVal (),
                        m_typeSet.GetValType (instruction->m_value.IntVal ())))
            goto nextStep;
        else
            break;
    }
    case OP_DECLARE: {

        // Allocate variable.
        assert (m_variables.IndexValid (instruction->m_value.IntVal ()));
        vmVariable& var = m_variables.Variables () [instruction->m_value.IntVal ()];

        // Must not already be allocated
        if (var.Allocated ()) {
            SetError (ErrReDIMmedVariable);
            break;
        }

        // Pop and validate array dimensions sizes into type (if applicable)
        if (var.m_type.PhysicalPointerLevel () == 0
        && !PopArrayDimensions (var.m_type))
            break;

        // Validate type size
        if (!ValidateTypeSize (var.m_type))
            break;

        // Allocate variable
        var.Allocate (m_data, m_dataTypes);
        goto nextStep;
    }

    case OP_JUMP:

        // Jump
        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_code.size ());
        m_ip = instruction->m_value.IntVal ();
        goto step;

    case OP_JUMP_TRUE:

        // Jump if reg != 0
        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_code.size ());
        if (Reg ().IntVal () != 0) {
            m_ip = instruction->m_value.IntVal ();
            goto step;
        }
        goto nextStep;

    case OP_JUMP_FALSE:

        // Jump if reg == 0
        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_code.size ());
        if (Reg ().IntVal () == 0) {
            m_ip = instruction->m_value.IntVal ();
            goto step;
        }
        goto nextStep;

    case OP_OP_NEG:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal ()  = -Reg ().IntVal ();
        else if (instruction->m_type == VTP_REAL)   Reg ().RealVal () = -Reg ().RealVal ();
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_PLUS:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () += Reg2 ().IntVal ();
        else if (instruction->m_type == VTP_REAL)   Reg ().RealVal () += Reg2 ().RealVal ();
        else if (instruction->m_type == VTP_STRING) RegString () = Reg2String () + RegString ();
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_MINUS:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () - Reg ().IntVal ();
        else if (instruction->m_type == VTP_REAL)   Reg ().RealVal () = Reg2 ().RealVal () - Reg ().RealVal ();
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_TIMES:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () *= Reg2 ().IntVal ();
        else if (instruction->m_type == VTP_REAL)   Reg ().RealVal () *= Reg2 ().RealVal ();
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_DIV:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () / Reg ().IntVal ();
        else if (instruction->m_type == VTP_REAL)   Reg ().RealVal () = Reg2 ().RealVal () / Reg ().RealVal ();
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_MOD:
        if (instruction->m_type == VTP_INT) {
            vmInt i = Reg2 ().IntVal () % Reg ().IntVal ();
            if (i >= 0) Reg ().IntVal () = i;
            else        Reg ().IntVal () += i;
        }
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_NOT:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg ().IntVal () == 0 ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_EQUAL:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () == Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () == Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () == RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_NOT_EQUAL:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () != Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () != Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () != RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_GREATER:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () > Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () > Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () > RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_GREATER_EQUAL:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () >= Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () >= Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () >= RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_LESS:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () < Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () < Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () < RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_OP_LESS_EQUAL:
        if (instruction->m_type == VTP_INT)         Reg ().IntVal () = Reg2 ().IntVal () <= Reg ().IntVal () ? -1 : 0;
        else if (instruction->m_type == VTP_REAL)   Reg ().IntVal () = Reg2 ().RealVal () <= Reg ().RealVal () ? -1 : 0;
        else if (instruction->m_type == VTP_STRING) Reg ().IntVal () = Reg2String () <= RegString () ? -1 : 0;
        else {
            SetError (ErrBadOperator);
            break;
        }
        goto nextStep;

    case OP_CONV_INT_REAL:
        Reg ().RealVal () = Reg ().IntVal ();
        goto nextStep;

    case OP_CONV_INT_REAL2:
        Reg2 ().RealVal () = Reg2 ().IntVal ();
        goto nextStep;

    case OP_CONV_REAL_INT:
        Reg ().IntVal () = Reg ().RealVal ();
        goto nextStep;

    case OP_CONV_REAL_INT2:
        Reg2 ().IntVal () = Reg2 ().RealVal ();
        goto nextStep;

    case OP_CONV_INT_STRING:
        RegString () = IntToString (Reg ().IntVal ());
        goto nextStep;

    case OP_CONV_REAL_STRING:
        RegString () = RealToString (Reg ().RealVal ());
        goto nextStep;

    case OP_CONV_INT_STRING2:
        Reg2String () = IntToString (Reg2 ().IntVal ());
        goto nextStep;

    case OP_CONV_REAL_STRING2:
        Reg2String () = RealToString (Reg2 ().RealVal ());
        goto nextStep;

    case OP_OP_AND:
        Reg ().IntVal () = Reg ().IntVal () & Reg2 ().IntVal ();
        goto nextStep;

    case OP_OP_OR:
        Reg ().IntVal () = Reg ().IntVal () | Reg2 ().IntVal ();
        goto nextStep;

    case OP_OP_XOR:
        Reg ().IntVal () = Reg ().IntVal () ^ Reg2 ().IntVal ();
        goto nextStep;

    case OP_CALL_FUNC:

        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_functions.size ());

        // Call external function
        m_functions [instruction->m_value.IntVal ()] (*this);
        if (!Error ())
            goto nextStep;
        break;

    case OP_CALL_OPERATOR_FUNC:

        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_operatorFunctions.size ());

        // Call external function
        m_operatorFunctions [instruction->m_value.IntVal ()] (*this);
        if (!Error ())
            goto nextStep;
        break;

    case OP_TIMESHARE:
        m_ip++;                             // Move on to next instruction
        break;                              // And return

    case OP_FREE_TEMP:
        m_data.FreeTemp ();                 // Free temporary data
        goto nextStep;

    case OP_ALLOC: {

        // Extract type, and array dimensions
        vmValType type = m_typeSet.GetValType (instruction->m_value.IntVal ());
        if (!PopArrayDimensions (type))
            break;

        // Validate type size
        if (!ValidateTypeSize (type))
            break;

        // Allocate and initialise new data
        m_reg.IntVal() = m_data.Allocate (m_dataTypes.DataSize (type));
        m_data.InitData (m_reg.IntVal(), type, m_dataTypes);
        
        goto nextStep;
    }

    case OP_CALL: {

        // Call
        assert (instruction->m_value.IntVal () >= 0);
        assert (instruction->m_value.IntVal () < m_code.size ());

        // Check for stack overflow
        if (m_callStack.size () >= VM_MAXSTACKCALLS) {
            SetError (ErrStackOverflow);
            break;
        }

        // Push return address
        m_callStack.push_back (m_ip + 1);
//        vmValue temp ((int) m_ip + 1);
//        m_stack.Push (temp);
//        m_stack.Push (vmValue ((int) m_ip + 1));

        // Jump to subroutine
        m_ip = instruction->m_value.IntVal ();
        goto step;
    }
    case OP_RETURN:

        // Pop and validate return address
        if (m_callStack.empty ()) {
            SetError (ErrReturnWithoutGosub);
            break;
        }
        tempI = m_callStack [m_callStack.size () - 1];
        m_callStack.pop_back ();
        if (tempI >= m_code.size ()) {
            SetError (ErrStackError);
            break;
        }

        // Jump to return address
        m_ip = tempI;
        goto step;

    case OP_DATA_READ:

        // Read program data into register
        if (ReadProgramData ((vmBasicValType) instruction->m_type))
            goto nextStep;
        else
            break;

    case OP_DATA_RESET:

        m_programDataOffset = instruction->m_value.IntVal ();
        goto nextStep;

    case OP_RUN:
        Reset ();                           // Reset program
        break;                              // Timeshare break

    case OP_BREAKPT:
        m_paused = true;                    // Pause program
        break;                              // Timeshare break

    default:
        SetError (ErrInvalid);
    }
}

int TomVM::StoreStringConstant (std::string str) {
    int index = m_stringConstants.size ();
    m_stringConstants.push_back (str);
    return index;
}

void TomVM::BlockCopy      (int sourceIndex, int destIndex, int size) {

    // Block copy data
    assert (m_data.IndexValid (sourceIndex));
    assert (m_data.IndexValid (sourceIndex + size - 1));
    assert (m_data.IndexValid (destIndex));
    assert (m_data.IndexValid (destIndex + size - 1));
    for (int i = 0; i < size; i++)
        m_data.Data () [destIndex + i] = m_data.Data () [sourceIndex + i];
}

void TomVM::CopyStructure  (int sourceIndex, int destIndex, vmValType& type) {
    assert (m_dataTypes.TypeValid (type));
    assert (type.VirtualPointerLevel () == 0);
    assert (type.m_arrayLevel == 0);
    assert (type.m_basicType >= 0);

    // Find structure definition
    vmStructure &s = m_dataTypes.Structures () [type.m_basicType];

    // Copy fields in structure
    for (int i = 0; i < s.m_fieldCount; i++) {
        vmStructureField& f = m_dataTypes.Fields () [s.m_firstField + i];
        CopyField (sourceIndex + f.m_dataOffset, destIndex + f.m_dataOffset, f.m_type);
    }
}

void TomVM::CopyArray      (int sourceIndex, int destIndex, vmValType& type) {
    assert (m_dataTypes.TypeValid (type));
    assert (type.VirtualPointerLevel () == 0);
    assert (type.m_arrayLevel > 0);
    assert (m_data.IndexValid (sourceIndex));
    assert (m_data.IndexValid (destIndex));
    assert (m_data.Data () [sourceIndex].IntVal () == m_data.Data () [destIndex].IntVal ());             // Array sizes match
    assert (m_data.Data () [sourceIndex + 1].IntVal () == m_data.Data () [destIndex + 1].IntVal ());     // Element sizes match

    // Find element type and size
    vmValType elementType = type;
    elementType.m_arrayLevel--;
    int elementSize = m_data.Data () [sourceIndex + 1].IntVal ();

    // Copy elements
    for (int i = 0; i < m_data.Data () [sourceIndex].IntVal (); i++) {
        if (elementType.m_arrayLevel > 0)
            CopyArray ( sourceIndex + 2 + i * elementSize,
                        destIndex + 2 + i * elementSize,
                        elementType);
        else
            CopyField ( sourceIndex + 2 + i * elementSize,
                        destIndex + 2 + i * elementSize,
                        elementType);
    }
}

void TomVM::CopyField      (int sourceIndex, int destIndex, vmValType& type) {

    assert (m_dataTypes.TypeValid (type));

    // If type is basic string, copy string value
    if (type == VTP_STRING) {
        vmValue& dest = m_data.Data () [destIndex];

        // Allocate string space if necessary
        if (dest.IntVal () == 0)
            dest.IntVal () = m_strings.Alloc ();

        // Copy string value
        m_strings.Value (dest.IntVal ()) = m_strings.Value (m_data.Data () [sourceIndex].IntVal ());
    }

    // If type is basic, or pointer then just copy value
    else if (type.IsBasic () || type.VirtualPointerLevel () > 0)
        m_data.Data () [destIndex] = m_data.Data () [sourceIndex];

    // If contains no strings, can just block copy
    else if (!m_dataTypes.ContainsString (type))
        BlockCopy (sourceIndex, destIndex, m_dataTypes.DataSize (type));

    // Otherwise copy array or structure
    else if (type.m_arrayLevel > 0)
        CopyArray (sourceIndex, destIndex, type);
    else
        CopyStructure (sourceIndex, destIndex, type);
}

bool TomVM::CopyData       (int sourceIndex, int destIndex, vmValType type) {
    assert (m_dataTypes.TypeValid (type));
    assert (type.VirtualPointerLevel () == 0);

    // If a referenced type (which it should always be), convert to regular type.
    // (To facilitate comparisons against basic types such as VTP_STRING.)
    if (type.m_byRef)
        type.m_pointerLevel--;
    type.m_byRef = false;

    // Check pointers are valid
    if (!m_data.IndexValid (sourceIndex) || !m_data.IndexValid (destIndex)
    ||  sourceIndex == 0 || destIndex == 0) {
        SetError (ErrUnsetPointer);
        return false;
    }
    
    // Calculate element size
    int size = 1;
    if (type.m_basicType >= 0)
        size = m_dataTypes.Structures () [type.m_basicType].m_dataSize;

    // If the data types are arrays, then their sizes could be different.
    // If so, this is a run-time error.
    if (type.m_arrayLevel > 0) {
        int s = sourceIndex, d = destIndex;
        for (int i = 0; i < type.m_arrayLevel; i++) {
            assert (m_data.IndexValid (s));
            assert (m_data.IndexValid (s + 1));
            assert (m_data.IndexValid (d));
            assert (m_data.IndexValid (d + 1));
            if (m_data.Data () [s].IntVal () != m_data.Data () [d].IntVal ()) {
                SetError (ErrArraySizeMismatch);
                return false;
            }

            // Update data size
            size *= m_data.Data () [s].IntVal ();
            size += 2;

            // Point to first element in array
            s += 2; 
            d += 2;
        }
    }

    // If data type doesn't contain strings, can do a straight block copy
    if (!m_dataTypes.ContainsString (type))
        BlockCopy (sourceIndex, destIndex, size);
    else
        CopyField (sourceIndex, destIndex, type);

    return true;
}

bool TomVM::PopArrayDimensions (vmValType& type) {
    assert (m_dataTypes.TypeValid (type));
    assert (type.VirtualPointerLevel () == 0);

    // Pop and validate array indices from stack into type
    int i;
    vmValue v;
    for (i = 0; i < type.m_arrayLevel; i++) {
        m_stack.Pop (v);
        int size = v.IntVal () + 1;
        if (size < 1) {
            SetError (ErrZeroLengthArray);
            return false;
        }
        type.m_arrayDims [i] = size;
    }

    return true;
}

bool TomVM::ValidateTypeSize (vmValType& type) {

    // Enforce variable size limitations.
    // This prevents rogue programs from trying to allocate unrealistic amounts
    // of data.
    if (m_dataTypes.DataSizeBiggerThan (type, m_data.MaxDataSize ())) {
        SetError (ErrVariableTooBig);
        return false;
    }

    if (!m_data.RoomFor (m_dataTypes.DataSize (type))) {
        SetError (ErrOutOfMemory);
        return false;
    }

    return true;
}

void TomVM::PatchInBreakPt (unsigned int offset) {

    // Only patch if offset is valid and there is no breakpoint there already.
    // Note: Don't patch in breakpoint to last instruction of program as this is
    // always OP_END anyway.
    if (offset < m_code.size () - 1 && m_code [offset].m_opCode != OP_BREAKPT) {

        // Record previous op-code
        vmPatchedBreakPt bp;
        bp.m_offset         = offset;
        bp.m_replacedOpCode = (vmOpCode) m_code [offset].m_opCode;
        m_patchedBreakPts.push_back (bp);

        // Patch in breakpoint
        m_code [offset].m_opCode = OP_BREAKPT;
    }
}

void TomVM::InternalPatchOut () {

    // Patch out breakpoints and restore program to its no breakpoint state.
    for (   vmPatchedBreakPtList::iterator i = m_patchedBreakPts.begin ();
            i != m_patchedBreakPts.end ();
            i++)
        if ((*i).m_offset < m_code.size ())
            m_code [(*i).m_offset].m_opCode = (*i).m_replacedOpCode;
    m_patchedBreakPts.clear ();
    m_breakPtsPatched = false;
}

void TomVM::InternalPatchIn () {

    // Patch breakpoint instructions into the virtual machine code program.
    // This consists of swapping the virtual machine op-codes with OP_BREAKPT
    // codes.
    // We record the old op-code in the m_patchedBreakPts list, so we can restore
    // the program when we've finished.

    // User breakpts
    // Convert from line numbers to offsets
    unsigned int offset = 0;
    vmUserBreakPts::iterator i;
    for (   i  = m_userBreakPts.begin ();                                               // Loop through breakpoints
            i != m_userBreakPts.end ();
            i++) {
        while (offset < m_code.size () && m_code [offset].m_sourceLine < (*i).first)
            offset++;
        if (offset < m_code.size () && m_code [offset].m_sourceLine == (*i).first)      // Is breakpoint line valid?
            (*i).second.m_offset = offset;
        else
            (*i).second.m_offset = 0xffff;                                              // 0xffff means line invalid
    }

    // Patch in user breakpts
    for (   i  = m_userBreakPts.begin ();                                               // Loop through breakpoints
            i != m_userBreakPts.end ();
            i++)
        PatchInBreakPt ((*i).second.m_offset);

    // Patch in temp breakpts
    for (   vmTempBreakPtList::iterator j = m_tempBreakPts.begin ();
            j != m_tempBreakPts.end ();
            j++)
        PatchInBreakPt ((*j).m_offset);

    m_breakPtsPatched = true;
}

unsigned int TomVM::CalcBreakPtOffset (unsigned int line) {
    unsigned int offset = 0;
    while (offset < m_code.size () && m_code [offset].m_sourceLine < line)
        offset++;
    if (offset < m_code.size () && m_code [offset].m_sourceLine == line)        // Is breakpoint line valid?
        return offset;
    else
        return 0xffff;                                                          // 0xffff means line invalid
}

void TomVM::AddStepBreakPts (bool stepInto) {

    // Add temporary breakpoints to catch execution after stepping over the current line
    if (m_ip >= m_code.size ())
        return;
    PatchOut ();

    // Calculate op-code range that corresponds to the current line.
    unsigned int line, startOffset, endOffset;
    startOffset = m_ip;
    line        = m_code [startOffset].m_sourceLine;

    // Search for start of line
    while (startOffset > 0 && m_code [startOffset - 1].m_sourceLine == line)
        startOffset--;

    // Search for start of next line
    endOffset = m_ip + 1;
    while (endOffset < m_code.size () && m_code [endOffset].m_sourceLine == line)
        endOffset++;

    // Create breakpoint on next line
    m_tempBreakPts.push_back (TempBreakPt (endOffset));

    // Scan for jumps, and place breakpoints at destination addresses
    for (unsigned int i = startOffset; i < endOffset; i++) {
        unsigned int dest = 0xffff;
        switch (m_code [i].m_opCode) {
        case OP_CALL:
            if (!stepInto)                                  // If stepInto then fall through to JUMP handling.
                break;                                      // Otherwise break out, and no BP will be set.
        case OP_JUMP:
        case OP_JUMP_TRUE:
        case OP_JUMP_FALSE:
            dest = m_code [i].m_value.IntVal ();            // Destination jump address
            break;
        case OP_RETURN:
            if (!m_callStack.empty ())                      // Look at call stack and place breakpoint on return
                dest = m_callStack [m_callStack.size () - 1];
            break;
        }

        if (dest < m_code.size ()                           // Destination valid?
        &&  (dest < startOffset || dest >= endOffset))      // Destination outside line we are stepping over?
            m_tempBreakPts.push_back (TempBreakPt (dest));  // Add breakpoint
    }
}

bool TomVM::AddStepOutBreakPt () {

    // Call stack must contain at least 1 return
    if (!m_callStack.empty ()) {
        unsigned int returnAddr = m_callStack [m_callStack.size () - 1];        // Find return addr
        if (returnAddr < m_code.size ()) {                                      // Validate it
            m_tempBreakPts.push_back (TempBreakPt (returnAddr));                // Place breakpoint
            return true;
        }
    }
    return false;
}

vmState TomVM::GetState () {
    vmState s;

    // Instruction pointer
    s.ip = m_ip;

    // Registers
    s.reg           = m_reg;
    s.reg2          = m_reg2;
    s.regString     = m_regString;
    s.reg2String    = m_reg2String;

    // Stacks
    s.stackTop      = m_stack.Size ();
    s.callStackTop  = m_callStack.size ();

    // Top of program
    s.codeSize      = InstructionCount ();

    // Variable data
    m_data.GetState (s.dataSize, s.tempDataStart);

    // Error state
    s.error         = Error ();
    if (s.error)    s.errorString   = GetError ();
    else            s.errorString   = "";

    // Other state
    s.paused        = m_paused;
    
    return s;
}

void TomVM::SetState (vmState& state) {

    // Instruction pointer
    m_ip                = state.ip;

    // Registers
    m_reg               = state.reg;
    m_reg2              = state.reg2;
    m_regString         = state.regString;
    m_reg2String        = state.reg2String;

    // Stacks
    if (state.stackTop < m_stack.Size ())
        m_stack.Resize (state.stackTop);
    if (state.callStackTop < m_callStack.size ())
        m_callStack.resize (state.callStackTop);

    // Top of program
    if (state.codeSize < m_code.size ())
        m_code.resize (state.codeSize);

    // Variable data
    m_data.SetState (state.dataSize, state.tempDataStart);

    // Error state
    if (state.error)    SetError (state.errorString);
    else                ClearError ();

    // Other state
    m_paused = state.paused;
}

std::string TomVM::BasicValToString (vmValue val, vmBasicValType type, bool constant) {
    switch (type) {
    case VTP_INT:       return IntToString (val.IntVal ());
    case VTP_REAL:      return RealToString (val.RealVal ());
    case VTP_STRING:    
        if (constant) {
            if (val.IntVal () >= 0 && val.IntVal () < m_stringConstants.size ())
                return "\"" + m_stringConstants [val.IntVal ()] + "\"";
            else
                return "???";
        }
        else
            return m_strings.IndexValid (val.IntVal ()) ? "\"" + m_strings.Value (val.IntVal ()) + "\"" : (std::string) "???";
    }
    return "???";
}

inline std::string TrimToLength (std::string str, int& length) {
    if (str.length () > length) {
        length = 0;
        return str.substr(0, length);
    }
    else {
        length -= str.length ();
        return str;
    }
}

void TomVM::Deref (vmValue& val, vmValType& type) {
    type.m_pointerLevel--;
    if (type.m_pointerLevel == 0 && !type.IsBasic ()) {

        // Can't follow pointer, as type is not basic (and therefore cannot be loaded into a register)
        // Use value by reference instead
        type.m_pointerLevel = 1;
        type.m_byRef = true;
    }
    else {

        // Follow pointer
        assert (m_data.IndexValid (val.IntVal ()));
        val = m_data.Data () [val.IntVal ()];
    }
}

std::string TomVM::ValToString (vmValue val, vmValType type, int& maxChars) {
    assert (m_dataTypes.TypeValid (type));
    assert (type.PhysicalPointerLevel () > 0 || type.IsBasic ());

    if (maxChars <= 0)
        return "";

    // Basic types
    if (type.IsBasic ())
        return TrimToLength (BasicValToString (val, type.m_basicType, false), maxChars);

    // Pointer types
    if (type.VirtualPointerLevel () > 0) {

        // Follow pointer down
        if (val.IntVal () == 0)
            return TrimToLength ("[UNSET POINTER]", maxChars);
        Deref (val, type);
        return TrimToLength ("&", maxChars) + ValToString (val, type, maxChars);
    }

    // Type is not basic, or a pointer. Must be a value by reference. Either a structure or an array
    assert (type.m_pointerLevel == 1);
    assert (type.m_byRef);
    int dataIndex = val.IntVal ();              // Points to data
    if (dataIndex == 0)
        return TrimToLength ("[UNSET]", maxChars);
    std::string result = "";

    // Arrays
    if (type.m_arrayLevel > 0) {
        assert (m_data.IndexValid (dataIndex));
        assert (m_data.IndexValid (dataIndex + 1));

        // Read array header
        int elements    = m_data.Data () [dataIndex].IntVal ();
        int elementSize = m_data.Data () [dataIndex + 1].IntVal ();
        int arrayStart  = dataIndex + 2;

        // Enumerate elements
        std::string result = TrimToLength ("{", maxChars);
        for (int i = 0; i < elements && maxChars > 0; i++) {
            vmValue element = vmValue (arrayStart + i * elementSize);   // Address of element
            vmValType elementType = type;       // Element type.
            elementType.m_arrayLevel--;         // Less one array level.
            elementType.m_pointerLevel = 1;     // Currently have a pointer
            elementType.m_byRef = false;

            // Deref to reach data
            Deref (element, elementType);

            // Describe element
            result += ValToString (element, elementType, maxChars);
            if (i < elements - 1)
                result += TrimToLength (", ", maxChars);
        }
        result += TrimToLength ("}", maxChars);
        return result;
    }

    // Structures
    if (type.m_basicType >= 0) {
        std::string result = TrimToLength ("{", maxChars);
        vmStructure& structure = m_dataTypes.Structures () [type.m_basicType];
        for (int i = 0; i < structure.m_fieldCount && maxChars > 0; i++) {
            vmStructureField& field = m_dataTypes.Fields () [structure.m_firstField + i];
            vmValue fieldVal    = vmValue (dataIndex + field.m_dataOffset);
            vmValType fieldType = field.m_type;
            fieldType.m_pointerLevel++;
            Deref (fieldVal, fieldType);
            result += TrimToLength (field.m_name + "=", maxChars) + ValToString (fieldVal, fieldType, maxChars);
            if (i < structure.m_fieldCount - 1)
                result += TrimToLength (", ", maxChars);
        }
        result += TrimToLength ("}", maxChars);
        return result;
    }

    return "???";
}

std::string TomVM::VarToString (vmVariable& v, int maxChars) {
    vmValue     val = vmValue ((int) v.m_dataIndex);
    vmValType   type = v.m_type;
    type.m_pointerLevel++;
    Deref (val, type);
    return ValToString (val, type, maxChars);
}

bool TomVM::ReadProgramData    (vmBasicValType type) {

    // Read program data into register.

    // Check for out-of-data.
    if (m_programDataOffset >= m_programData.size ()) {
        SetError (ErrOutOfData);
        return false;
    }

    // Find program data
    vmProgramDataElement e = m_programData [m_programDataOffset++];

    // Convert to requested type
    switch (type) {
    case VTP_STRING:

        // Convert type to int.
        switch (e.Type ()) {
        case VTP_STRING:
            assert (e.Value ().IntVal () >= 0);
            assert (e.Value ().IntVal () < m_stringConstants.size ());
            RegString () = m_stringConstants [e.Value ().IntVal ()];
            return true;
        case VTP_INT:
            RegString () = IntToString (e.Value ().IntVal ());
            return true;
        case VTP_REAL:
            RegString () = RealToString (e.Value ().RealVal ());
            return true;
        }
        break;

    case VTP_INT:
        switch (e.Type ()) {
        case VTP_STRING:
            SetError (ErrDataIsString);
            return false;
        case VTP_INT:
            Reg ().IntVal () = e.Value ().IntVal ();
            return true;
        case VTP_REAL:
            Reg ().IntVal () = e.Value ().RealVal ();
            return true;
        }
        break;

    case VTP_REAL:
        switch (e.Type ()) {
        case VTP_STRING:
            SetError (ErrDataIsString);
            return false;
        case VTP_INT:
            Reg ().RealVal () = e.Value ().IntVal ();
            return true;
        case VTP_REAL:
            Reg ().RealVal () = e.Value ().RealVal ();
            return true;
        }
        break;
    } 
    assert (false);
    return false;
}

#ifdef VM_STATE_STREAMING
void TomVM::StreamOut (std::ostream& stream) {
    int i;

    // Stream header
    WriteString (stream, streamHeader);
    WriteLong (stream, streamVersion);

    // Variables
    m_variables.StreamOut (stream);         // Note: m_variables automatically streams out m_dataTypes

    // String constants
    WriteLong (stream, m_stringConstants.size ());
    for (i = 0; i < m_stringConstants.size (); i++)
        WriteString (stream, m_stringConstants [i]);
    
    // Data type lookup table
    m_typeSet.StreamOut (stream);

    // Program code
    WriteLong (stream, m_code.size ());
    for (i = 0; i < m_code.size (); i++)
        m_code [i].StreamOut (stream);

    // Program data (for "DATA" statements)
    WriteLong (stream, m_programData.size ());
    for (i = 0; i < m_programData.size (); i++)
        m_programData [i].StreamOut (stream);
}

void TomVM::StreamIn (std::istream& stream) {
    int i, count;

    // Clear existing program
    New ();

    // Read in stream header, and validate
	int length = ReadLong (stream);
	if (length != streamHeader.length ()) {
		SetError ("Error in Virtual Machine stream: " + IntToString (length));
		return;
	}
    char buf [16];
    stream.read (buf, 15);
	buf [15] = 0;
    if ((std::string) buf != streamHeader) {
        SetError ("Error in Virtual Machine stream: " + (std::string) buf);
        return;
    }

    // Read in stream version and validate
    int version = ReadLong (stream);
    if (version != streamVersion) {
        SetError ((std::string) "Wrong version in Virtual Machine stream. Stream version: " + IntToString (version) + ", expected version: " + IntToString (streamVersion));
        return;
    }

    // Variables
    m_variables.StreamIn (stream);

    // String constants
    count = ReadLong (stream);
    m_stringConstants.resize (count);
    for (i = 0; i < count; i++)
        m_stringConstants [i] = ReadString (stream);

    // Data type lookup table
    m_typeSet.StreamIn (stream);

    // Program code
    count = ReadLong (stream);
    m_code.resize (count);
    for (i = 0; i < count; i++)
        m_code [i].StreamIn (stream);

    // Program data (for "DATA" statements)
    count = ReadLong (stream);
    m_programData.resize (count);
    for (i = 0; i < count; i++)
        m_programData [i].StreamIn (stream);
}
#endif
