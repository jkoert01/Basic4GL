//---------------------------------------------------------------------------
/*  Created 22-Oct-2003: Thomas Mulgrew
    Copyright (C) Thomas Mulgrew

    Matrix and vector routines.
*/                                                                           


#pragma hdrstop

#include "TomTrigBasicLib.h"

//---------------------------------------------------------------------------

#ifndef _MSC_VER
//#include <mem.h>
#pragma package(smart_init)
#endif

////////////////////////////////////////////////////////////////////////////////
// Basic matrix creation functions
vmReal matrix [16];

inline void ReturnMatrix (TomVM& vm) {
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, matrix);
}

////////////////////////////////////////////////////////////////////////////////
// Read vector/matrix
vmReal v1 [4], v2 [4], m1 [16], m2 [16];

int ReadVec (TomVM& vm, int index, vmReal *v) {
    assert (v != NULL);

    // Read a 3D vector.
    // This can be a 2, 3 or 4 element vector, but will always be returned as a 4
    // element vector. (z = 0 & w = 1 if not specified.)
    int size = ArrayDimensionSize (vm.Data (), index, 0);
    if (size < 2 || size > 4) {
        vm.FunctionError ("Vector must be 2, 3 or 4 element vector");
        return -1;                  // -1 = error
    }

    // Read in vector and convert to 4 element format
    v [2] = 0;
    v [3] = 1;
    ReadArray (vm.Data (), index, vmValType (VTP_REAL, 1, 1, true), v, size);

    // Return original size
    return size;
}

bool ReadMatrix (TomVM& vm, int index, vmReal *m) {
    assert (m != NULL);

    // Read 3D matrix.
    // Matrix must be 4x4
    if (ArrayDimensionSize (vm.Data (), index, 0) != 4
    ||  ArrayDimensionSize (vm.Data (), index, 1) != 4) {
        vm.FunctionError ("Matrix must be a 4x4 matrix (e.g 'dim matrix#(3)(3)' )");
        return false;
    }

    // Read in matrix
    ReadArray (vm.Data (), index, vmValType (VTP_REAL, 2, 1, true), m, 16);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Function wrappers

void WrapVec4 (TomVM& vm) {
    vmReal vec4 [4] = { vm.GetRealParam (4), vm.GetRealParam (3), vm.GetRealParam (2), vm.GetRealParam (1) };
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), 4, vec4);
}
void WrapVec3 (TomVM& vm) {
    vmReal vec3 [3] = { vm.GetRealParam (3), vm.GetRealParam (2), vm.GetRealParam (1) };
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), 3, vec3);
}
void WrapVec2 (TomVM& vm) {
    vmReal vec2 [2] = { vm.GetRealParam (2), vm.GetRealParam (1) };
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), 2, vec2);
}
void WrapMatrixZero (TomVM& vm) {
    ClearMatrix ();
    ReturnMatrix (vm);
}
void WrapMatrixIdentity (TomVM& vm) {
    Identity ();
    ReturnMatrix (vm);
}
void WrapMatrixScale (TomVM& vm) {
    Scale (vm.GetRealParam(1));
    ReturnMatrix (vm);
}
void WrapMatrixScale_2 (TomVM& vm) {
    Scale (vm.GetRealParam (3), vm.GetRealParam (2), vm.GetRealParam (1));
    ReturnMatrix (vm);
}
void WrapMatrixTranslate (TomVM& vm) {
    Translate (vm.GetRealParam (3), vm.GetRealParam (2), vm.GetRealParam (1));
    ReturnMatrix (vm);
}
void WrapMatrixRotateX (TomVM& vm) { RotateX (vm.GetRealParam (1)); ReturnMatrix (vm); }
void WrapMatrixRotateY (TomVM& vm) { RotateY (vm.GetRealParam (1)); ReturnMatrix (vm); }
void WrapMatrixRotateZ (TomVM& vm) { RotateZ (vm.GetRealParam (1)); ReturnMatrix (vm); }
void WrapMatrixBasis (TomVM& vm) {
    ClearMatrix ();
    matrix [15] = 1;
    ReadArray (vm.Data (), vm.GetIntParam (3), vmValType (VTP_REAL, 1, 1, true), &matrix [0], 4);
    ReadArray (vm.Data (), vm.GetIntParam (2), vmValType (VTP_REAL, 1, 1, true), &matrix [4], 4);
    ReadArray (vm.Data (), vm.GetIntParam (1), vmValType (VTP_REAL, 1, 1, true), &matrix [8], 4);
    ReturnMatrix (vm);
}
void WrapMatrixCrossProduct (TomVM& vm) {
    if (ReadVec (vm, vm.GetIntParam (1), v1) < 0)
        return;
    CrossProduct (v1);
    ReturnMatrix (vm);
}
void WrapCross (TomVM& vm) {

    // Fetch vectors
    int s1 = ReadVec (vm, vm.GetIntParam (2), v1),
        s2 = ReadVec (vm, vm.GetIntParam (1), v2);
    if (s1 < 0 || s2 < 0)
        return;

    // Calculate cross product vector
    vmReal result [4];
    CrossProduct (v1, v2, result);

    // Return resulting vector
    // (Vector will be the same length as the first source vector)
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), max (max (s1, s2), 3), result);
}
void WrapLength (TomVM& vm) {

    // Fetch vector
    if (ReadVec (vm, vm.GetIntParam (1), v1) < 0)
        return;

    // Calculate length
    vm.Reg ().RealVal () = Length (v1);
}
void WrapNormalize (TomVM& vm) {

    // Fetch vector
    int size = ReadVec (vm, vm.GetIntParam (1), v1);
    if (size < 0)
        return;

    // Normalize vector
    Normalize (v1);

    // Return resulting vector
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), size, v1);
}
void WrapDeterminant (TomVM& vm) {

    // Fetch matrix
    if (!ReadMatrix (vm, vm.GetIntParam (1), m1))
        return;

    // Return result
    vm.Reg ().RealVal () = Determinant (m1);
}
void WrapTranspose (TomVM& vm) {

    // Fetch matrix
    if (!ReadMatrix (vm, vm.GetIntParam (1), m1))
        return;

    // Transpose
    Transpose (m1, m2);

    // Create new matrix and assign to register
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, m2);
}
void WrapRTInvert (TomVM& vm) {

    // Fetch matrix
    if (!ReadMatrix (vm, vm.GetIntParam (1), m1))
        return;

    // RTInvert
    RTInvert (m1, m2);

    // Create new matrix and assign to register
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, m2);
}
void WrapOrthonormalize (TomVM& vm) {

    // Fetch matrix
    if (!ReadMatrix (vm, vm.GetIntParam (1), m1))
        return;

    // Orthonormalize
    Orthonormalize (m1);

    // Create new matrix and assign to register
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, m1);
}

