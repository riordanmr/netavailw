#include "CritSec.h"


CCritSec::CCritSec()
{
    InitializeCriticalSection(&m_crit);
}

CCritSec::~CCritSec()
{
    DeleteCriticalSection(&m_crit);
}

CRITICAL_SECTION* CCritSec::GetCritSecPtr()
{
    return &m_crit;
};
