//---------------------------------------------------------------------------
// Created 28-Sep-2003: Thomas Mulgrew (tmulgrew@slingshot.co.nz)
// Copyright (C) Thomas Mulgrew
#ifndef compFunctionH
#define compFunctionH
//---------------------------------------------------------------------------
#include "../VM/vmTypes.h"
#include "../VM/vmFunction.h"
#include <vector>

////////////////////////////////////////////////////////////////////////////////
// compParamTypeList
typedef std::vector<vmValType> vmValTypeList;

class compParamTypeList {
    vmValTypeList m_params;

public:
    compParamTypeList () { ; }
    compParamTypeList (compParamTypeList& list);
    compParamTypeList& operator<< (vmValType val) {
        m_params.push_back (val);
        return *this;
    }

    vmValTypeList &Params () { return m_params; }
};

////////////////////////////////////////////////////////////////////////////////
// compFuncSpec
//
// Specifies a function and parameters
struct compFuncSpec {
    compParamTypeList   m_paramTypes;
    bool                m_brackets,         // True if function requires brackets around parameters
                        m_isFunction;       // True if function returns a value
    vmValType           m_returnType;
    bool                m_timeshare;        // True if virtual machine should perform a timesharing break immediately after returning
    int                 m_index;            // Index in Virtual Machine's "functions" array
    bool                m_freeTempData;     // True if function allocates temporary data that should be freed before the next instruction

    compFuncSpec () { ; }
    compFuncSpec (  compParamTypeList&  paramTypes,
                    bool                brackets,
                    bool                isFunction,
                    vmValType           returnType,
                    bool                timeshare,
                    int                 index,
                    bool                freeTempData)
        :   m_paramTypes    (paramTypes),
            m_brackets      (brackets),
            m_isFunction    (isFunction),
            m_returnType    (returnType),
            m_timeshare     (timeshare),
            m_index         (index),
            m_freeTempData  (freeTempData)          { ; }
    compFuncSpec (  const compFuncSpec& spec)
        :   m_isFunction    (spec.m_isFunction),
            m_brackets      (spec.m_brackets),
            m_returnType    (spec.m_returnType),
            m_timeshare     (spec.m_timeshare),
            m_index         (spec.m_index),
            m_freeTempData  (spec.m_freeTempData)   { m_paramTypes = spec.m_paramTypes; }
};

#endif
