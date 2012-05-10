//---------------------------------------------------------------------------
/*  Created 5-Sep-2003: Thomas Mulgrew                                              

    Used to compile source code in BASIC language to TomVM Op codes.
*/

#pragma hdrstop
#include "TomComp.h"

//---------------------------------------------------------------------------

#ifndef _MSC_VER
#pragma package(smart_init)
#endif

////////////////////////////////////////////////////////////////////////////////
// TomBasicCompiler

TomBasicCompiler::TomBasicCompiler (TomVM& vm, bool caseSensitive)
         : m_vm (vm), m_caseSensitive (caseSensitive), m_syntax(LS_BASIC4GL) {
    ClearState ();

    // Setup operators
    // Note: From experimentation it appears QBasic binds "xor" looser than
    // "and" and "or". So for compatibility, we will too..
    m_binaryOperators   ["xor"] = compOperator (OT_BOOLOPERATOR, OP_OP_XOR, 2,               10);
    m_binaryOperators   ["or"]  = compOperator (OT_BOOLOPERATOR, OP_OP_OR, 2,                11);
    m_binaryOperators   ["and"] = compOperator (OT_BOOLOPERATOR, OP_OP_AND, 2,               12);
    m_unaryOperators    ["not"] = compOperator (OT_BOOLOPERATOR, OP_OP_NOT, 1,               20);
    m_binaryOperators   ["="]   = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_EQUAL, 2,             30);
    m_binaryOperators   ["<>"]  = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_NOT_EQUAL, 2,         30);
    m_binaryOperators   [">"]   = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_GREATER, 2,           30);
    m_binaryOperators   [">="]  = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_GREATER_EQUAL, 2,     30);
    m_binaryOperators   ["<"]   = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_LESS, 2,              30);
    m_binaryOperators   ["<="]  = compOperator (OT_RETURNBOOLOPERATOR, OP_OP_LESS_EQUAL, 2,        30);
    m_binaryOperators   ["+"]   = compOperator (OT_OPERATOR, OP_OP_PLUS, 2,              40);
    m_binaryOperators   ["-"]   = compOperator (OT_OPERATOR, OP_OP_MINUS, 2,             40);
    m_binaryOperators   ["*"]   = compOperator (OT_OPERATOR, OP_OP_TIMES, 2,             41);
    m_binaryOperators   ["/"]   = compOperator (OT_OPERATOR, OP_OP_DIV, 2,               42);
    m_binaryOperators   ["%"]   = compOperator (OT_OPERATOR, OP_OP_MOD, 2,               43);
    m_unaryOperators    ["-"]   = compOperator (OT_OPERATOR, OP_OP_NEG, 1,               50);


    // Setup reserved words
    m_reservedWords.insert ("dim");
    m_reservedWords.insert ("goto");
    m_reservedWords.insert ("if");
    m_reservedWords.insert ("then");
    m_reservedWords.insert ("elseif");
    m_reservedWords.insert ("else");
    m_reservedWords.insert ("endif");
    m_reservedWords.insert ("end");
    m_reservedWords.insert ("gosub");
    m_reservedWords.insert ("return");
    m_reservedWords.insert ("for");
    m_reservedWords.insert ("to");
    m_reservedWords.insert ("step");
    m_reservedWords.insert ("next");
    m_reservedWords.insert ("while");
    m_reservedWords.insert ("wend");
    m_reservedWords.insert ("run");
    m_reservedWords.insert ("struc");
    m_reservedWords.insert ("endstruc");
    m_reservedWords.insert ("const");
    m_reservedWords.insert ("alloc");
    m_reservedWords.insert ("null");
    m_reservedWords.insert ("data");
    m_reservedWords.insert ("read");
    m_reservedWords.insert ("reset");
    m_reservedWords.insert ("type");        // QBasic/Freebasic compatibility
    m_reservedWords.insert ("as");          //
    m_reservedWords.insert ("integer");     //
    m_reservedWords.insert ("single");      //
    m_reservedWords.insert ("double");      //
    m_reservedWords.insert ("string");      //
    m_reservedWords.insert ("language");    // Language syntax
    m_reservedWords.insert ("traditional");
    m_reservedWords.insert ("basic4gl");
    m_reservedWords.insert ("traditional_print");
    m_reservedWords.insert ("input");
    m_reservedWords.insert ("do");
    m_reservedWords.insert ("loop");
    m_reservedWords.insert ("until");
}

void TomBasicCompiler::ClearState () {
    m_regType   = VTP_INT;
    m_reg2Type  = VTP_INT;
    m_freeTempData = false;
    m_operandStack.clear ();
    m_operatorStack.clear ();
    m_jumps.clear ();
    m_resets.clear ();
    m_flowControl.clear ();
    m_syntax = LS_BASIC4GL;
}

bool TomBasicCompiler::Compile () {

    // Clear existing program
    m_vm.New ();
    m_lastLine = 0;
    m_lastCol  = 0;

    // Compile source code
    ClearState ();
    m_parser.Reset ();
    m_programConstants.clear ();
    m_labels.clear ();
    m_labelIndex.clear ();
    InternalCompile ();

    return !Error ();
}

bool TomBasicCompiler::CheckParser () {
    // Check parser for error
    // Copy error state (if any)
    if (m_parser.Error ()) {
        SetError ((std::string) "Parse error: " + m_parser.GetError());
        m_parser.ClearError ();
        return false;
    }
    return true;
}

bool TomBasicCompiler::GetToken (bool skipEOL, bool dataMode) {

    // Read a token
    m_token = m_parser.NextToken (skipEOL, dataMode);
    if (!CheckParser ())
        return false;

    // Text token processing
    if (m_token.m_type == CTT_TEXT) {

        // Apply case sensitivity
        if (!m_caseSensitive)
            m_token.m_text = LowerCase (m_token.m_text);

        // Match against reserved keywords
        compStringSet::iterator si = m_reservedWords.find (m_token.m_text);
        if (si != m_reservedWords.end ())
            m_token.m_type = CTT_KEYWORD;

        // Match against external functions
        else if (IsFunction (m_token.m_text))
            m_token.m_type = CTT_FUNCTION;

        else {

            // Match against constants

            // Try permanent constants first
            bool isConstant;
            compConstantMap::iterator smi = m_constants.find (m_token.m_text);
            isConstant = smi != m_constants.end ();

            // Otherwise try program constants
            if (!isConstant) {
                smi = m_programConstants.find (m_token.m_text);
                isConstant = smi != m_programConstants.end ();
            }

            if (isConstant) {
                m_token.m_type      = CTT_CONSTANT;             // Replace token with constant

                // Replace text with text value of constant
                switch ((*smi).second.m_valType) {
                case VTP_INT:
                    m_token.m_text = IntToString ((*smi).second.m_intVal);
                    break;
                case VTP_REAL:
                    m_token.m_text = RealToString ((*smi).second.m_realVal);
                    break;
                case VTP_STRING:
                    m_token.m_text = (*smi).second.m_stringVal;
                    break;
                }
                m_token.m_valType   = (*smi).second.m_valType;
            }
        }
    }
    else if (m_token.m_type == CTT_CONSTANT && m_token.m_valType == VTP_STRING) {

        // 19-Jul-2003
        // Prefix string text constants with "S". This prevents them matching
        // any recognised keywords (which are stored in lower case).
        // (This is basically a work around to existing code which doesn't
        // check the token type if it matches a reserved keyword).
        m_token.m_text = (std::string) "S" + m_token.m_text;
    }
    return true;
}

bool TomBasicCompiler::NeedAutoEndif() {

    // Look at the top-most control structure
    if (m_flowControl.empty())
        return false;

    compFlowControl top = FlowControlTOS ();

    // Auto endif required if top-most flow control is a non-block "if" or "else"
    return (top.m_type == FCT_IF || top.m_type == FCT_ELSE) && !top.m_blockIf;
}

void TomBasicCompiler::InternalCompile () {

    // Clear error state
    ClearError ();
    m_parser.ClearError ();

    // Read initial token
    if (!GetToken (true))
        return;

    // Compile code
    while (!m_parser.Eof () && CompileInstruction ())
        ;

    // Terminate program
    AddInstruction (OP_END, VTP_INT, vmValue ());

    if (!Error ()) {
        // Link up gotos
        std::vector<compJump>::iterator i;
        for (i = m_jumps.begin (); i != m_jumps.end (); i++) {

            // Find instruction
            assert ((*i).m_jumpInstruction < m_vm.InstructionCount ());
            vmInstruction& instr = m_vm.Instruction ((*i).m_jumpInstruction);

            // Point token to goto instruction, so that it will be displayed
            // if there is an error.
            m_token.m_line = instr.m_sourceLine;
            m_token.m_col  = instr.m_sourceChar;

            // Label must exist
            if (!LabelExists ((*i).m_labelName)) {
                SetError ((std::string) "Label: " + (*i).m_labelName + " does not exist");
                return;
            }

            // Patch in offset
            instr.m_value.IntVal () = Label ((*i).m_labelName).m_offset;
        }

        // Link up resets
        for (i = m_resets.begin (); i != m_resets.end (); i++) {

            // Find instruction
            assert ((*i).m_jumpInstruction < m_vm.InstructionCount ());
            vmInstruction& instr = m_vm.Instruction ((*i).m_jumpInstruction);

            // Point token to reset instruction, so that it will be displayed
            // if there is an error.
            m_token.m_line = instr.m_sourceLine;
            m_token.m_col  = instr.m_sourceChar;

            // Label must exist
            if (!LabelExists ((*i).m_labelName)) {
                SetError ((std::string) "Label: " + (*i).m_labelName + " does not exist");
                return;
            }

            // Patch in data offset
            instr.m_value.IntVal () = Label ((*i).m_labelName).m_programDataOffset;
        }

        // Check for open flow control structures
        if (!m_flowControl.empty ()) {

            // Find topmost structure
            compFlowControl top = FlowControlTOS ();

            // Point parser to it
            m_parser.SetPos (top.m_sourceLine, top.m_sourceCol);

            // Set error message
            switch (FlowControlTOS ().m_type) {
            case FCT_IF:    SetError ("'if' without 'endif'");          break;
            case FCT_ELSE:  SetError ("'else' without 'endif'");        break;
            case FCT_FOR:   SetError ("'for' without 'next'");          break;
            case FCT_WHILE: SetError ("'while' without 'wend'");        break;
            case FCT_DO_PRE:
            case FCT_DO_POST:
                            SetError ("'do' without 'loop'");           break;
            default:        SetError ("Open flow control structure");   break;
            }

            return;
        }
    }
}

void TomBasicCompiler::AddInstruction (vmOpCode opCode, vmBasicValType type, const vmValue& val) {

    // Add instruction, and include source code position audit
    unsigned int line = m_token.m_line, col = m_token.m_col;

    // Prevent line and col going backwards. (Debugging tools rely on source-
    // offsets never decreasing as source code is traversed.)
    if (line < m_lastLine || (line == m_lastLine && col < m_lastCol)) {
        line = m_lastLine;
        col  = m_lastCol;
    }
    m_vm.AddInstruction (vmInstruction (opCode, type, val, line, col));
    m_lastLine = line;
    m_lastCol  = col;
}

