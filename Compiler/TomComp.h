//---------------------------------------------------------------------------
/*  Created 5-Sep-2003: Thomas Mulgrew

    Used to compile source code in BASIC language to TomVM Op codes.
*/

#ifndef TomCompH                                         
#define TomCompH
//---------------------------------------------------------------------------

#include "../VM/TomVM.h"
#include "compParse.h"
#include "compFunction.h"
#include <map>
#include <set>

#define TC_STEPSBETWEENREFRESH 1000
#define TC_MAXOVERLOADEDFUNCTIONS 256           // Allow 256 functions of the same name (should be more than enough for anything...)

////////////////////////////////////////////////////////////////////////////////
// Internal compiler types

// compOperator
// Used for tracking which operators are about to be applied to operands.
// Basic4GL converts infix expressions into reverse polish using an operator
// stack and an operand stack.

enum compOpType {
    OT_OPERATOR,
    OT_RETURNBOOLOPERATOR,
    OT_BOOLOPERATOR,
    OT_LBRACKET,
    OT_STOP                         // Forces expression evaluation to stop
};

struct compOperator {
    compOpType  m_type;
    vmOpCode    m_opCode;
    int         m_params;           // 1 -> Calculate "op Reg"          (e.g. "Not Reg")
                                    // 2 -> Calculate "Reg2 op Reg"     (e.g. "Reg2 - Reg")
    int         m_binding;          // Operator binding. Higher = tighter.


    compOperator (compOpType type, vmOpCode opCode, int params, int binding)
        : m_type (type), m_opCode (opCode), m_params (params), m_binding (binding) { ; }
    compOperator ()
        : m_type (OT_OPERATOR), m_opCode (OP_NOP), m_params (0), m_binding (0) { ; }
    compOperator (const compOperator& o)
        : m_type (o.m_type), m_opCode (o.m_opCode), m_params (o.m_params), m_binding (o.m_binding) { ; }
};

// CompLabel
// A program label, i.e. a named destination for "goto" and "gosub"s
struct compLabel {
    unsigned int m_offset;              // Instruction index in code
    unsigned int m_programDataOffset;   // Program data offset. (For use with "RESET labelname" command.)

    compLabel (unsigned int offset, int dataOffset)
        : m_offset (offset), m_programDataOffset (dataOffset) { ; }
    compLabel ()
        : m_offset (0), m_programDataOffset (0) { ; }
};

// compJump
// Used to track program jumps. Actuall addresses are patched into jump
// instructions after the main compilation pass has completed. (Thus forward
// jumps are possible.)
struct compJump {
    unsigned int m_jumpInstruction;     // Instruction containing jump instruction
    std::string m_labelName;            // Label to which we are jumping

    compJump (unsigned int instruction, std::string& labelName)
        : m_jumpInstruction (instruction), m_labelName (labelName) { ; }
    compJump ()
        : m_jumpInstruction (0), m_labelName ("") { ; }
};

// compFlowControl
// Used to maintain a stack of open flow control structures, so that when an
// "endif", "else", "next" or "wend" is found we know what it corresponds to.
enum compFlowControlType {
    FCT_IF = 0,
    FCT_ELSE,
    FCT_FOR,
    FCT_WHILE,
    FCT_DO_PRE,         // Do with a pre-condition
    FCT_DO_POST};       // Do with a post-condition

struct compFlowControl {
    compFlowControlType m_type;                         // Type of flow control construct
    int                 m_jumpOut;                      // Index of instruction that jumps past (out) of flow control construct
    int                 m_jumpLoop;                     // Instruction to jump to to loop
    int                 m_sourceLine, m_sourceCol;      // Source offset for error reporting
    std::string         m_data;                         // Misc data
    bool                m_impliedEndif;                 // If/elseif/else only. True if there is an implied endif after the explict endif
    bool                m_blockIf;                      // True if is a block if. Block ifs require "endifs". Non-block ifs have an implicit endif at the end of the line

