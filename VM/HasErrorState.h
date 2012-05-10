//---------------------------------------------------------------------------
/*  Created 27-Jun-2003: Thomas Mulgrew

    Simple base class for object error handling.

*/

#ifndef HasErrorStateH
#define HasErrorStateH
#include <string>
#include <assert.h>
//---------------------------------------------------------------------------

class HasErrorState {
    // Error status
    bool                        m_error;
    std::string                 m_errorString;

protected:
    void SetError (std::string text) {
        m_error         = true;
        m_errorString   = text;
    }

public:
    // Error handling
    HasErrorState () : m_error (false) { ; }
    bool Error ()               { return m_error; }
    std::string GetError ()     { assert (m_error); return m_errorString; }
    void ClearError ()          { m_error = false; }
};

#endif