bool TomBasicCompiler::CompileInstruction () {
    m_needColon = true;                     // Instructions by default must be separated by colons

    // Is it a label?
    compToken nextToken = m_parser.PeekToken(false);
    if (!CheckParser ())
        return false;
    if (    m_token.m_newLine && m_token.m_type == CTT_TEXT
        &&  nextToken.m_text == ":") {

        // Label declaration
        // Must not already exist
        if (LabelExists (m_token.m_text)) {
            SetError ("Duplicate label name: " + m_token.m_text);
            return false;
        }

        // Create new label
        AddLabel (m_token.m_text, compLabel (m_vm.InstructionCount (), m_vm.ProgramData ().size ()));

        // Skip label
        if (!GetToken ())
            return false;
    }
    // Determine the type of instruction, based on the current token
    else if (m_token.m_text == "struc" || m_token.m_text == "type") {
        if (!CompileStructure ())
            return false;
    }
    else if (m_token.m_text == "dim") {
        if (!CompileDim (false))
            return false;
    }
    else if (m_token.m_text == "goto") {
        if (!GetToken ())
            return false;
        if (!CompileGoto (OP_JUMP))
            return false;
    }
    else if (m_token.m_text == "gosub") {
        if (!GetToken())
            return false;
        if (!CompileGoto (OP_CALL))
            return false;
    }
    else if (m_token.m_text == "return") {
        if (!GetToken ())
            return false;
        AddInstruction (OP_RETURN, VTP_INT, vmValue ());
    }
    else if (m_token.m_text == "if") {
        if (!CompileIf (false))
            return false;
    }
    else if (m_token.m_text == "elseif") {
        if (!(CompileElse (true) && CompileIf (true)))
            return false;
    }
    else if (m_token.m_text == "else") {
        if (!CompileElse (false))
            return false;
    }
    else if (m_token.m_text == "endif") {
        if (!CompileEndIf (false))
            return false;
    }
    else if (m_token.m_text == "for") {
        if (!CompileFor ())
            return false;
    }
    else if (m_token.m_text == "next") {
        if (!CompileNext ())
            return false;
    }
    else if (m_token.m_text == "while") {
        if (!CompileWhile ())
            return false;
    }
    else if (m_token.m_text == "wend") {
        if (!CompileWend ())
            return false;
    }
    else if (m_token.m_text == "do") {
        if (!CompileDo())
            return false;
    }
    else if (m_token.m_text == "loop") {
        if (!CompileLoop())
            return false;
    }
    else if (m_token.m_text == "end") {
        if (!GetToken ())
            return false;

        // Special case! "End" immediately followed by "if" is syntactically equivalent to "endif"
        if (m_token.m_text == "if") {
            if (!CompileEndIf (false))
                return false;
        }
        else
            // Otherwise is "End" program instruction
            AddInstruction (OP_END, VTP_INT, vmValue ());
    }
    else if (m_token.m_text == "run") {
        if (!GetToken ())
            return false;
        AddInstruction (OP_RUN, VTP_INT, vmValue ());
    }
    else if (m_token.m_text == "const") {
        if (!CompileConstant ())
            return false;
    }
    else if (m_token.m_text == "alloc") {
        if (!CompileAlloc ())
            return false;
    }
    else if (m_token.m_text == "data") {
        if (!CompileData ())
            return false;
    }
    else if (m_token.m_text == "read") {
        if (!CompileDataRead ())
            return false;
    }
    else if (m_token.m_text == "reset") {
        if (!CompileDataReset ())
            return false;
    }
    else if (m_token.m_text == "print") {
        if (!CompilePrint(false))
            return false;
    }
    else if (m_token.m_text == "printr") {
        if (!CompilePrint(true))
            return false;
    }
    else if (m_token.m_text == "input") {
        if (!CompileInput ())
            return false;
    }
    else if (m_token.m_text == "language") {
        if (!CompileLanguage())
            return false;
    }
    else if (m_token.m_type == CTT_FUNCTION) {
        if (!CompileFunction ())
            return false;
    }
    else if (!CompileAssignment ())
        return false;

    // Free any temporary data (if necessary)
    if (!CompileFreeTempData ())
        return false;

    // Skip separators (:, EOL)
    if (!SkipSeparators ())
        return false;

    return true;
}

bool TomBasicCompiler::AtSeparatorOrSpecial () {

    // Note: Endif is special, as it doesn't require a preceding colon to separate
    // from other instructions on the same line.
    // e.g.
    //      if CONDITION then INSTRUCTION(S) endif
    // Likewise else
    //      if CONDITION then INSTRUCTION(S) else INSTRUCTION(S) endif
    //
    // (unlike while, which would have to be written as:
    //      while CONDITION: INSTRUCTION(S): wend
    // )
    return AtSeparator () || m_token.m_text == "endif" || m_token.m_text == "else" || m_token.m_text == "elseif";
}

bool TomBasicCompiler::AtSeparator () {
    return      m_token.m_type == CTT_EOL
            ||  m_token.m_type == CTT_EOF
            ||  m_token.m_text == ":";
}

bool TomBasicCompiler::SkipSeparators () {

    // Expect separator. Either as an EOL or EOF or ':'
    if (    m_needColon
        &&  !AtSeparatorOrSpecial ()) {
        SetError ("Expected ':'");
        return false;
    }

    // Skip separators
    while (     m_token.m_type == CTT_EOL
            ||  m_token.m_type == CTT_EOF
            ||  m_token.m_text == ":") {

        // If we reach the end of the line, insert any necessary implicit "endifs"
        if (m_token.m_type == CTT_EOL || m_token.m_type == CTT_EOF) {

            // Generate any automatic endifs for the previous line
            while (NeedAutoEndif())
                if (!CompileEndIf(true))
                    return false;

            // Convert any remaining flow control structures to block
            for (std::vector<compFlowControl>::iterator i = m_flowControl.begin();
                i != m_flowControl.end();
                i++)
                i->m_blockIf = true;
        }
        
        if (!GetToken (true))
            return false;
    }

    return true;
}

bool TomBasicCompiler::CompileStructure () {

    // Skip STRUC
    std::string keyword = m_token.m_text;               // Record whether "struc" or "type" was used to declare type  
    if (!GetToken ())
        return false;

    // Expect structure name
    if (m_token.m_type != CTT_TEXT) {
        SetError ("Expected structure name");
        return false;
    }
    std::string name = m_token.m_text;
    if (!CheckName (name))
        return false;
    if (m_vm.DataTypes ().StrucStored (name)) {         // Must be unused
        SetError ((std::string) "'" + name + "' has already been used as a structure name");
        return false;
    }
    if (!GetToken ())                                   // Skip structure name
        return false;

    if (!SkipSeparators ())                             // Require : or new line
        return false;

    // Create structure
    m_vm.DataTypes ().NewStruc (name);

    // Expect at least one field
    if (m_token.m_text == "endstruc" || m_token.m_text == "end") {
        SetError ("Expected DIM or field name");
        return false;
    }

    // Populate with fields
    while (m_token.m_text != "endstruc" && m_token.m_text != "end") {

        // dim statement is optional
/*        if (m_token.m_text != "dim") {
            SetError ("Expected 'dim' or 'endstruc'");
            return false;
        }*/
        if (!CompileDim (true))
            return false;

        if (!SkipSeparators ())
            return false;
    }

    if (m_token.m_text == "end") {

        // Skip END
        if (!GetToken ())
            return false;

        // Check END keyword matches declaration keyword
        if (m_token.m_text != keyword) {
            SetError ("Expected '" + keyword + "'");
            return false;
        }

        // Skip STRUC/TYPE
        if (!GetToken ())
            return false;
    }
    else {

        // Make sure "struc" was declared
        if (keyword != "struc") {
            SetError ("Expected 'END '" + keyword + "'");
            return false;
        }

        // Skip ENDSTRUC
        if (!GetToken ())
            return false;
    }
    return true;
}

bool TomBasicCompiler::CompileDim (bool forStruc) {

    // Skip optional DIM
    if (m_token.m_text == "dim")
        if (!GetToken ())
            return false;

    // Expect at least one field in dim
    if (AtSeparatorOrSpecial ()) {
        SetError ("Expected variable declaration");
        return false;
    }

    // Parse fields in dim
    bool needComma = false;             // First element doesn't need a comma
    while (!AtSeparatorOrSpecial ()) {

        // Handle commas
        if (needComma) {
            if (m_token.m_text != ",") {
                SetError ("Expected ','");
                return false;
            }
            if (!GetToken ())
                return false;
        }
        needComma = true;               // Remaining elements do need commas

        // Extract field type
        std::string name;
        vmValType type;
        if (!CompileDimField (name, type, forStruc))
            return false;
        if (!CheckName (name))
            return false;

        if (forStruc) {

            // Validate field name and type
            vmStructure& struc = m_vm.DataTypes ().CurrentStruc ();
            if (m_vm.DataTypes ().FieldStored (struc, name)) {
                SetError ((std::string) "Field '" + name + "' has already been DIMmed in structure '" + struc.m_name + "'");
                return false;
            }
            if (type.m_pointerLevel == 0 && type.m_basicType == m_vm.DataTypes ().GetStruc (struc.m_name)) {
                SetError ("Structure cannot contain an element of its own type");
                return false;
            }

            // Add field to structure
            m_vm.DataTypes ().NewField (name, type);
        }
        else {

            // Regular DIM.
            // Check if variable has already been DIMmed. (This is allowed, but
            // only if DIMed to the same type.)
            int varIndex = m_vm.Variables ().GetVar (name);
            if (varIndex >= 0) {
                if (!(m_vm.Variables ().Variables () [varIndex].m_type == type)) {
                    SetError ((std::string) "Variable '" + name + "' has already been allocated as a different type.");
                    return false;
                }

                // Variable already created (earlier), so fall through to
                // allocation code generation.
            }
            else
                // Create new variable
                varIndex = m_vm.Variables ().NewVar (name, type);

            // Generate code to allocate variable data
            // Note:    Opcode contains the index of the variable. Variable type
            //          and size data is stored in the variable entry.
            AddInstruction (OP_DECLARE, VTP_INT, vmValue (varIndex));

            // If this was an array and not a pointer, then its array indices
            // will have been pushed to the stack.
            // The DECLARE operator automatically removes them however
            if (type.PhysicalPointerLevel() == 0)
                for (int i = 0; i < type.m_arrayLevel; i++)
                    m_operandStack.pop_back ();
        }
    }
    return true;
}