////////////////////////////////////////////////////////////////////////////////
// Overloaded operators
void DoScaleVec (TomVM& vm, vmReal scale, int vecIndex) {

    // Extract data
    int size = ReadVec (vm, vecIndex, v1);
    if (size < 0)
        return;

    // Scale 3D vector
    Scale (v1, scale);

    // Return as temp vector (using original size)
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), size, v1);
}
void OpScaleVec (TomVM& vm) {
    DoScaleVec (vm, vm.Reg ().RealVal (), vm.Reg2 ().IntVal ());
}
void OpScaleVec2 (TomVM& vm) {
    DoScaleVec (vm, vm.Reg2 ().RealVal (), vm.Reg ().IntVal ());
}
void OpDivVec (TomVM& vm) {
    DoScaleVec (vm, 1.0 / vm.Reg ().RealVal (), vm.Reg2 ().IntVal ());
}
void DoScaleMatrix (TomVM& vm, vmReal scale, int matrixIndex) {

    // Read in matrix
    if (!ReadMatrix (vm, matrixIndex, m1))
        return;

    // Scale matrix
    ScaleMatrix (m1, scale);

    // Create new matrix and assign to register
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, m1);
}
void OpScaleMatrix (TomVM& vm) {
    DoScaleMatrix (vm, vm.Reg ().RealVal (), vm.Reg2 ().IntVal ());
}
void OpScaleMatrix2 (TomVM& vm) {
    DoScaleMatrix (vm, vm.Reg2 ().RealVal (), vm.Reg ().IntVal ());
}
void OpDivMatrix (TomVM& vm) {
    DoScaleMatrix (vm, 1.0 / vm.Reg ().RealVal (), vm.Reg2 ().IntVal ());
}
void OpMatrixVec (TomVM& vm) {

    // Matrix at reg2. Vector at reg.

    // Read in matrix
    if (!ReadMatrix (vm, vm.Reg2 ().IntVal (), m1))
        return;

    // Read in vector
    int size = ReadVec (vm, vm.Reg ().IntVal (), v1);
    if (size < 0)
        return;

    // Calculate resulting vector
    vmReal result [4];
    MatrixTimesVec (m1, v1, result);

    // Return as temporary vector
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), size, result);
}
void OpMatrixMatrix (TomVM& vm) {

    // Matrix * Matrix
    // Left matrix at reg2, right matrix at reg1
    if (!ReadMatrix (vm, vm.Reg2 ().IntVal (), m1)
    ||  !ReadMatrix (vm, vm.Reg  ().IntVal (), m2))
        return;

    // Multiply them out
    vmReal result [16];
    MatrixTimesMatrix (m1, m2, result);

    // Return as temporary matrix
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, result);
}
void OpVecVec (TomVM &vm) {

    // Vector * Vector = dot product

    // Fetch vectors
    if (ReadVec (vm, vm.Reg2 ().IntVal (), v1) < 0
    ||  ReadVec (vm, vm.Reg  ().IntVal (), v2) < 0)
        return;

    // Return result
    vm.Reg ().RealVal () = DotProduct (v1, v2);
}
void OpVecPlusVec (TomVM& vm) {

    // Fetch vectors
    int s1 = ReadVec (vm, vm.Reg2 ().IntVal (), v1),
        s2 = ReadVec (vm, vm.Reg  ().IntVal (), v2);
    if (s1 < 0 || s2 < 0)
        return;

    // Calculate result
    vmReal result [4];
    VecPlus (v1, v2, result);

    // Return as temporary vector
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), max (s1, s2), result);
}
void OpVecMinusVec (TomVM& vm) {

    // Fetch vectors
    int s1 = ReadVec (vm, vm.Reg2 ().IntVal (), v1),
        s2 = ReadVec (vm, vm.Reg  ().IntVal (), v2);
    if (s1 < 0 || s2 < 0)
        return;

    // Calculate result
    vmReal result [4];
    VecMinus (v1, v2, result);

    // Return as temporary vector
    vm.Reg ().IntVal () = FillTempRealArray (vm.Data (), vm.DataTypes (), max (s1, s2), result);
}
void OpMatrixPlusMatrix (TomVM& vm) {

    // Matrix + Matrix
    // Left matrix at reg2, right matrix at reg1
    if (!ReadMatrix (vm, vm.Reg2 ().IntVal (), m1)
    ||  !ReadMatrix (vm, vm.Reg  ().IntVal (), m2))
        return;

    // Add them
    vmReal result [16];
    MatrixPlus (m1, m2, result);

    // Return as temporary matrix
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, result);
}
void OpMatrixMinusMatrix (TomVM& vm) {

    // Matrix - Matrix
    // Left matrix at reg2, right matrix at reg1
    if (!ReadMatrix (vm, vm.Reg2 ().IntVal (), m1)
    ||  !ReadMatrix (vm, vm.Reg  ().IntVal (), m2))
        return;

    // Add them
    vmReal result [16];
    MatrixMinus (m1, m2, result);

    // Return as temporary matrix
    vm.Reg ().IntVal () = FillTempRealArray2D (vm.Data (), vm.DataTypes (), 4, 4, result);
}
void OpNegVec (TomVM& vm)       { DoScaleVec (vm, -1, vm.Reg ().IntVal ()); }
void OpNegMatrix (TomVM& vm)    { DoScaleMatrix (vm, -1, vm.Reg ().IntVal ()); }

