//
// basic thread template (for DirectShow)
//
// (c) Geraint Davies 2003.  Freely Redistributable.
//


#ifndef _thread_h_
#define _thread_h_

class thread
{
public:
    thread() : 
		m_hThread(NULL),
		m_idThread(0),
		m_evExit(true),    // manual reset
		m_evStartPermit(true),
		m_evStart(true),
		m_evRestartRequest(true)
    {
	}

    void StartThread()
    {
        if (m_hThread == NULL) {
            m_evExit.Reset();
			m_evStartPermit.Set();
            m_hThread = CreateThread(NULL, 0, DispatchThread, this, 0, &m_idThread);
        }
    }

    void StopThread()
    {
        m_evExit.Set();
        if (m_hThread != NULL) {
            if (GetCurrentThreadId() != m_idThread) {
                WaitForSingleObject(m_hThread,INFINITE);
            }
            CloseHandle(m_hThread);
            m_hThread = NULL;
        }
    }
    
    //bool ShouldExit()
    //{
    //    return m_evExit.Check() ? true : false;
    //}
    HANDLE ExitEvent()
    {
        return m_evExit;
    }
	VOID BeginRestartThread()
	{
		m_evStartPermit.Reset();
		m_evStart.Reset();
		m_evRestartRequest.Set();
		const HANDLE phObjects[] = { m_hThread, m_evStart };
		static const DWORD g_nTimeoutTime = 2000; // 2 seconds
		const DWORD nWaitResult = WaitForMultipleObjects(_countof(phObjects), phObjects, FALSE, g_nTimeoutTime);
		ASSERT(nWaitResult - WAIT_OBJECT_0 < _countof(phObjects) || nWaitResult == WAIT_TIMEOUT);
		if(nWaitResult == WAIT_OBJECT_0 + 0) // m_hThread
			StopThread();
	}
	VOID EndRestartThread()
	{
		if(m_hThread)
			m_evStartPermit.Set();
		else
			StartThread();
	}
    HANDLE RestartRequestEvent()
    {
        return m_evRestartRequest;
    }
	
private:
    static DWORD WINAPI DispatchThread(void* pContext) {
        thread* pThis = (thread*) pContext;
        return pThis->ThreadProc();
    }
    virtual DWORD ThreadProc() { 
        return 0;
    }

private:
    HANDLE m_hThread;
    DWORD m_idThread;
    CAMEvent m_evExit;
	CAMEvent m_evStartPermit;
	CAMEvent m_evStart;
	CAMEvent m_evRestartRequest;

protected:
	BOOL InternalRestartThread()
	{
		m_evRestartRequest.Reset();
		m_evStart.Set();
		const HANDLE phObjects[] = { m_evExit, m_evStartPermit };
		const DWORD nWaitResult = WaitForMultipleObjects(_countof(phObjects), phObjects, FALSE, INFINITE);
		ASSERT(nWaitResult - WAIT_OBJECT_0 < _countof(phObjects));
		return nWaitResult == WAIT_OBJECT_0 + 1; // m_evStartPermit
	}
};

#endif //  _thread_h_