bool TomBasicCompiler::CompileDimField (std::string& name, vmValType& type, bool forStruc) {

    type = VTP_UNDEFINED;
    name = "";

    // Look for structure type
    if (m_token.m_type == CTT_TEXT) {
        int i = m_vm.DataTypes ().GetStruc (m_token.m_text);
        if (i >= 0) {
            type.m_basicType = (vmBasicValType) i;
            if (!GetToken ())           // Skip token type keyword
                return false;
        }
    }

    // Look for preceeding & (indicates pointer)
    if (m_token.m_text == "&") {
        type.m_pointerLevel++;
        if (!GetToken ())
            return false;
    }

    // Look for variable name
    if (m_token.m_type != CTT_TEXT) {
        SetError ("Expected variable name");
        return false;
    }
    name = m_token.m_text;
    if (!GetToken ())
        return false;

    // Determine variable type
    assert (name.length () > 0);
    char last = name [name.length () - 1];
    if (type.m_basicType == VTP_UNDEFINED) {
        type.m_basicType = VTP_INT;
        if (last == '$')        type.m_basicType = VTP_STRING;
        else if (last == '#')   type.m_basicType = VTP_REAL;
        else if (last == '%')   type.m_basicType = VTP_INT;
    }
    else {
        if (last == '$' || last == '#' || last == '%') {
            SetError ((std::string) "\"" + name + "\" is a structure variable, and cannot end with #, $ or %");
            return false;
        }
    }

    // Look for array dimensions
    if (m_token.m_text == "(") {

        bool foundComma = false;
        while (m_token.m_text == "(" || foundComma) {

            // Room for one more dimension?
            if (type.m_arrayLevel >= VM_MAXDIMENSIONS) {
                SetError ((std::string) "Arrays cannot have more than " + IntToString (VM_MAXDIMENSIONS) + " dimensions.");
                return false;
            }
            if (!GetToken ())           // Skip "("
                return false;

            // Validate dimensions.
            // Firstly, pointers don't have dimensions declared with them.
            if (type.m_pointerLevel > 0) {
                if (m_token.m_text != ")") {
                    SetError ("Use '()' to declare a pointer to an array");
                    return false;
                }
                type.m_arrayLevel++;
            }
            // Structure field types must have constant array size that we can
            // evaluate at compile time (i.e right now).
            else if (forStruc) {

                if (m_token.m_type != CTT_CONSTANT || m_token.m_valType != VTP_INT) {
                    SetError ("Expected array size as integer constant");
                    return false;
                }

                // Store array dimension.
                // Note: Add one, because DIM array(N) actually allocates N+1 entries
                // (0,1,...,N inclusive).
                type << (StringToInt (m_token.m_text) + 1);
                if (!GetToken ())
                    return false;
            }
            // Regular DIMmed array dimensions are sized at run time.
            // Here we generate code to calculate the dimension size and push it to
            // stack.
            else {
                if (!(CompileExpression () && CompileConvert (VTP_INT) && CompilePush ()))
                    return false;
                type.m_arrayLevel++;
            }

            // Expect closing ')', or a separating comma
            foundComma = false;
            if (m_token.m_text == ")") {
                if (!GetToken ())
                    return false;
            }
            else if (m_token.m_text == ",")
                foundComma = true;
            else {
                SetError ("Expected ')' or ','");
                return false;
            }
        }
    }

    // "as" keyword (QBasic/FreeBasic compatibility)
    if (m_token.m_text == "as") {
        if (type.m_basicType != VTP_INT || last == '%') {
            SetError ("'" + name + "'s type has already been defined. Cannot use 'as' here.");
            return false;
        }

        // Skip "as"
        if (!GetToken ())
            return false;

        // Expect "single", "double", "integer", "string" or a structure type
        if (m_token.m_type != CTT_TEXT && m_token.m_type != CTT_KEYWORD) {
            SetError ("Expected 'single', 'double', 'real', 'integer', 'string' or type name");
            return false;
        }
        if (m_token.m_text == "integer")
            type.m_basicType = VTP_INT;
        else    if (m_token.m_text == "single"
                ||  m_token.m_text == "double")

            // Note: Basic4GL supports only one type of floating point number.
            // We will accept both keywords, but simply allocate a real (= single
            // precision) floating point number each time.
            type.m_basicType = VTP_REAL;
        else if (m_token.m_text == "string")
            type.m_basicType = VTP_STRING;
        else {

            // Look for recognised structure name
            int i = m_vm.DataTypes ().GetStruc (m_token.m_text);
            if (i < 0) {
                SetError ("Expected 'single', 'double', 'real', 'integer', 'string' or type name");
                return false;
            }
            type.m_basicType = (vmBasicValType) i;
        }

        // Skip type name
        if (!GetToken())
            return false;
    }
    return true;
}

bool TomBasicCompiler::CompileLoadVar () {

    // Look for "take address"
    bool takeAddress = false;
    if (m_token.m_text == "&") {
        takeAddress = true;
        if (!GetToken ())
            return false;
    }

    // Look for variable name
    if (m_token.m_type != CTT_TEXT) {
        SetError ("Expected variable name");
        return false;
    }
    int varIndex = m_vm.Variables ().GetVar (m_token.m_text);
    if (!m_vm.Variables ().IndexValid (varIndex)) {
        SetError ((std::string) "Unknown variable: " + m_token.m_text + ". Must be declared with DIM");
        return false;
    }
    if (!GetToken ())
        return false;

    // Generate code to load variable
    AddInstruction (OP_LOAD_VAR, VTP_INT, vmValue (varIndex));

    // Register now contains a pointer to variable
    vmVariable& var = m_vm.Variables ().Variables () [varIndex];
    m_regType = var.m_type;
    m_regType.m_pointerLevel++;

    // Dereference to reach data
    if (!CompileDerefs ())
        return false;

    // Compile data lookups (e.g. ".fieldname", array indices, take address e.t.c)
    return CompileDataLookup (takeAddress);
}

bool TomBasicCompiler::CompileDerefs () {

    // Generate code to dereference pointer
    if (!CompileDeref ())
        return false;

    // In Basic4GL syntax, pointers are implicitly dereferenced (similar to C++'s
    // "reference" type.)
    if (m_regType.VirtualPointerLevel () > 0)
        if (!CompileDeref ())
            return false;

    return true;
}

bool TomBasicCompiler::CompileDeref () {

    // Generate code to dereference pointer in reg. (i.e reg = [reg]).
    assert (m_vm.DataTypes ().TypeValid (m_regType));

    // Not a pointer?
    if (m_regType.VirtualPointerLevel () <= 0) {
        assert (false);                                                                 // This should never happen
        SetError ("INTERNAL COMPILER ERROR: Attempted to dereference a non-pointer");
        return false;
    }

    // If reg is pointing to a structure or an array, we don't dereference
    // (as we can't fit an array or structure into a 4 byte register!).
    // Instead we leave it as a pointer, but update the type to indicate
    // to the compiler that we are using a pointer internally to represent
    // a variable.
    assert (!m_regType.m_byRef);
    if (    m_regType.PhysicalPointerLevel () == 1  // Must be pointer to actual data (not pointer to pointer e.t.c)
        && (    m_regType.m_arrayLevel > 0          // Array
            ||  m_regType.m_basicType >= 0)) {      // or structure
        m_regType.m_byRef = true;
        return true;
    }

    /////////////////////////
    // Generate dereference

    // Generate deref instruction
    m_regType.m_pointerLevel--;
    AddInstruction (OP_DEREF, m_regType.StoredType (), vmValue ());  // Load variable

    return true;
} 

bool TomBasicCompiler::CompileDataLookup (bool takeAddress) {

    // Compile various data operations that can be performed on data object.
    // These operations include:
    //  * Array indexing:           data (index)
    //  * Field lookup:             data.field
    //  * Taking address:           &data
    // Or any combination of the above.

    bool done = false;
    while (!done) {
        if (m_token.m_text == ".") {

            // Lookup subfield
            // Register must contain a structure type
            if (    m_regType.VirtualPointerLevel () != 0
                ||  m_regType.m_arrayLevel != 0
                ||  m_regType.m_basicType < 0) {
                SetError ("Unexpected '.'");
                return false;
            }
            assert (m_vm.DataTypes ().TypeValid (m_regType));

            // Skip "."
            if (!GetToken ())
                return false;

            // Read field name
            if (m_token.m_type != CTT_TEXT) {
                SetError ("Expected field name");
                return false;
            }
            std::string fieldName = m_token.m_text;
            if (!GetToken ())
                return false;

            // Validate field
            vmStructure& s = m_vm.DataTypes ().Structures () [m_regType.m_basicType];
            int fieldIndex = m_vm.DataTypes ().GetField (s, fieldName);
            if (fieldIndex < 0) {
                SetError ((std::string) "'" + fieldName + "' is not a field of structure '" + s.m_name + "'");
                return false;
            }

            // Generate code to calculate address of field
            // Reg is initially pointing to address of structure.
            vmStructureField& field = m_vm.DataTypes ().Fields () [fieldIndex];
            AddInstruction (OP_ADD_CONST, VTP_INT, vmValue (field.m_dataOffset));

            // Reg now contains pointer to field
            m_regType = field.m_type;
            m_regType.m_pointerLevel++;

            // Dereference to reach data
            if (!CompileDerefs ())
                return false;
        }
        else if (m_token.m_text == "(") {

            // Register must contain an array
            if (    m_regType.VirtualPointerLevel () != 0
                ||  m_regType.m_arrayLevel == 0) {
                SetError ("Unexpected '('");
                return false;
            }

            do {
                if (m_regType.m_arrayLevel == 0) {
                    SetError ("Unexpected ','");
                    return false;
                }

                // Index into array
                if (!GetToken ())                   // Skip "(" or ","
                    return false;

                // Generate code to push array address
                if (!CompilePush ())
                    return false;

                // Evaluate array index, and convert to an integer.
                if (!CompileExpression ())
                    return false;
                if (!CompileConvert (VTP_INT)) {
                    SetError ("Array index must be a number. " + m_vm.DataTypes ().DescribeVariable ("", m_regType) + " is not a number");
                    return false;
                }

                // Generate code to pop array address into reg2
                if (!CompilePop ())
                    return false;

                // Generate code to index into array.
                // Input:  reg  = Array index
                //         reg2 = Array address
                // Output: reg  = Pointer to array element
                AddInstruction (OP_ARRAY_INDEX, VTP_INT, vmValue ());

                // reg now points to an element
                m_regType = m_reg2Type;
                m_regType.m_byRef           = false;
                m_regType.m_pointerLevel    = 1;
                m_regType.m_arrayLevel--;

                // Dereference to get to element
                if (!CompileDerefs ())
                    return false;

            } while (m_token.m_text == ",");

            // Expect closing bracket
            if (m_token.m_text != ")") {
                SetError ("Expected ')'");
                return false;
            }
            if (!GetToken ())
                return false;
        }
        else
            done = true;
    }

    // Compile take address (if necessary)
    if (takeAddress)
        if (!CompileTakeAddress ())
            return false;

    return true;
}

bool TomBasicCompiler::CompileExpression (bool mustBeConstant) {

    // Compile expression.
    // Generates code that once executed will leave the result of the expression
    // in Reg.

    // Must start with either:
    //      A constant (numeric or string)
    //      A variable reference

    // Push "stop evaluation" operand to stack. (To protect any existing operators
    // on the stack.
    m_operatorStack.push_back (compOperator (OT_STOP, OP_NOP, 0, -200000));    // Stop evaluation operator

    if (!CompileExpressionLoad (mustBeConstant))
        return false;

    compOperatorMap::iterator o;
    while ((m_token.m_text == ")" && OperatorTOS ().m_type != OT_STOP) || (o = m_binaryOperators.find (m_token.m_text)) != m_binaryOperators.end ()) {

        // Special case, right bracket
        if (m_token.m_text == ")") {

            // Evaluate all operators down to left bracket
            while (OperatorTOS ().m_type != OT_STOP && OperatorTOS ().m_type != OT_LBRACKET)
                if (!CompileOperation ())
                    return false;

            // If operator stack is empty, then the expression terminates before
            // the closing bracket
            if (OperatorTOS ().m_type == OT_STOP) {
                m_operatorStack.pop_back ();            // Remove stopper
                return true;
            }

            // Remove left bracket
            m_operatorStack.pop_back ();

            // Move on
            if (!GetToken ())
                return false;

            // Result may be an array or a structure to which a data lookup can
            // be applied.
            if (!CompileDataLookup (false))
                return false;
        }

        // Otherwise must be regular binary operator
        else {

            // Compare current operator with top of stack operator
            while (OperatorTOS ().m_type != OT_STOP && OperatorTOS ().m_binding >= (*o).second.m_binding)
                if (!CompileOperation ())
                    return false;

            // Save operator to stack
            m_operatorStack.push_back ((*o).second);

            // Push first operand
            if (!CompilePush ())
                return false;

            // Load second operand
            if (!GetToken ())
                return false;
            if (!CompileExpressionLoad (mustBeConstant))
                return false;
        }
    }

    // Perform remaining operations
    while (OperatorTOS ().m_type != OT_STOP)
        if (!CompileOperation ())
            return false;

    // Remove stopper
    m_operatorStack.pop_back ();

    return true;
}