    compFlowControl (compFlowControlType type, int jumpOut, int jumpLoop, int line, int col, bool impliedEndif = false, const std::string& data = (std::string) "", bool blockIf = false)
        :   m_type (type),
            m_jumpOut (jumpOut),
            m_jumpLoop (jumpLoop),
            m_sourceLine (line),
            m_sourceCol (col),
            m_impliedEndif (impliedEndif),
            m_data (data),
            m_blockIf (blockIf) { ; }
    compFlowControl ()
        :   m_type ((compFlowControlType) 0),
            m_jumpOut (0),
            m_jumpLoop (0),
            m_sourceLine (0),
            m_sourceCol (0),
            m_impliedEndif (false),
            m_data ("") { ; }
};

// compConstant
//
// Recognised constants (e.g. "true", "false")

struct compConstant {
    vmBasicValType  m_valType;          // Value type
    vmInt           m_intVal;           // Value
    vmReal          m_realVal;
    vmString        m_stringVal;

    compConstant ()
        :   m_valType (VTP_STRING),
            m_stringVal (""),
            m_intVal (0),
            m_realVal (0) { ; }
    compConstant (const std::string& s)
        :   m_valType (VTP_STRING),
            m_stringVal (s),
            m_intVal (0),
            m_realVal (0) { ; }
    compConstant (int i)
        :   m_valType (VTP_INT),
            m_intVal (i),
            m_stringVal (""),
            m_realVal (0) { ; }
    compConstant (unsigned int i)
        :   m_valType (VTP_INT),
            m_intVal (i),
            m_stringVal (""),
            m_realVal (0) { ; }
    compConstant (float r)
        :   m_valType (VTP_REAL),
            m_realVal (r),
            m_stringVal (""),
            m_intVal (0) { ; }
    compConstant (double r)
        :   m_valType (VTP_REAL),
            m_realVal (r),
            m_stringVal (""),
            m_intVal (0) { ; }
    compConstant (const compConstant& c)
        :   m_valType (c.m_valType),
            m_realVal (c.m_realVal),
            m_intVal (c.m_intVal),
            m_stringVal (c.m_stringVal) { ; }
};

// Language extension: Operator overloading
//
typedef bool (*compUnOperExt)(  vmValType& regType,     // IN: Current type in register.                                                        OUT: Required type cast before calling function
                                vmOpCode oper,          // IN: Operator being applied
                                int& operFunction,      // OUT: Index of VM_CALL_OPERATOR_FUNC function to call
                                vmValType& resultType,  // OUT: Resulting value type
                                bool& freeTempData);    // OUT: Set to true if temp data needs to be freed
typedef bool (*compBinOperExt)( vmValType& regType,     // IN: Current type in register.                                                        OUT: Required type cast before calling function
                                vmValType& reg2Type,    // IN: Current type in second register (operation is reg2 OP reg1, e.g reg2 + reg1):    OUT: Required type cast before calling function
                                vmOpCode oper,          // IN: Operator being applied
                                int& operFunction,      // OUT: Index of VM_CALL_OPERATOR_FUNC function to call
                                vmValType& resultType,  // OUT: Resulting value type
                                bool& freeTempData);    // OUT: Set to true if temp data needs to be freed

// Container types
typedef std::map<std::string,compOperator> compOperatorMap;
typedef std::set<std::string> compStringSet;
typedef std::map<std::string,compLabel> compLabelMap;
typedef std::map<unsigned int,std::string> compLabelIndex;          // Index offset to label
typedef std::map<std::string,compConstant> compConstantMap;
typedef std::vector<compFuncSpec> compFuncSpecArray;
typedef std::map<std::string,int> compStringIndex;
typedef std::multimap<std::string,int> compFuncIndex;

// Misc
struct compParserPos {
    unsigned int m_line;
    int m_col;
    compToken m_token;
};

enum compLanguageSyntax {
    LS_TRADITIONAL          = 0,    // As compatible as possible with other BASICs
    LS_BASIC4GL             = 1,    // Standard Basic4GL syntax for backwards compatibility with existing code.
    LS_TRADITIONAL_PRINT    = 2     // Traditional mode PRINT, but otherwise standard Basic4GL syntax
};

////////////////////////////////////////////////////////////////////////////////
// TomBasicCompiler
//
// Basic4GL v2 language compiler.