// Indices
int scaleVec, scaleVec2, scaleMatrix, scaleMatrix2, matrixVec, matrixMatrix,
    divVec, divMatrix, vecVec, vecPlusVec, vecMinusVec,
    matrixPlusMatrix, matrixMinusMatrix, negVec, negMatrix;

// Compiler callback
bool TrigUnOperatorExtension (  vmValType& regType,     // IN: Current type in register.                                                        OUT: Required type cast before calling function
                                vmOpCode oper,          // IN: Operator being applied
                                int& operFunction,      // OUT: Index of VM_CALL_OPERATOR_FUNC function to call
                                vmValType& resultType,  // OUT: Resulting value type
                                bool& freeTempData) {   // OUT: Set to true if temp data needs to be freed

    // Must be real values, and not pointers (references are OK however)
    if (regType.VirtualPointerLevel () > 0 || regType.m_basicType != VTP_REAL)
        return false;

    if (oper == OP_OP_NEG) {
        if (regType.m_arrayLevel == 1) {                // -Vector
            operFunction    = negVec;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        if (regType.m_arrayLevel == 2) {                // -Matrix
            operFunction    = negMatrix;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
    }

    return false;
}
bool TrigBinOperatorExtension ( vmValType& regType,     // IN: Current type in register.                                                        OUT: Required type cast before calling function
                                vmValType& reg2Type,    // IN: Current type in second register (operation is reg2 OP reg1, e.g reg2 + reg1):    OUT: Required type cast before calling function
                                vmOpCode oper,          // IN: Operator being applied
                                int& operFunction,      // OUT: Index of VM_CALL_OPERATOR_FUNC function to call
                                vmValType& resultType,  // OUT: Resulting value type
                                bool& freeTempData) {   // OUT: Set to true if temp data needs to be freed

    // Pointers not accepted (references are OK however)
    if (regType.VirtualPointerLevel () > 0 || reg2Type.VirtualPointerLevel () > 0)
        return false;

    // Validate data types. We will only work with ints and reals
    if (regType.m_basicType != VTP_REAL && regType.m_basicType != VTP_INT)
        return false;
    if (reg2Type.m_basicType != VTP_REAL && reg2Type.m_basicType != VTP_INT)
        return false;

    // Is acceptible to have an integer value, but must be typecast to a real
    // Arrays of integers not acceptible
    if (regType.m_basicType == VTP_INT) {
        if (regType.IsBasic ())   regType.m_basicType = VTP_REAL;
        else                        return false;
    }
    if (reg2Type.m_basicType == VTP_INT) {
        if (reg2Type.IsBasic ())  reg2Type.m_basicType = VTP_REAL;
        else                        return false;
    }

    // Look for recognised combinations
    if (oper == OP_OP_TIMES) {
        if (regType.m_arrayLevel == 0 && reg2Type.m_arrayLevel == 1) {          // Vector * scalar
            operFunction    = scaleVec;
            resultType      = reg2Type;
            freeTempData    = true;
            return true;
        }
        else if (regType.m_arrayLevel == 1 && reg2Type.m_arrayLevel == 0) {     // Scalar * vector
            operFunction    = scaleVec2;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        if (regType.m_arrayLevel == 0 && reg2Type.m_arrayLevel == 2) {          // Vector * scalar
            operFunction    = scaleMatrix;
            resultType      = reg2Type;
            freeTempData    = true;
            return true;
        }
        else if (regType.m_arrayLevel == 2 && reg2Type.m_arrayLevel == 0) {     // Scalar * vector
            operFunction    = scaleMatrix2;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        else if (reg2Type.m_arrayLevel == 2 && regType.m_arrayLevel == 1) {     // Matrix * vector
            operFunction    = matrixVec;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        else if (reg2Type.m_arrayLevel == 2 && regType.m_arrayLevel == 2) {     // Matrix * matrix
            operFunction    = matrixMatrix;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        else if (regType.m_arrayLevel == 1 && reg2Type.m_arrayLevel == 1) {     // Vec * Vec (Dot product)
            operFunction    = vecVec;
            resultType      = VTP_REAL;
            freeTempData    = false;
            return true;
        }
        return false;
    }
    else if (oper == OP_OP_DIV) {
        if (regType.m_arrayLevel == 0 && reg2Type.m_arrayLevel == 1) {          // Vector / scalar
            operFunction    = divVec;
            resultType      = reg2Type;
            freeTempData    = true;
            return true;
        }
        if (regType.m_arrayLevel == 0 && reg2Type.m_arrayLevel == 2) {          // Matrix / scalar
            operFunction    = divMatrix;
            resultType      = reg2Type;
            freeTempData    = true;
            return true;
        }
    }
    else if (oper == OP_OP_PLUS) {
        if (regType.m_arrayLevel == 1 && reg2Type.m_arrayLevel == 1) {          // Vector + vector
            operFunction    = vecPlusVec;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        else if (regType.m_arrayLevel == 2 && reg2Type.m_arrayLevel == 2) {     // Matrix + matrix
            operFunction    = matrixPlusMatrix;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
    }
    else if (oper == OP_OP_MINUS) {
        if (regType.m_arrayLevel == 1 && reg2Type.m_arrayLevel == 1) {          // Vector - vector
            operFunction    = vecMinusVec;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
        else if (regType.m_arrayLevel == 2 && reg2Type.m_arrayLevel == 2) {     // Matrix - matrix
            operFunction    = matrixMinusMatrix;
            resultType      = regType;
            freeTempData    = true;
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Initialisation

void InitTomTrigBasicLib (TomBasicCompiler& comp) {

    /////////////////////
    // Initialise state

    ///////////////////////
    // Register constants

    ///////////////////////
    // Register functions
    // AGain we need a dummy noParam
    compParamTypeList noParam;
    comp.AddFunction ("Vec4",               WrapVec4,               compParamTypeList () << VTP_REAL << VTP_REAL << VTP_REAL << VTP_REAL,   true,   true,   vmValType (VTP_REAL, 1, 1, true), false, true);
    comp.AddFunction ("Vec3",               WrapVec3,               compParamTypeList () << VTP_REAL << VTP_REAL << VTP_REAL,               true,   true,   vmValType (VTP_REAL, 1, 1, true), false, true);
    comp.AddFunction ("Vec2",               WrapVec2,               compParamTypeList () << VTP_REAL << VTP_REAL,                           true,   true,   vmValType (VTP_REAL, 1, 1, true), false, true);
    comp.AddFunction ("MatrixZero",         WrapMatrixZero,         noParam,                                                   true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixIdentity",     WrapMatrixIdentity,     noParam,                                                   true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixScale",        WrapMatrixScale,        compParamTypeList () << VTP_REAL,                                       true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixScale",        WrapMatrixScale_2,      compParamTypeList () << VTP_REAL << VTP_REAL << VTP_REAL,               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixTranslate",    WrapMatrixTranslate,    compParamTypeList () << VTP_REAL << VTP_REAL << VTP_REAL,               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixRotateX",      WrapMatrixRotateX,      compParamTypeList () << VTP_REAL,                                       true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixRotateY",      WrapMatrixRotateY,      compParamTypeList () << VTP_REAL,                                       true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixRotateZ",      WrapMatrixRotateZ,      compParamTypeList () << VTP_REAL,                                       true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixBasis",        WrapMatrixBasis,        compParamTypeList () << vmValType (VTP_REAL, 1, 1, true) << vmValType (VTP_REAL, 1, 1, true) << vmValType (VTP_REAL, 1, 1, true), true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("MatrixCrossProduct", WrapMatrixCrossProduct, compParamTypeList () << vmValType (VTP_REAL, 1, 1, true),               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("CrossProduct",       WrapCross,              compParamTypeList () << vmValType (VTP_REAL, 1, 1, true) << vmValType (VTP_REAL, 1, 1, true), true, true, vmValType (VTP_REAL, 1, 1, true), false, true);
    comp.AddFunction ("Length",             WrapLength,             compParamTypeList () << vmValType (VTP_REAL, 1, 1, true),               true,   true,   VTP_REAL);
    comp.AddFunction ("Normalize",          WrapNormalize,          compParamTypeList () << vmValType (VTP_REAL, 1, 1, true),               true,   true,   vmValType (VTP_REAL, 1, 1, true), false, true);
    comp.AddFunction ("Determinant",        WrapDeterminant,        compParamTypeList () << vmValType (VTP_REAL, 2, 1, true),               true,   true,   VTP_REAL);
    comp.AddFunction ("Transpose",          WrapTranspose,          compParamTypeList () << vmValType (VTP_REAL, 2, 1, true),               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("RTInvert",           WrapRTInvert,           compParamTypeList () << vmValType (VTP_REAL, 2, 1, true),               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);
    comp.AddFunction ("Orthonormalize",     WrapOrthonormalize,     compParamTypeList () << vmValType (VTP_REAL, 2, 1, true),               true,   true,   vmValType (VTP_REAL, 2, 1, true), false, true);

    //////////////////////////////////
    // Register overloaded operators
    scaleVec            = comp.VM ().AddOperatorFunction (OpScaleVec);
    scaleVec2           = comp.VM ().AddOperatorFunction (OpScaleVec2);
    scaleMatrix         = comp.VM ().AddOperatorFunction (OpScaleMatrix);
    scaleMatrix2        = comp.VM ().AddOperatorFunction (OpScaleMatrix2);
    divVec              = comp.VM ().AddOperatorFunction (OpDivVec);
    divMatrix           = comp.VM ().AddOperatorFunction (OpDivMatrix);
    matrixVec           = comp.VM ().AddOperatorFunction (OpMatrixVec);
    matrixMatrix        = comp.VM ().AddOperatorFunction (OpMatrixMatrix);
    vecVec              = comp.VM ().AddOperatorFunction (OpVecVec);
    vecPlusVec          = comp.VM ().AddOperatorFunction (OpVecPlusVec);
    vecMinusVec         = comp.VM ().AddOperatorFunction (OpVecMinusVec);
    matrixPlusMatrix    = comp.VM ().AddOperatorFunction (OpMatrixPlusMatrix);
    matrixMinusMatrix   = comp.VM ().AddOperatorFunction (OpMatrixMinusMatrix);
    negVec              = comp.VM ().AddOperatorFunction (OpNegVec);
    negMatrix           = comp.VM ().AddOperatorFunction (OpNegMatrix);

    // Compiler callback
    comp.AddUnOperExt   (TrigUnOperatorExtension);
    comp.AddBinOperExt  (TrigBinOperatorExtension);
}