bool TomBasicCompiler::CompileOperation () {

    // Compile topmost operation on operator stack
    assert (!m_operatorStack.empty ());

    // Remove operator from stack
    compOperator& o = OperatorTOS ();
    m_operatorStack.pop_back ();

    // Must not be a left bracket
    if (o.m_type == OT_LBRACKET) {
        SetError ("Expected ')'");
        return false;
    }

    // Binary or unary operation?
    if (o.m_params == 1) {

        // Try plug in language extension first
        if (CompileExtendedUnOperation (o.m_opCode))
            return true;

        // Can only operate on basic types.
        // (This will change once vector and matrix routines have been implemented).
        if (!m_regType.IsBasic ()) {
            SetError ("Operator cannot be applied to this data type");
            return false;
        }

        // Special case, boolean operator.
        // Must convert to boolean first
        if (o.m_type == OT_BOOLOPERATOR)
            CompileConvert (VTP_INT);

        // Perform unary operation
        AddInstruction (o.m_opCode, m_regType.m_basicType, vmValue ());

        // Special case, boolean operator
        // Result will be an integer
        if (o.m_type == OT_RETURNBOOLOPERATOR)
            m_regType = VTP_INT;
    }
    else if (o.m_params == 2) {

        // Generate code to pop first operand from stack into Reg2
        if (!CompilePop ())
            return false;

        // Try plug in language extension first
        if (CompileExtendedBinOperation (o.m_opCode))
            return true;

        // Ensure operands are equal type. Generate code to convert one if necessary.

        vmBasicValType opCodeType;                  // Data type to store in OP_CODE
        if (m_regType.IsNull () || m_reg2Type.IsNull ()) {

            // Can compare NULL to any pointer type. However, operator must be '=' or '<>'
            if (o.m_opCode != OP_OP_EQUAL && o.m_opCode != OP_OP_NOT_EQUAL) {
                SetError ("Operator cannot be applied to this data type");
                return false;
            }

            // Convert NULL pointer type to non NULL pointer type
            // Note: If both pointers a NULL, CompileConvert will simply do nothing
            if (m_regType.IsNull ())
                if (!CompileConvert (m_reg2Type))
                    return false;

            if (m_reg2Type.IsNull ())
                if (!CompileConvert2 (m_regType))
                    return false;

            opCodeType = VTP_INT;               // Integer comparison is used internally
        }
        else if (m_regType.VirtualPointerLevel () > 0 || m_reg2Type.VirtualPointerLevel () > 0) {

            // Can compare 2 pointers. However operator must be '=' or '<>' and
            // pointer types must be exactly the same
            if (o.m_opCode != OP_OP_EQUAL && o.m_opCode != OP_OP_NOT_EQUAL) {
                SetError ("Operator cannot be applied to this data type");
                return false;
            }
            if (!m_regType.ExactEquals (m_reg2Type)) {
                SetError ("Cannot compare pointers to different types");
                return false;
            }

            opCodeType = VTP_INT;               // Integer comparison is used internally
        }
        else {

            // Otherwise all operators can be applied to basic data types
            if (!m_regType.IsBasic () || !m_reg2Type.IsBasic ()) {
                SetError ("Operator cannot be applied to this data type");
                return false;
            }

            // Convert operands to highest type
            vmBasicValType highest = m_regType.m_basicType;
            if (m_reg2Type.m_basicType > highest)
                highest = m_reg2Type.m_basicType;
            if (o.m_type == OT_BOOLOPERATOR)
                highest = VTP_INT;
            if (m_syntax == LS_TRADITIONAL && o.m_opCode == OP_OP_DIV)        // 14-Aug-05 Tom: In traditional mode, division is always between floating pt numbers
                highest = VTP_REAL;

            CompileConvert (highest);
            CompileConvert2 (highest);
            opCodeType = highest;
        }

        // Generate operation code
        AddInstruction (o.m_opCode, opCodeType, vmValue ());

        // Special case, boolean operator
        // Result will be an integer
        if (o.m_type == OT_RETURNBOOLOPERATOR)
            m_regType = VTP_INT;
    }
    else
        assert (false);

    return true;
}

bool TomBasicCompiler::CompileExpressionLoad (bool mustBeConstant) {

    // Like CompileLoad, but will also accept and stack preceeding unary operators

    // Push any unary operators found
    while (true) {

        // Special case, left bracket
        if (m_token.m_text == "(")
            m_operatorStack.push_back (compOperator (OT_LBRACKET, OP_NOP, 0, -10000));         // Brackets bind looser than anything else

        // Otherwise look for recognised unary operator
        else {
            compOperatorMap::iterator o = m_unaryOperators.find (m_token.m_text);
            if (o != m_unaryOperators.end ())                   // Operator found
                m_operatorStack.push_back((*o).second);         // => Stack it
            else {                                              // Not an operator
                if (mustBeConstant)
                    return CompileLoadConst ();
                else
                    return CompileLoad ();                      // => Proceed on to load variable/constant
            }
        }

        if (!GetToken ())
            return false;
    }
}

bool TomBasicCompiler::CompileLoad () {

    // Compile load var or constant, or function result
    if (m_token.m_type == CTT_CONSTANT || m_token.m_text == "null")
        return CompileLoadConst ();
    else if (m_token.m_type == CTT_TEXT || m_token.m_text == "&")
        return CompileLoadVar ();
    else if (m_token.m_type == CTT_FUNCTION) {
        return CompileFunction (true);
    }

    SetError ("Expected constant, variable or function");
    return false;
}

bool TomBasicCompiler::CompileLoadConst () {

    // Special case, "NULL" reserved word
    if (m_token.m_text == "null") {
        AddInstruction (OP_LOAD_CONST, VTP_INT, vmValue (0));       // Load 0 into register
        m_regType = vmValType (VTP_NULL, 0, 1, false);              // Type is pointer to VTP_NULL
        return GetToken ();
    }

    // Compile load constant
    if (m_token.m_type == CTT_CONSTANT) {

        // Special case, string constants
        if (m_token.m_valType == VTP_STRING) {

            // Allocate new string constant
            std::string text = m_token.m_text.substr (1, m_token.m_text.length () - 1);     // Remove S prefix
            int index = m_vm.StoreStringConstant (text);

            // store load instruction
            AddInstruction (OP_LOAD_CONST, VTP_STRING, vmValue (index));
            m_regType = VTP_STRING;
        }
        else if (m_token.m_valType == VTP_REAL) {
            AddInstruction (OP_LOAD_CONST, VTP_REAL, vmValue (StringToReal (m_token.m_text)));
            m_regType = VTP_REAL;
        }
        else if (m_token.m_valType == VTP_INT) {
            AddInstruction (OP_LOAD_CONST, VTP_INT, vmValue (StringToInt (m_token.m_text)));
            m_regType = VTP_INT;
        }
        else {
            SetError ("Unknown data type");
            return false;
        }

        return GetToken ();
    }

    SetError ("Expected constant");
    return false;
}

bool TomBasicCompiler::CompilePush () {

    // Store pushed value type
    m_operandStack.push_back (m_regType);

    // Generate push code
    AddInstruction (OP_PUSH, m_regType.StoredType (), vmValue ());

    return true;
}

bool TomBasicCompiler::CompilePop () {

    if (m_operandStack.empty ()) {
        SetError ("Expression error");
        return false;
    }

    // Retrieve pushed value type
    m_reg2Type = m_operandStack [m_operandStack.size () - 1];
    m_operandStack.pop_back ();

    // Generate pop code
    AddInstruction (OP_POP, m_reg2Type.StoredType (), vmValue ());

    return true;
}

bool TomBasicCompiler::CompileConvert (vmBasicValType type) {

    // Convert reg to given type
    if (m_regType == type)              // Already same type
        return true;

    // Determine opcode
    vmOpCode code = OP_NOP;
    if (m_regType == VTP_INT) {
        if (type == VTP_REAL)           code = OP_CONV_INT_REAL;
        else if (type == VTP_STRING)    code = OP_CONV_INT_STRING;
    }
    else if (m_regType == VTP_REAL) {
        if (type == VTP_INT)            code = OP_CONV_REAL_INT;
        else if (type == VTP_STRING)    code = OP_CONV_REAL_STRING;
    }

    // Store instruction
    if (code != OP_NOP) {
        AddInstruction (code, VTP_INT, vmValue ());
        m_regType = type;
        return true;
    }

    SetError ("Incorrect data type");
    return false;
}

bool TomBasicCompiler::CompileConvert2 (vmBasicValType type) {

    // Convert reg2 to given type
    if (m_reg2Type == type)             // Already same type
        return true;

    // Determine opcode
    vmOpCode code = OP_NOP;
    if (m_reg2Type == VTP_INT) {
        if (type == VTP_REAL)           code = OP_CONV_INT_REAL2;
        else if (type == VTP_STRING)    code = OP_CONV_INT_STRING2;
    }
    else if (m_reg2Type == VTP_REAL) {
        if (type == VTP_INT)            code = OP_CONV_REAL_INT2;
        else if (type == VTP_STRING)    code = OP_CONV_REAL_STRING2;
    }

    // Store instruction
    if (code != OP_NOP) {
        AddInstruction (code, VTP_INT, vmValue ());
        m_reg2Type = type;
        return true;
    }

    SetError ("Incorrect data type");
    return false;
}

bool TomBasicCompiler::CompileTakeAddress () {

    // Take address of data in reg.
    // We do this my moving the previously generate deref from the end of the program.
    // (If the last instruction is not a deref, then there is a problem.)

    // Special case: Implicit pointer
    if (m_regType.m_byRef) {
        m_regType.m_byRef = false;              // Convert to explicit pointer
        return true;
    }

    // Check last instruction was a deref
    if (m_vm.InstructionCount () <= 0 || m_vm.Instruction (m_vm.InstructionCount () - 1).m_opCode != OP_DEREF) {
        SetError ("Cannot take address of this data");
        return false;
    }

    // Remove it
    m_vm.RemoveLastInstruction ();
    m_regType.m_pointerLevel++;

    return true;        
}

bool TomBasicCompiler::CompileAssignment () {

    // Generate code to load target variable
    if (!CompileLoadVar ())
        return false;

    // Expect =
    if (m_token.m_text != "=") {
        SetError ("Expected '='");
        return false;
    }

    // Convert load target variable into take address of target variable
    if (!CompileTakeAddress ()) {
        SetError ("Left side cannot be assigned to");
        return false;
    }

    // Skip =
    if (!GetToken ())
        return false;

    // Push target address
    if (!CompilePush ())
        return false;

    // Generate code to evaluate expression
    if (!CompileExpression ())
        return false;

    // Pop target address into reg2
    if (!CompilePop ())
        return false;

    // Simple type case: reg2 points to basic type
    if (    m_reg2Type.m_pointerLevel == 1
        &&  m_reg2Type.m_arrayLevel   == 0
        &&  m_reg2Type.m_basicType    < 0) {

        // Attempt to convert value in reg to same type
        if (!CompileConvert (m_reg2Type.m_basicType)) {
            SetError ("Types do not match");
            return false;
        }

        // Save reg into [reg2]
        AddInstruction (OP_SAVE, m_reg2Type.m_basicType, vmValue ());
    }

    // Pointer case. m_reg2 must point to a pointer and m_reg1 point to a value.
    else if (m_reg2Type.VirtualPointerLevel () == 2 && m_regType.VirtualPointerLevel () == 1) {

        // Must both point to same type, OR m_reg1 must point to NULL 
        if (    m_regType.IsNull ()
            || (    m_regType.m_arrayLevel == m_reg2Type.m_arrayLevel
                &&  m_regType.m_basicType  == m_reg2Type.m_basicType))
            AddInstruction (OP_SAVE, VTP_INT, vmValue ());              // Generate code to save address
            
        else {
            SetError ("Types do not match");
            return false;
        }
    }

    // Copy object case
    else if (   m_reg2Type.VirtualPointerLevel () == 1
            &&  m_regType.VirtualPointerLevel () == 0
            &&  m_regType.PhysicalPointerLevel () == 1) {

        // Check that both are the same type
        if (    m_regType.m_arrayLevel == m_reg2Type.m_arrayLevel
            &&  m_regType.m_basicType  == m_reg2Type.m_basicType)
            AddInstruction (OP_COPY, VTP_INT, vmValue ((vmInt) m_vm.StoreType (m_regType)));
        else {
            SetError ("Types do not match");
            return false;
        }
    }
    else {
        SetError ("Types do not match");
        return false;
    }

    return true;
}