class TomBasicCompiler : public HasErrorState {

    // Virtual machine
    TomVM&                  m_vm;

    // Parser
    compParser              m_parser;

    // Settings
    bool                    m_caseSensitive;
    compOperatorMap         m_unaryOperators;       // Recognised operators. Unary have one operand (e.g NOT x)
    compOperatorMap         m_binaryOperators;      // Binary have to (e.g. x + y)
    compStringSet           m_reservedWords;
    compConstantMap         m_constants;            // Permanent constants.
    compConstantMap         m_programConstants;     // Constants declared using the const command.
    compFuncSpecArray       m_functions;
    compFuncIndex           m_functionIndex;        // Maps function name to index of function (in m_functions array)
    compLanguageSyntax      m_syntax;

    // Compiler state
    vmValType                       m_regType, m_reg2Type;
    std::vector<vmValType>          m_operandStack;
    std::vector<compOperator>       m_operatorStack;
    compOperator& OperatorTOS () { return m_operatorStack [m_operatorStack.size () - 1]; }
    compLabelMap                    m_labels;
    compLabelIndex                  m_labelIndex;
    std::vector<compJump>           m_jumps;        // Jumps to fix up
    std::vector<compJump>           m_resets;       // Resets to fix up
    std::vector<compFlowControl>    m_flowControl;  // Flow control structure stack
    compToken                       m_token;
    bool                            m_needColon;    // True if next instruction must be separated by a colon (or newline)
    bool                            m_freeTempData; // True if need to generate code to free temporary data before the next instruction
    unsigned int                    m_lastLine,
                                    m_lastCol;


    void ClearState ();
    bool GetToken (bool skipEOL = false, bool dataMode = false);
    bool CheckParser ();

    // Compilation
    void AddInstruction (vmOpCode opCode, vmBasicValType type, const vmValue& val);
    bool AtSeparator            ();
    bool AtSeparatorOrSpecial   ();
    bool SkipSeparators         ();
    bool CompileInstruction     ();
    bool CompileStructure       ();
    bool CompileDim             (bool forStruc);
    bool CompileDimField        (std::string& name, vmValType& type, bool forStruc);
    bool CompileLoadVar         ();
    bool CompileDeref           ();
    bool CompileDerefs          ();
    bool CompileDataLookup      (bool takeAddress);
    bool CompileExpression      (bool mustBeConstant = false);
    bool CompilePush            ();
    bool CompilePop             ();
    bool CompileConvert         (vmBasicValType type);
    bool CompileConvert2        (vmBasicValType type);
    bool CompileConvert         (vmValType type);
    bool CompileConvert2        (vmValType type);
    bool CompileTakeAddress     ();
    bool CompileAssignment      ();
    bool CompileLoad            ();
    bool CompileExpressionLoad  (bool mustBeConstant = false);
    bool CompileLoadConst       ();
    bool CompileOperation       ();
    bool CompileGoto            (vmOpCode jumpType = OP_JUMP);
    bool CompileIf              (bool elseif);
    bool CompileElse            (bool elseif);
    bool CompileEndIf           (bool automatic);
    bool CompileFor             ();
    bool CompileNext            ();
    bool CompileDo              ();
    bool CompileLoop            ();
    bool CompileWhile           ();
    bool CompileWend            ();
    bool CheckName              (std::string name);
    bool CompileFunction        (bool needResult = false);
    bool CompileConstant        ();
    bool CompileFreeTempData    ();
    bool CompileExtendedUnOperation     (vmOpCode oper);
    bool CompileExtendedBinOperation    (vmOpCode oper);
    bool CompileAlloc           ();
    bool CompileData            ();
    bool CompileDataRead        ();
    bool CompileDataReset       ();
    bool CompilePrint           (bool forceNewLine);
    bool CompileInput           ();
    bool CompileLanguage        ();

    bool EvaluateConstantExpression (vmBasicValType& type, vmValue& result, std::string& stringResult);
    bool CompileConstantExpression (vmBasicValType type = VTP_UNDEFINED);
    void InternalCompile        ();

