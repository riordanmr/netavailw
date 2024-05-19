#pragma once

#include <Windows.h>

// Class that represents a Windows critical section.
class CCritSec
{
    CRITICAL_SECTION m_crit;

public:
    CCritSec();
    ~CCritSec();

    CRITICAL_SECTION* GetCritSecPtr();
};

// Class that automatically acquires a critical section at creation,
// and releases it upon deletion.

class CCritSecInScope
{
    CRITICAL_SECTION* m_pCrit;
public:
    CCritSecInScope(CRITICAL_SECTION *pCrit) {
        m_pCrit = pCrit;
        EnterCriticalSection(m_pCrit);
    }

    ~CCritSecInScope() {
        LeaveCriticalSection(m_pCrit);
    }
};