bool TomBasicCompiler::CompileGoto (vmOpCode jumpType) {
    assert (    jumpType == OP_JUMP
            ||  jumpType == OP_JUMP_TRUE
            ||  jumpType == OP_JUMP_FALSE
            ||  jumpType == OP_CALL);

    // Validate label
    if (m_token.m_type != CTT_TEXT) {
        SetError ("Expected label name");
        return false;
    }

    // Record jump, so that we can fix up the offset in the second compile pass.
    m_jumps.push_back (compJump (m_vm.InstructionCount (), m_token.m_text));

    // Add jump instruction
    AddInstruction (jumpType, VTP_INT, vmValue (0));

    // Move on
    return GetToken ();
}

bool TomBasicCompiler::CompileIf (bool elseif) {

    // Skip "if"
    int line = m_parser.Line (), col = m_parser.Col ();
    if (!GetToken ())
        return false;

    // Generate code to evaluate expression
    if (!CompileExpression ())
        return false;

    // Generate code to convert to integer
    if (!CompileConvert (VTP_INT))
        return false;

    // Free any temporary data expression may have created
    if (!CompileFreeTempData ())
        return false;

    // Special case!
    // If next instruction is a "goto", then we can ommit the "then"
    if (m_token.m_text != "goto") {

        // Otherwise expect "then"
        if (m_token.m_text != "then") {
            SetError ("Expected 'then'");
            return false;
        }
        if (!GetToken ())
            return false;
    }

    // Determine whether this "if" has an automatic "endif" inserted at the end of the line
    bool autoEndif = m_syntax == LS_TRADITIONAL                                 // Only applies to traditional syntax
        && !(m_token.m_type == CTT_EOL || m_token.m_type == CTT_EOF);           // "then" must not be the last token on the line

    // Create flow control structure
    m_flowControl.push_back (compFlowControl (FCT_IF, m_vm.InstructionCount (), 0, line, col, elseif, "", !autoEndif));

    // Create conditional jump
    AddInstruction (OP_JUMP_FALSE, VTP_INT, vmValue (0));

    m_needColon = false;                // Don't need colon between this and next instruction
    return true;
}

bool TomBasicCompiler::CompileElse (bool elseif) {

    // Find "if" on top of flow control stack
    if (!FlowControlTopIs (FCT_IF)) {
        SetError ("'else' without 'if'");
        return false;
    }
    compFlowControl top = FlowControlTOS ();
    m_flowControl.pop_back ();

    // Skip "else"
    // (But not if it's really an "elseif". CompileIf will skip over it then.)
    int line = m_parser.Line (), col = m_parser.Col ();
    if (!elseif) {
        if (!GetToken ())
            return false;
    }

    // Push else to flow control stack
    m_flowControl.push_back (compFlowControl (FCT_ELSE, m_vm.InstructionCount (), 0, line, col, top.m_impliedEndif, "", top.m_blockIf));

    // Generate code to jump around else block
    AddInstruction (OP_JUMP, VTP_INT, vmValue (0));

    // Fixup jump around IF block
    assert (top.m_jumpOut < m_vm.InstructionCount ());
    m_vm.Instruction (top.m_jumpOut).m_value.IntVal () = m_vm.InstructionCount ();

    m_needColon = false;                // Don't need colon between this and next instruction
    return true;
}

bool TomBasicCompiler::CompileEndIf (bool automatic) {

    // Find if or else on top of flow control stack
    if (!(FlowControlTopIs (FCT_IF) || FlowControlTopIs (FCT_ELSE))) {
        SetError ("'endif' without 'if'");
        return false;
    }
    compFlowControl top = FlowControlTOS ();
    m_flowControl.pop_back ();

    // Skip "endif"
    if (!top.m_impliedEndif && !automatic) {
        if (!GetToken ())
            return false;
    }

    // Fixup jump around IF or ELSE block
    assert (top.m_jumpOut < m_vm.InstructionCount ());
    m_vm.Instruction (top.m_jumpOut).m_value.IntVal () = m_vm.InstructionCount ();

    // If there's an implied endif then add it
    if (top.m_impliedEndif)
        return CompileEndIf (automatic);
    else
        return true;
}

bool TomBasicCompiler::CompileFor () {

    // Skip "for"
    int line = m_parser.Line (), col = m_parser.Col ();
    if (!GetToken ())
        return false;

    // Extract loop variable name
    compToken nextToken = m_parser.PeekToken (false);
    if (!CheckParser ())
        return false;
    if (nextToken.m_text == "(") {
        SetError ("Cannot use array variable in 'for' - 'next' structure");
        return false;
    }
    std::string loopVar = m_token.m_text;

    // Verify variable is numeric
    int varIndex = m_vm.Variables ().GetVar(loopVar);
    if (varIndex < 0) {
        SetError ((std::string) "Unknown variable: " + m_token.m_text + ". Must be declared with DIM");
        return false;
    }
    vmVariable& var = m_vm.Variables ().Variables () [varIndex];
    if (!(var.m_type == VTP_INT || var.m_type == VTP_REAL)) {
        SetError ((std::string) "Loop variable must be an Integer or Real");
        return false;
    }

    // Compile assignment
    int varLine = m_parser.Line (), varCol = m_parser.Col ();
    compToken varToken = m_token;
    if (!CompileAssignment ())
        return false;

    // Save loop back position
    int loopPos = m_vm.InstructionCount ();

    // Expect "to"
    if (m_token.m_text != "to") {
        SetError ("Expected 'to'");
        return false;
    }
    if (!GetToken ())
        return false;

    // Compile load variable and push
    compParserPos savedPos = SavePos ();                            // Save parser position
    m_parser.SetPos (varLine, varCol);                              // Point to variable name
    m_token = varToken;

    if (!CompileLoadVar ())                                         // Load variable
        return false;
    if (!CompilePush ())                                            // And push
        return false;

    RestorePos (savedPos);                                          // Restore parser position

    // Compile "to" expression
    if (!CompileExpression ())
        return false;
    if (!CompileConvert (var.m_type))
        return false;

    // Evaluate step. (Must be a constant expression)
    vmBasicValType stepType = var.m_type.m_basicType;
    vmValue stepValue;

    if (m_token.m_text == "step") {

        // Skip step instruction
        if (!GetToken ())
            return false;

        // Compile step constant (expression)
        std::string stringValue;
        if (!EvaluateConstantExpression(stepType, stepValue, stringValue))
            return false;
    }
    else {

        // No explicit step.
        // Use 1 as default
        if (stepType == VTP_INT)
            stepValue = vmValue (1);
        else
            stepValue = vmValue ((vmReal) 1.0f);
    }

    // Choose comparison operator (based on direction of step)
    compOperator comparison;
    if (stepType == VTP_INT) {
             if (stepValue.IntVal () > 0)   comparison = m_binaryOperators ["<="];
        else if (stepValue.IntVal () < 0)   comparison = m_binaryOperators [">="];
        else                                comparison = m_binaryOperators ["<>"];
    }
    else {
        assert (stepType == VTP_REAL);
             if (stepValue.RealVal () > 0)  comparison = m_binaryOperators ["<="];
        else if (stepValue.RealVal () < 0)  comparison = m_binaryOperators [">="];
        else                                comparison = m_binaryOperators ["<>"];
    }

    // Compile comparison expression
    m_operatorStack.push_back (comparison);
    if (!CompileOperation ())
        return false;

    // Generate step expression
    std::string step = loopVar + " = " + loopVar + " + " + (stepType == VTP_INT ? IntToString (stepValue.IntVal ()) : RealToString (stepValue.RealVal ()));

    // Create flow control structure
    m_flowControl.push_back (compFlowControl (FCT_FOR, m_vm.InstructionCount (), loopPos, line, col, false, step));

    // Create conditional jump
    AddInstruction (OP_JUMP_FALSE, VTP_INT, vmValue (0));

    return true;
}

bool TomBasicCompiler::CompileNext () {

    // Find for on top of flow control stack
    if (!FlowControlTopIs (FCT_FOR)) {
        SetError ("'for' without 'next'");
        return false;
    }
    compFlowControl top = FlowControlTOS ();
    m_flowControl.pop_back ();

    // Skip "next"
    int nextLine = m_token.m_line, nextCol = m_token.m_col;
    if (!GetToken ())
        return false;

    // Generate instruction to increment loop variable
    m_parser.SetSpecial (top.m_data, nextLine, nextCol);    // Special mode. Compile this string instead of source code.
                                                            // We pass in the line and the column of the "next" instruction so that generated code will be
                                                            // associated with this offset in the source code. (This keeps the debugger happy) 
    compToken saveToken = m_token;
    if (!GetToken ())
        return false;
    if (!CompileAssignment ())
        return false;
    m_parser.SetNormal ();
    m_token = saveToken;

    // Generate jump back instruction
    AddInstruction (OP_JUMP, VTP_INT, vmValue (top.m_jumpLoop));

    // Fixup jump around FOR block
    assert (top.m_jumpOut < m_vm.InstructionCount ());
    m_vm.Instruction (top.m_jumpOut).m_value.IntVal () = m_vm.InstructionCount ();
    return true;
}

bool TomBasicCompiler::CompileWhile () {

    // Save loop position
    int loopPos = m_vm.InstructionCount ();

    // Skip "while"
    int line = m_parser.Line (), col = m_parser.Col ();
    if (!GetToken ())
        return false;

    // Generate code to evaluate expression
    if (!CompileExpression ())
        return false;

    // Generate code to convert to integer
    if (!CompileConvert (VTP_INT))
        return false;

    // Free any temporary data expression may have created
    if (!CompileFreeTempData ())
        return false;

    // Create flow control structure
    m_flowControl.push_back (compFlowControl (FCT_WHILE, m_vm.InstructionCount (), loopPos, line, col));

    // Create conditional jump
    AddInstruction (OP_JUMP_FALSE, VTP_INT, vmValue (0));
    return true;
}

bool TomBasicCompiler::CompileWend () {

    // Find while on top of flow control stack
    if (!FlowControlTopIs (FCT_WHILE)) {
        SetError ("'wend' without 'while'");
        return false;
    }
    compFlowControl top = FlowControlTOS ();
    m_flowControl.pop_back ();

    // Skip "wend"
    if (!GetToken ())
        return false;

    // Generate jump back
    AddInstruction (OP_JUMP, VTP_INT, vmValue (top.m_jumpLoop));

    // Fixup jump around WHILE block
    assert (top.m_jumpOut < m_vm.InstructionCount ());
    m_vm.Instruction (top.m_jumpOut).m_value.IntVal () = m_vm.InstructionCount ();
    return true;
}