    // Language extension
    std::vector<compUnOperExt>  m_unOperExts;       // Unary operator extensions
    std::vector<compBinOperExt> m_binOperExts;      // Binary operator extensions

    // Misc
    bool LabelExists (std::string& labelText) {
        return m_labels.find (labelText) != m_labels.end ();
    }
    compLabel& Label (std::string& labelText) {
        assert (LabelExists (labelText));
        return m_labels [labelText];
    }
    void AddLabel (std::string& labelText, const compLabel& label) {
        assert (!LabelExists (labelText));
        m_labels [labelText] = label;
        m_labelIndex [label.m_offset] = labelText;
    }
    compFlowControl& FlowControlTOS () {
        assert (!m_flowControl.empty ());
        return m_flowControl [m_flowControl.size () - 1];
    }
    bool FlowControlTopIs (compFlowControlType type) {
        return      !m_flowControl.empty ()
                &&  FlowControlTOS ().m_type == type;
    }
    compParserPos SavePos ();
    void RestorePos (compParserPos& pos);
    compFuncSpec *FindFunction(std::string name, int paramCount);
    bool NeedAutoEndif();

public:

    TomBasicCompiler (TomVM& vm, bool caseSensitive = false);

    TomVM&          VM ()           { return m_vm; }
    compParser&     Parser ()       { return m_parser; }
    bool            CaseSensitive (){ return m_caseSensitive; }

    bool Compile ();

    ////////////
    // Settings
    compLanguageSyntax Syntax() { return m_syntax; }

    //////////////////////
    // Language extension

    // Constants
    void AddConstant (std::string name, compConstant c)   { m_constants [LowerCase (name)] = c; }
    void AddConstant (std::string name, std::string s)    { AddConstant (name, compConstant (s)); }
    void AddConstant (std::string name, int i)            { AddConstant (name, compConstant (i)); }
    void AddConstant (std::string name, unsigned int i)   { AddConstant (name, compConstant (i)); }
    void AddConstant (std::string name, float r)          { AddConstant (name, compConstant (r)); }
    void AddConstant (std::string name, double r)         { AddConstant (name, compConstant (r)); }
    compConstantMap& Constants () { return m_constants; }

    // Functions
    bool IsFunction (std::string& name) {
        compFuncIndex::iterator i = m_functionIndex.find (LowerCase (name));
        return i != m_functionIndex.end () && (*i).first == LowerCase (name);
    }
    void AddFunction (  std::string         name,
                        vmFunction          func,
                        compParamTypeList   params,
                        bool                brackets,
                        bool                isFunction,
                        vmValType           returnType,
                        bool                timeshare = false,
                        bool                freeTempData = false);
    std::string FunctionName (int index);           // Find function name for function #. Used for debug reporting
    compFuncSpecArray& Functions ()     { return m_functions; }
    compFuncIndex& FunctionIndex ()     { return m_functionIndex; }

    // Language extension
    void AddUnOperExt  (compUnOperExt e)    { m_unOperExts.push_back (e); }
    void AddBinOperExt (compBinOperExt e)   { m_binOperExts.push_back (e); }

    // Language features (for context highlighting)
    bool IsReservedWord (std::string& text) {
        compStringSet::iterator si = m_reservedWords.find (text);
        return si != m_reservedWords.end ();
    }
    bool IsConstant (std::string& text) {
        compConstantMap::iterator si = m_constants.find (text);
        return si != m_constants.end ();
    }
    bool IsBinaryOperator (std::string& text) {
        compOperatorMap::iterator si = m_binaryOperators.find (text);
        return si != m_binaryOperators.end ();
    }
    bool IsUnaryOperator (std::string& text) {
        compOperatorMap::iterator si = m_unaryOperators.find (text);
        return si != m_unaryOperators.end ();
    }
    bool IsOperator (std::string& text) { return IsBinaryOperator (text) || IsUnaryOperator (text); }
    unsigned int Line () { return m_token.m_line; }
    unsigned int Col  () { return m_token.m_col; }

    // Debugging
    std::string DescribeStackCall (unsigned int returnAddr);
    bool TempCompileExpression (std::string expression, vmValType& valType);
};

#endif
