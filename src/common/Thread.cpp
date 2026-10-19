// Thread: the worker-thread base class.
#include "Thread.h"

#include "XboxKernel.h"

// 0x000AA458
Thread::Thread()
{
	m_dwThreadId = 0;
	m_hThread = NULL;
	m_bTerminate = false;
}

// 0x000AA448
Thread::~Thread()
{
	CloseThread();
}

// 0x000AA3B8
unsigned __stdcall Thread::ThreadProc(void *param)
{
	Thread *thread = (Thread *)param;
	unsigned result = 1;
	if (thread != NULL)
	{
		result = thread->Run();
		thread->OnExit();
	}
	return result;
}

// 0x000AA416
HANDLE Thread::Start(unsigned stackSize, bool suspended)
{
	m_bTerminate = false;
	if (m_hThread == NULL)
		m_hThread = (HANDLE)_beginthreadex(NULL, stackSize, ThreadProc, this, suspended ? 4 /* CREATE_SUSPENDED */ : 0,
		                                   &m_dwThreadId);
	return m_hThread;
}

// 0x000AA385
DWORD Thread::Resume()
{
	m_bTerminate = false;
	if (m_hThread == NULL)
		return 0;
	return ResumeThread(m_hThread);
}

// 0x000AA39A
DWORD Thread::Suspend()
{
	if (m_hThread == NULL)
		return 0;
	return SuspendThread(m_hThread);
}

// 0x000AA404
void Thread::CloseThread()
{
	if (m_hThread != NULL)
	{
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}

// 0x000AA46A
void Thread::Join()
{
	if (m_hThread != NULL)
		WaitForSingleObject(m_hThread, INFINITE);
	m_bTerminate = false;
	CloseThread();
}

// 0x000234B4
void Thread::Stop()
{
	m_bTerminate = true;
	Join();
	CloseThread();
}