bool TomBasicCompiler::CheckName (std::string name) {

    // Check that name is a suitable variable, structure or structure field name.
    if (m_constants.find (name) != m_constants.end ()
    ||  m_programConstants.find (name) != m_programConstants.end ()) {
        SetError ("'" + name + "' is a constant, and cannot be used here");
        return false;
    }
    if (m_reservedWords.find (name) != m_reservedWords.end ()) {
        SetError ("'" + name + "' is a reserved word, and cannot be used here");
        return false;
    }
    return true;
}

void TomBasicCompiler::AddFunction (    std::string         name,
                                        vmFunction          func,
                                        compParamTypeList   params,
                                        bool                isFunction,
                                        bool                brackets,
                                        vmValType           returnType,
                                        bool                timeshare,
                                        bool                freeTempData) {

    // Register wrapper function to virtual machine
    int vmIndex = m_vm.AddFunction (func);

    // Register function spec to compiler
    int specIndex = m_functions.size ();
    m_functions.push_back (compFuncSpec (params, isFunction, brackets, returnType, timeshare, vmIndex, freeTempData));

    // Add function name -> function spec mapping
    m_functionIndex.insert (std::make_pair(LowerCase (name), specIndex));
}

bool TomBasicCompiler::CompileFunction (bool needResult) {

    // Find function specifications.
    // (Note: There may be more than one with the same name.
    // We collect the possible candidates in an array, and prune out the ones
    // whose paramater types are incompatible as we go..)
    compFuncSpec *functions [TC_MAXOVERLOADEDFUNCTIONS];
    int functionCount = 0;

    bool found = false;
    compFuncIndex::iterator it;
    for (   it = m_functionIndex.find (m_token.m_text);
            it != m_functionIndex.end () &&  (*it).first == m_token.m_text && functionCount < TC_MAXOVERLOADEDFUNCTIONS;
            it++) {
        compFuncSpec& spec = m_functions [(*it).second];                        // Get specification
        found = true;

        // Check whether function returns a value (if we need one)
        if (!needResult || spec.m_isFunction)
            functions [functionCount++] = &spec;
    }

    // No functions?
    if (functionCount == 0) {

        if (found) {
            // We found some functions, but discarded them all. This would only
            // ever happen if we required a return value, but none of the functions
            // return one.
            SetError (m_token.m_text + " does not return a value");
            return false;
        }
        else {
            SetError (m_token.m_text + " is not a recognised function name");
            return false;
        }
    }

    // Skip function name token
    if (!GetToken ())
        return false;

    // Only the first instance will be checked to see whether we need brackets.
    // (Therefore either all instances should have brackets, or all instances
    // should have no brackets.)
    bool brackets = functions [0]->m_brackets;
    if (m_syntax == LS_TRADITIONAL && brackets) {       // Special brackets rules for traditional syntax

        brackets = false;
        // Look for a version of the function that:
        //  * Has at least one parameter, AND
        //  * Returns a value
        //
        // If one is found, then we require brackets. Otherwise we don't.
        for (int i = 0; i < functionCount && !brackets; i++)
            brackets = functions[i]->m_isFunction
;//                    && functions[i]->m_paramTypes.Params().size() > 0;       // Need to rethink below loop before we can enable this
    }

    // Expect opening bracket
    if (brackets) {
        if (m_token.m_text != "(") {
            SetError ("Expected '('");
            return false;
        }
        // Skip it
        if (!GetToken ())
            return false;
    }

    // Generate code to push parameters
    bool first = true;
    int count = 0;
    while (functionCount > 0 && m_token.m_text != ")" && !AtSeparatorOrSpecial ()) {

        // Trim functions with less parameters than we have found
        int src, dst;
        dst = 0;
        for (src = 0; src < functionCount; src++)
            if (functions [src]->m_paramTypes.Params ().size () > count)
                functions [dst++] = functions [src];
        functionCount = dst;

        // None left?
        if (functionCount == 0) {
            if (brackets)   SetError ("Expected ')'");
            else            SetError ("Expected ':' or end of line");
            return false;
        }

        if (!first) {
            // Expect comma
            if (m_token.m_text != ",") {
                SetError ("Expected ','");
                return false;
            }
            // Skip it
            if (!GetToken ())
                return false;
        }
        first = false;

        // Generate code to evaluate parameter
        if (!CompileExpression ())
            return false;

        // Find first valid function which matches at this parameter
        int matchIndex = -1;
        int i;
        for (i = 0; i < functionCount && matchIndex < 0; i++)
            if (CompileConvert (functions [i]->m_paramTypes.Params () [count]))     // Can convert register to parameter type
                matchIndex = i;                                                     // Matching function found

        if (matchIndex >= 0) {

            // Clear any errors that non-matching instances might have set.
            ClearError ();

            vmValType& type = functions [matchIndex]->m_paramTypes.Params () [count];

            // Filter out all functions whose "count" parameter doesn't match "type".
            int src, dst;
            dst = 0;
            for (src = 0; src < functionCount; src++)
                if (functions [src]->m_paramTypes.Params () [count] == type)
                    functions [dst++] = functions [src];
            functionCount = dst;
            assert (functionCount > 0);     // (Should at least have the function that originally matched the register)

            // Generate code to push parameter to stack
            CompilePush ();
        }
        else
            return false;                   // No function matched. (Return last compile convert error).

        count++;                            // Count parameters pushed
    }

    // Find the first function instance that accepts this number of parameters
    int matchIndex = -1;
    int i;
    for (i = 0; i < functionCount && matchIndex < 0; i++)
        if (functions [i]->m_paramTypes.Params ().size () == count)
            matchIndex = i;
    if (matchIndex < 0) {
        if (count == 0) SetError ("Expected function parameter");
        else            SetError ("Expected ','");
        return false;
    }
    compFuncSpec& spec = *functions [matchIndex];

    // Expect closing bracket
    if (brackets) {
        if (m_token.m_text != ")") {
            SetError ("Expected ')'");
            return false;
        }
        // Skip it
        if (!GetToken ())
            return false;
    }

    // Generate code to call function
    AddInstruction (OP_CALL_FUNC, VTP_INT, vmValue (spec.m_index));

    // If function has return type, it will have changed the type in the register
    if (spec.m_isFunction) {
        m_regType = spec.m_returnType;
        if (!CompileDataLookup (false))
            return false;
    }

    // Note whether function has generated temporary data
    m_freeTempData = m_freeTempData | spec.m_freeTempData;

    // Generate code to clean up stack
    for (int i2 = 0; i2 < count; i2++)
        if (!CompilePop ())
            return false;

    // Generate explicit timesharing break (if necessary)
    if (spec.m_timeshare)
        AddInstruction (OP_TIMESHARE, VTP_INT, vmValue ());

    return true;
}

bool TomBasicCompiler::CompileConvert (vmValType type) {

    // Can convert NULL to a different pointer type
    if (m_regType.IsNull ()) {
        if (type.VirtualPointerLevel () <= 0) {
            SetError ("Cannot convert NULL to " + m_vm.DataTypes ().DescribeVariable ("", type));
            return false;
        }

        // No generated code necessary, just substitute in type
        m_regType = type;
        return true;
    }

    // Can convert values to references. (This is used when evaluating function
    // parameters.)
    if (    type.m_pointerLevel         == 1 && type.m_byRef    // type is a reference
        &&  m_regType.m_pointerLevel    == 0                    // regType is a value
        &&  m_regType.m_basicType       == type.m_basicType     // Same type of data
        &&  m_regType.m_arrayLevel      == type.m_arrayLevel) {

        // Convert register to pointer
        if (CompileTakeAddress ()) {

            // Convert pointer to reference
            m_regType.m_byRef = true;
            return true;
        }
        else
            return false;
    }

    // Can convert to basic types.
    // For non basic types, all we can do is verify that the register contains
    // the type that we expect, and raise a compiler error otherwise.
    if (type.IsBasic ())                    return CompileConvert (type.m_basicType);
    else if (m_regType.ExactEquals (type))  return true;        // Note: Exact equals is required as == will say that pointers are equal to references.
                                                                // (Internally this is true, but we want to enforce that programs use the correct type.)

    SetError ("Cannot convert to " + m_vm.DataTypes ().DescribeVariable ("", type));
    return false;
}

bool TomBasicCompiler::CompileConvert2 (vmValType type) {

    // Can convert NULL to a different pointer type
    if (m_reg2Type.IsNull ()) {
        if (type.VirtualPointerLevel () <= 0) {
            SetError ("Cannot convert NULL to " + m_vm.DataTypes ().DescribeVariable ("", type));
            return false;
        }

        // No generated code necessary, just substitute in type
        m_reg2Type = type;
        return true;
    }

    // Can convert to basic types.
    // For non basic types, all we can do is verify that the register contains
    // the type that we expect, and raise a compiler error otherwise.
    if (type.IsBasic ())            return CompileConvert2 (type.m_basicType);
    else if (m_reg2Type.ExactEquals (type))   return true;      // Note: Exact equals is required as == will say that pointers are equal to references.
                                                                // (Internally this is true, but we want to enforce that programs use the correct type.)

    SetError ("Cannot convert to " + m_vm.DataTypes ().DescribeVariable ("", type));
    return false;
}

bool TomBasicCompiler::CompileConstant () {

    // Skip CONST
    if (!GetToken ())
        return false;

    // Expect at least one field in dim
    if (AtSeparatorOrSpecial ()) {
        SetError ("Expected constant declaration");
        return false;
    }

    // Parse fields in dim
    bool needComma = false;             // First element doesn't need a comma
    while (!AtSeparatorOrSpecial ()) {

        // Handle commas
        if (needComma) {
            if (m_token.m_text != ",") {
                SetError ("Expected ','");
                return false;
            }
            if (!GetToken ())
                return false;
        }
        needComma = true;               // Remaining elements do need commas

        // Read constant name
        if (!m_token.m_type == CTT_TEXT) {
            SetError ("Expected constant name");
            return false;
        }
        std::string name = m_token.m_text;
        if (m_programConstants.find (name) != m_programConstants.end ()) {
            SetError ("'" + name + "' has already been declared as a constant.");
            return false;
        }
        if (!CheckName (name))
            return false;
        if (!GetToken ())
            return false;

        // Determine constant type from last character of constant name
        vmBasicValType type = VTP_INT;
        if (name.length () > 0) {
            char last = name [name.length () - 1];
            if (last == '$')        type = VTP_STRING;
            else if (last == '#')   type = VTP_REAL;
            else if (last == '%')   type = VTP_INT;
        }
        // Expect =
        if (m_token.m_text != "=") {
            SetError ("Expected '='");
            return false;
        }
        if (!GetToken ())
            return false;

        // Compile constant expression
        vmValue value;
        std::string stringValue;
        if (!EvaluateConstantExpression (type, value, stringValue))
            return false;
        switch (type) {
        case VTP_INT:       m_programConstants [name] = compConstant (value.IntVal ());     break;
        case VTP_REAL:      m_programConstants [name] = compConstant (value.RealVal ());    break;
        case VTP_STRING:    m_programConstants [name] = compConstant ((std::string) ("S" + stringValue));         break;
        };

/*        // Expect constant value
        if (m_token.m_type != CTT_CONSTANT) {
            SetError ("Expected constant value");
            return false;
        }

        // Validate constant type matches that expected
        if (type == VTP_STRING && m_token.m_valType != VTP_STRING) {
            SetError ("String constant expected");
            return false;
        }
        else if (type != VTP_STRING && m_token.m_valType == VTP_STRING) {
            SetError ("Number constant expected");
            return false;
        }

        // Register constant
        switch (m_token.m_valType) {
        case VTP_INT:       m_programConstants [name] = compConstant (StringToInt  (m_token.m_text));   break;
        case VTP_REAL:      m_programConstants [name] = compConstant (StringToReal (m_token.m_text));   break;
        case VTP_STRING:    m_programConstants [name] = compConstant (m_token.m_text);                  break;
        };

        // Skip constant value
        if (!GetToken ())
            return false;*/
    }
    return true;
}

bool TomBasicCompiler::CompileFreeTempData    () {

    // Add instruction to free temp data (if necessary)
    if (m_freeTempData)
        AddInstruction (OP_FREE_TEMP, VTP_INT, vmValue ());
    m_freeTempData = false;

    return true;
}

bool TomBasicCompiler::CompileExtendedUnOperation (vmOpCode oper) {

    vmValType   type;
    int         opFunc;
    bool        freeTempData;
    vmValType   resultType;
    bool        found = false;

    // Iterate through external operator extension functions until we find
    // one that can handle our data.
    for (int i = 0; i < m_unOperExts.size () && !found; i++) {

        // Setup input data
        type            = m_regType;
        opFunc          = -1;
        freeTempData    = false;
        resultType      = vmValType ();

        // Call function
        found = m_unOperExts [i] (type, oper, opFunc, resultType, freeTempData);
    }

    if (!found)                 // No handler found.
        return false;           // This is not an error, but operation must be
                                // passed through to default operator handling.

    // Generate code to convert operands as necessary
    bool conv = CompileConvert  (type);
    assert (conv);

    // Generate code to call external operator function
    assert (opFunc >= 0);
    assert (opFunc < m_vm.OperatorFunctionCount ());
    AddInstruction (OP_CALL_OPERATOR_FUNC, VTP_INT, vmValue (opFunc));

    // Set register to result type
    m_regType = resultType;

    // Record whether we need to free temp data
    m_freeTempData = m_freeTempData || freeTempData;

    return true;
}

bool TomBasicCompiler::CompileExtendedBinOperation (vmOpCode oper) {

    vmValType   type1, type2;
    int         opFunc;
    bool        freeTempData;
    vmValType   resultType;
    bool        found = false;

    // Iterate through external operator extension functions until we find
    // one that can handle our data.
    for (int i = 0; i < m_binOperExts.size () && !found; i++) {

        // Setup input data
        type1           = m_regType;
        type2           = m_reg2Type;
        opFunc          = -1;
        freeTempData    = false;
        resultType      = vmValType ();

        // Call function
        found = m_binOperExts [i] (type1, type2, oper, opFunc, resultType, freeTempData);
    }

    if (!found)                 // No handler found.
        return false;           // This is not an error, but operation must be
                                // passed through to default operator handling.

    // Generate code to convert operands as necessary
    bool conv1 = CompileConvert  (type1);
    bool conv2 = CompileConvert2 (type2);
    assert (conv1);
    assert (conv2);

    // Generate code to call external operator function
    assert (opFunc >= 0);
    assert (opFunc < m_vm.OperatorFunctionCount ());
    AddInstruction (OP_CALL_OPERATOR_FUNC, VTP_INT, vmValue (opFunc));

    // Set register to result type
    m_regType = resultType;

    // Record whether we need to free temp data
    m_freeTempData = m_freeTempData || freeTempData;

    return true;
}

std::string TomBasicCompiler::FunctionName (int index) {
    for (   compFuncIndex::iterator i = m_functionIndex.begin ();
            i != m_functionIndex.end ();
            i++)
        if ((*i).second == index)
            return (*i).first;
    return "???";
}

bool TomBasicCompiler::CompileAlloc () {

    // Skip "alloc"
    if (!GetToken ())
        return false;

    // Expect &pointer variable
    if (m_token.m_text == "&") {
        SetError ("First argument must be a pointer");
        return false;
    }

    // Load pointer
    if (!(CompileLoadVar () && CompileTakeAddress ()))
        return false;

    // Store pointer type
    vmValType   ptrType     = m_regType,
                dataType    = m_regType;
    dataType.m_byRef = false;
    dataType.m_pointerLevel--;

    // Get pointer address
    if (!CompileTakeAddress ()) {
        SetError ("First argument must be a pointer");
        return false;
    }

    // Push destination address to stack
    if (!CompilePush ())
        return false;

    // Generate code to push array dimensions (if any) to the stack.
    int i;
    for (i = 0; i < dataType.m_arrayLevel; i++) {

        // Expect ,
        if (m_token.m_text != ",") {
            SetError ("Expected ','");
            return false;
        }
        if (!GetToken ())
            return false;

        // Generate code to evaluate array index, and convert to an integer.
        if (!CompileExpression ())
            return false;
        if (!CompileConvert (VTP_INT)) {
            SetError ("Array index must be a number. " + m_vm.DataTypes ().DescribeVariable ("", m_regType) + " is not a number");
            return false;
        }

        // Push array index to stack
        if (!CompilePush ())
            return false;
    }

    // Add alloc instruction
    AddInstruction (OP_ALLOC, VTP_INT, vmValue ((vmInt) m_vm.StoreType (dataType)));

    // Instruction automatically removes all array indices that were pushed to
    // the stack.
    for (i = 0; i < dataType.m_arrayLevel; i++)
        m_operandStack.pop_back ();

    // Instruction also automatically leaves the register pointing to the new
    // data.
    m_regType = ptrType;
    m_regType.m_byRef = false;

    // Generate code to pop destination address
    if (!CompilePop ())
        return false;

    // Generate code to save address to pointer
    AddInstruction (OP_SAVE, VTP_INT, vmValue ());

    return true;
}

compParserPos TomBasicCompiler::SavePos () {

    // Save the current parser position, so we can return to it later.
    compParserPos pos;
    pos.m_line  = m_parser.Line ();
    pos.m_col   = m_parser.Col ();
    pos.m_token = m_token;
    return pos;
}

void TomBasicCompiler::RestorePos (compParserPos& pos) {

    // Restore parser position
    m_parser.SetPos (pos.m_line, pos.m_col);
    m_token = pos.m_token;
}

std::string TomBasicCompiler::DescribeStackCall (unsigned int returnAddr) {

    // Return a string describing the gosub call
    if (returnAddr == 0 || returnAddr >= m_vm.InstructionCount ())
        return "???";

    // Look at instruction immediately before return address.
    // This should be the gosub
    if (m_vm.Instruction (returnAddr - 1).m_opCode != OP_CALL)
        return "???";

    // Get target address
    unsigned int target = m_vm.Instruction (returnAddr - 1).m_value.IntVal ();

    // Lookup label name
    compLabelIndex::iterator i = m_labelIndex.find (target);
    if (i == m_labelIndex.end ())
        return "???";

    // Return label name
    return (*i).second;
}

bool TomBasicCompiler::TempCompileExpression (std::string expression, vmValType& valType) {
    // Returns start address to execute
    // or 0xffff if error.

    // Load expression into parser
    m_parser.SourceCode ().clear ();
    m_parser.SourceCode ().push_back (expression);
    m_parser.Reset ();
    m_lastLine = 0;
    m_lastCol  = 0;

    // Reset compiler state
    ClearState ();

    // Clear error state
    ClearError ();
    m_parser.ClearError ();

    // Read first token
    if (!GetToken (true))
        return false;

    // Compile code
    if (!CompileExpression ())
        return false;

    if (m_token.m_type != CTT_EOL) {
        SetError ("Extra characters after expression");
        return false;
    }

    // Terminate program
    AddInstruction (OP_END, VTP_INT, vmValue ());

    // Return expression result type
    valType = m_regType;
    return true;
}

bool TomBasicCompiler::CompileData () {

    // Skip "data"
    if (!GetToken (false, true))            // Use "DATA" mode read
        return false;

    // Compile data elements
    bool needComma = false;
    do {

        // Handle commas
        if (needComma) {
            if (m_token.m_text != ",") {
                SetError ("Expected ','");
                return false;
            }
            if (!GetToken (false, true))
                return false;
        }
        needComma = true;               // Remaining elements do need commas

        // Consecutive commas?
        if (m_token.m_text == "," || AtSeparatorOrSpecial ()) {

            // Store a blank string
            m_vm.StoreProgramData (VTP_STRING, vmValue (0));
        }
        else {

            // Extract value
            vmValue v;
            if (m_token.m_valType == VTP_STRING) {

                // Allocate new string constant
                std::string text = m_token.m_text.substr (1, m_token.m_text.length () - 1);     // Remove S prefix
                v.IntVal () = m_vm.StoreStringConstant (text);
            }
            else if (m_token.m_valType == VTP_INT)
                v.IntVal () = StringToInt (m_token.m_text);
            else
                v.RealVal () = StringToReal (m_token.m_text);

            // Store data in VM
            m_vm.StoreProgramData (m_token.m_valType, v);

            // Next token
            if (!GetToken ())
                return false;
        }
    } while (!AtSeparatorOrSpecial ());
    return true;
}

bool TomBasicCompiler::CompileDataRead () {

    // Skip "read"
    if (!GetToken ())
        return false;

    // Expect at one variable name
    if (AtSeparatorOrSpecial ()) {
        SetError ("Expected variable name");
        return false;
    }

    // Parse fields in dim
    bool needComma = false;             // First element doesn't need a comma
    while (!AtSeparatorOrSpecial ()) {

        // Handle commas
        if (needComma) {
            if (m_token.m_text != ",") {
                SetError ("Expected ','");
                return false;
            }
            if (!GetToken ())
                return false;
        }
        needComma = true;               // Remaining elements do need commas

        // Generate code to load target variable address
        if (!CompileLoadVar ())
            return false;

        // Must be a basic type.
        vmValType type = m_regType;
        if (!type.IsBasic ()) {
            SetError ("Can only READ built in types (int, real or string)");
            return false;
        }

        // Convert load target variable into take address of target variable
        if (!CompileTakeAddress ()) {
            SetError ("Value cannot be READ into");
            return false;
        }

        if (!CompilePush ())
            return false;

        // Generate READ op-code
        AddInstruction (OP_DATA_READ, type.m_basicType, vmValue ());

        if (!CompilePop ())
            return false;

        // Save reg into [reg2]
        AddInstruction (OP_SAVE, m_reg2Type.m_basicType, vmValue ());
    }
    return true;
}

bool TomBasicCompiler::CompileDataReset () {

    // Skip "reset"
    if (!GetToken ())
        return false;

    // If label specified, use offset stored in label
    if (!AtSeparatorOrSpecial ()) {

        // Validate label
        if (m_token.m_type != CTT_TEXT) {
            SetError ("Expected label name");
            return false;
        }

        // Record reset, so that we can fix up the offset in the second compile pass.
        m_resets.push_back (compJump (m_vm.InstructionCount (), m_token.m_text));

        // Skip label name
        if (!GetToken ())
            return false;
    }

    // Add jump instruction
    AddInstruction (OP_DATA_RESET, VTP_INT, vmValue (0));

    return true;
}

bool TomBasicCompiler::EvaluateConstantExpression (vmBasicValType& type, vmValue& result, std::string& stringResult) {

    // Note: If type is passed in as VTP_UNDEFINED, then any constant expression
    // will be accepted, and type will be set to the type that was found.
    // Otherwise it will only accept expressions that can be cast to the specified type.
    // If the expression is a string, its value will be returned in stringResult.
    // Otherwise its value will be returned in result.

    // Mark the current size of the program. This is where the expression will start
    int expressionStart = m_vm.InstructionCount ();

    // Compile expression, specifying that it must be constant
    if (!CompileExpression (true))
        return false;

    // Convert to required type
    if (type != VTP_UNDEFINED)
        if (!CompileConvert (type))
            return false;

    // Add "end program" opcode, so we can safely evaluate it
    AddInstruction (OP_END, VTP_INT, vmValue ());

    // Setup virtual machine to execute expression
    // Note: Expressions can't branch or loop, and it's very difficult to write
    // one that evaluates to a large number of op-codes. Therefore we won't worry
    // about processing windows messages or checking for pause state etc.
    m_vm.ClearError ();
    m_vm.GotoInstruction (expressionStart);
    try {
        do {
            m_vm.Continue (1000);
        } while (!m_vm.Error () && !m_vm.Done ());
    }
    catch (...) {
        SetError ("Error evaluating constant expression");
        return false;
    }
    if (m_vm.Error ()) {
        SetError ("Error evaluating constant expression");
        return false;
    }

    // Now we have the result type of the constant expression,
    // AND the virtual machine has its value stored in the register.

    // Roll back all the expression op-codes
    m_vm.GotoInstruction (0);               
    m_vm.RollbackProgram (expressionStart);

    // Set return values
    type = m_regType.m_basicType;
    if (type == VTP_STRING) stringResult    = m_vm.RegString ();
    else                    result          = m_vm.Reg ();

    return true;
}

bool TomBasicCompiler::CompileConstantExpression (vmBasicValType type) {

    // Evaluate constant expression
    vmValue value;
    std::string stringValue = "";
    if (!EvaluateConstantExpression (type, value, stringValue))
        return false;

    // Generate "load constant" instruction
    if (type == VTP_STRING) {

        // Create string constant entry if necessary
        int index = m_vm.StoreStringConstant (stringValue);
        AddInstruction (OP_LOAD_CONST, VTP_STRING, vmValue (index));
    }
    else
        AddInstruction (OP_LOAD_CONST, type, value);
        
    return true;
}

compFuncSpec *TomBasicCompiler::FindFunction(std::string name, int paramCount) {

    // Search for function with matching name & param count
    compFuncIndex::iterator it;
    for (   it = m_functionIndex.find (name);
            it != m_functionIndex.end () && (*it).first == name;
            it++) {
        compFuncSpec& spec = m_functions[(*it).second];
        if (spec.m_paramTypes.Params().size() == paramCount)
            return &spec;
    }

    // None found
    SetError((std::string) "'" + name + "' function not found");
    return NULL;
}

bool TomBasicCompiler::CompilePrint (bool forceNewline) {

    // The print function has a special syntax, and must be compiled separtely

    // Skip "print"
    if (!GetToken())
        return false;

    bool foundSemiColon = false;
    int operandCount = 0;
    while (!AtSeparatorOrSpecial()) {

        // Look for semicolon
        if (m_token.m_text == ";") {

            // Record it, and move on to next
            foundSemiColon = true;
            if (!GetToken())
                return false;
        }
        else {
            foundSemiColon = false;

            // If this is not the first operand, then there will be a string
            // sitting in the register. Need to push it first.
            if (operandCount > 0)
                if (!CompilePush())
                    return false;

            // Evaluate expression & convert it to string
            if (!(CompileExpression() && CompileConvert(VTP_STRING)))
                return false;

            operandCount++;
        }
    }

    // Add all operands together
    while (operandCount > 1) {
        if (!CompilePop())
            return false;
        AddInstruction(OP_OP_PLUS, VTP_STRING, vmValue());
        m_regType = VTP_STRING;

        operandCount--;
    }

    // Push string as function parameter
    if (operandCount == 1)
        if (!CompilePush())
            return false;

    // Find print/printr function
    bool newLine = forceNewline ||
        ((m_syntax == LS_TRADITIONAL || m_syntax == LS_TRADITIONAL_PRINT) && !foundSemiColon);

    if (!newLine && operandCount == 0)                      // Nothing to print?
        return true;                                        // Do nothing!

    compFuncSpec *spec = FindFunction(newLine ? "printr" : "print", operandCount);
    if (spec == NULL)
        return false;

    // Generate code to call it
    AddInstruction (OP_CALL_FUNC, VTP_INT, vmValue(spec->m_index));

    // Generate code to clean up stack
    if (operandCount == 1)
        if (!CompilePop ())
            return false;

    return true;
}

bool TomBasicCompiler::CompileInput () {

    // Input also has a special syntax.
    // This still isn't a complete input implementation, as it doesn't support
    // inputting multiple values on one line.
    // But it's a lot better than before.

    // Skip "input"
    if (!GetToken())
        return false;

    // Check for prompt
    if (m_token.m_type == CTT_CONSTANT && m_token.m_valType == VTP_STRING) {

        // Allocate new string constant
        std::string text = m_token.m_text.substr (1, m_token.m_text.length () - 1);     // Remove S prefix

        if (!GetToken())
            return false;          

        // Expect , or ;
        if (m_token.m_text == ";")
            text = text + "? ";
        else if (m_token.m_text != ",") {
            SetError("Expected ',' or ';'");
            return false;
        }
        if (!GetToken())
            return false;

        // Create new string constant
        int index = m_vm.StoreStringConstant (text);

        // Generate code to print it (load, push, call "print" function)
        AddInstruction (OP_LOAD_CONST, VTP_STRING, vmValue (index));
        m_regType = VTP_STRING;
        if (!CompilePush())
            return false;

        // Generate code to call "print" function
        compFuncSpec *printSpec = FindFunction("print", 1);
        if (printSpec == NULL)
            return false;
        AddInstruction (OP_CALL_FUNC, VTP_INT, vmValue(printSpec->m_index));

        // Generate code to clean up stack
        if (!CompilePop ())
            return false;
    }

    // Generate code to effectively perform
    //      variable = Input$()
    // or
    //      variable = val(Input$())
    //
    // (Depending on whether variable is a string)

    // Generate code to load target variable
    if (!CompileLoadVar ())
        return false;

    // Must be a simple variable
    if (!m_regType.IsBasic()) {
        SetError("Input variable must be a basic string, integer or real type");
        return false;
    }
    vmBasicValType variableType = m_regType.m_basicType;

    // Generate code to push its address to stack
    if (!(CompileTakeAddress() && CompilePush()))
        return false;

    // Generate code to call "input$()" function
    compFuncSpec *inputSpec = FindFunction("input$", 0);
    if (inputSpec == NULL)
        return false;
    AddInstruction (OP_CALL_FUNC, VTP_INT, vmValue(inputSpec->m_index));
    AddInstruction (OP_TIMESHARE, VTP_INT, vmValue ());         // Timesharing break is necessary
    m_regType = VTP_STRING;

    // If the variable is not a string, then we need to convert it to the target
    // type. We do this by inserting an implicit call to the val() function.
    if (variableType != VTP_STRING) {

        // Push register back as input to val function
        if (!CompilePush())
            return false;

        // Generate code to call "val()" function
        compFuncSpec *valSpec = FindFunction("val", 1);
        if (valSpec == NULL)
            return false;
        AddInstruction (OP_CALL_FUNC, VTP_INT, vmValue(valSpec->m_index));
        m_regType = VTP_REAL;

        // Clean up stack
        if (!CompilePop())
            return false;
    }

    // Generate code to pop target address into reg2
    if (!CompilePop ())
        return false;

    if (!CompileConvert (m_reg2Type.m_basicType)) {
        SetError ("Types do not match");        // Technically this should never actually happen
        return false;
    }

    // Generate code to save value
    AddInstruction (OP_SAVE, m_reg2Type.m_basicType, vmValue ());

    return true;
}

bool TomBasicCompiler::CompileLanguage () {

    // Compile language directive
    // Skip "language"
    if (!GetToken())
        return false;

    // Expect syntax type
            if (m_token.m_text == "traditional")        m_syntax = LS_TRADITIONAL;
    else    if (m_token.m_text == "basic4gl")           m_syntax = LS_BASIC4GL;
    else    if (m_token.m_text == "traditional_print")  m_syntax = LS_TRADITIONAL_PRINT;
    else {
        SetError ("Expected 'traditional', 'basic4gl' or 'traditional_print'");
        return false;
    }

    // Skip syntax token
    if (!GetToken())
        return false;

    return true;
}

bool TomBasicCompiler::CompileDo() {

    // Save loop position
    int loopPos = m_vm.InstructionCount ();

    // Skip "do"
    int line = m_parser.Line (), col = m_parser.Col ();
    if (!GetToken ())
        return false;

    // Look for "while" or "until"
    if (m_token.m_text == "while" || m_token.m_text == "until") {

        // Is this a negative condition?
        bool negative = m_token.m_text == "until";

        // Skip "while" or "until"
        if (!GetToken())
            return false;

        // Generate code to evaluate expression
        if (!CompileExpression ())
            return false;

        // Generate code to convert to integer
        if (!CompileConvert (VTP_INT))
            return false;

        // Free any temporary data expression may have created
        if (!CompileFreeTempData ())
            return false;

        // Create flow control structure
        m_flowControl.push_back (compFlowControl (FCT_DO_PRE, m_vm.InstructionCount (), loopPos, line, col));

        // Create conditional jump
        AddInstruction (negative ? OP_JUMP_TRUE : OP_JUMP_FALSE, VTP_INT, vmValue (0));

        // Done
        return true;
    }
    else {

        // Post condition DO.
        // Create flow control structure
        m_flowControl.push_back (compFlowControl (FCT_DO_POST, m_vm.InstructionCount (), loopPos, line, col));
        return true;
    }
}

bool TomBasicCompiler::CompileLoop() {

    if (!(FlowControlTopIs(FCT_DO_PRE) || FlowControlTopIs(FCT_DO_POST))) {
        SetError("'loop' without 'do'");
        return false;
    }

    // Find DO details
    compFlowControl top = FlowControlTOS ();
    m_flowControl.pop_back ();

    // Skip "DO"
    if (!GetToken())
        return false;

    // Look for "while" or "until"
    if (m_token.m_text == "while" || m_token.m_text == "until") {

        // This must be a post condition "do"
        if (top.m_type != FCT_DO_POST) {
            SetError("'until' or 'while' condition has already been specified for this 'do'");
            return false;
        }

        // Is this a negative condition?
        bool negative = m_token.m_text == "until";

        // Skip "while" or "until"
        if (!GetToken())
            return false;

        // Generate code to evaluate expression
        if (!CompileExpression ())
            return false;

        // Generate code to convert to integer
        if (!CompileConvert (VTP_INT))
            return false;

        // Free any temporary data expression may have created
        if (!CompileFreeTempData ())
            return false;

        // Create conditional jump back to "do"
        AddInstruction (negative ? OP_JUMP_FALSE : OP_JUMP_TRUE, VTP_INT, vmValue (top.m_jumpLoop));

        // Done
        return true;
    }
    else {

        // Jump unconditionally back to "do"
        AddInstruction (OP_JUMP, VTP_INT, vmValue(top.m_jumpLoop));

        // If this is a precondition "do", fixup the jump around the "do" block
        if (top.m_type == FCT_DO_PRE) {
            assert (top.m_jumpOut < m_vm.InstructionCount ());
            m_vm.Instruction (top.m_jumpOut).m_value.IntVal () = m_vm.InstructionCount ();
        }

        // Done
        return true;
    }
}
