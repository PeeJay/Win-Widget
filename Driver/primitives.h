#ifndef __INCLUDE_PRIMITIVES_H_
#define __INCLUDE_PRIMITIVES_H_

#include <windows.h>
//#include "log/logfunction.h"

//�������� ���������� ������������� � �������
//�������� - ���������� SAL by K.A. Knizhnik

#define task_proc WINAPI
typedef unsigned timeout_t; // timeout in milliseconds

//����� ������������� ��� ������������
class event_internals 
{ 
private: 
	HANDLE h;

public:
	void wait() { WaitForSingleObject(h, INFINITE); }

	bool wait_with_timeout(DWORD msec) { return WaitForSingleObject(h, msec) == WAIT_OBJECT_0; }

	void signal() { SetEvent(h); }
	void reset() { ResetEvent(h); }

	event_internals(bool signaled = false) { h = CreateEvent(NULL, true, signaled, NULL); }
	~event_internals() { CloseHandle(h); }
}; 

//���������� ����� ������������� ��� ����������� ���������� ������� � ��������� ������ �� ������ �������
class mutex_internals 
{ 
protected: 
	CRITICAL_SECTION cs;

public:
	void enter() { EnterCriticalSection(&cs); }
	void leave() { LeaveCriticalSection(&cs); }

	mutex_internals() { InitializeCriticalSection(&cs); }
	~mutex_internals() { DeleteCriticalSection(&cs); } 
};


class guard_internals 
{ 
protected: 
	mutex_internals* m_mutex;

public:
	guard_internals(mutex_internals* mutex) { m_mutex = mutex; if(m_mutex) m_mutex->enter(); }
	~guard_internals() { if(m_mutex) m_mutex->leave(); } 
};


class task
{ 
public: 
	typedef void (task_proc *fptr)(void* arg); 
	enum priority 
	{ 
		pri_background, 
		pri_low, 
		pri_normal, 
		pri_high, 
		pri_realtime 
	};
	enum 
	{ 
		min_stack    = 8*1024, 
		small_stack  = 16*1024, 
		normal_stack = 64*1024,
		big_stack    = 256*1024,
		huge_stack   = 1024*1024
	}; 
	//
	// Create new task. Pointer to task object returned by this function
	// can be used only for thread identification.
	//
	static task* create(fptr f, void* arg = NULL, priority pri = pri_normal, 
		size_t stack_size = normal_stack)
	{ 
		task* threadid;
		HANDLE h = CreateThread(NULL, stack_size, LPTHREAD_START_ROUTINE(f), arg, CREATE_SUSPENDED, (DWORD*)&threadid);
		if (h == NULL) 
		{ 
			//console::error("CreateThread failed with error code=%d\n", GetLastError());
			return NULL;
		}
		SetThreadPriority(h, THREAD_PRIORITY_LOWEST + 
			(THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST) 
			* (pri - pri_background) 
			/ (pri_realtime - pri_background));
		ResumeThread(h);
		CloseHandle(h);
		return threadid;
	}

	static void  exit()
	{
		ExitThread(0);
	} 
	//
	// Current task will sleep during specified period
	//
	static void  sleep(timeout_t msec)
	{ 
		Sleep(msec);
	}
	//
	// Get current task
	//
	static task* current()
	{ 
		return (task*)GetCurrentThreadId();
	}
}; 


class SimpleWorker
{
public:
	SimpleWorker() : m_task(NULL), m_exitFlag(false) {}
	virtual ~SimpleWorker() { Stop(); }

	// ������ ������
	virtual void Start()
	{
		m_task = task::create(process_function, (void*)this, task::pri_realtime);
	}
	// ��������� ������
	virtual void Stop()
	{
		if(m_task)
		{
			m_exitFlag = true;
			// �������� ���������� ������
			m_guard_thread.enter();
			m_guard_thread.leave();

			//m_task->exit();	
			m_task = NULL;
		}
	}
	bool IsWork()
	{
		return m_task != NULL;
	}
protected:
	// �������� ������� ������� ������
	virtual bool DoWork() = 0;
	// ������ ��� ����������� ���������������� ������ �� ������
	mutex_internals m_guard_thread;

	// ������ ��� ����������� �������������� ������� � ������
	mutex_internals m_guard;
	// ���� ��������������� � ������������� ���������� ������
	volatile int	m_exitFlag;

	// ������� ������� ������
	virtual void Work()
	{
		m_guard_thread.enter();
		while (!m_exitFlag)
		{
			try
			{
				//���� � ����������� ������ ��� ����������� �������������� ������� � ����������� ������ �������
				m_guard.enter();
				//������ � ����������� ������
				bool retVal = DoWork();
				m_guard.leave();
				//���� ����������� ����� �����������������, �� �����
				if(!retVal)
					break;
			}
			catch (...)
			{
				exit(-1);
			}
		}
		m_guard_thread.leave();
		task::exit();
	}
private:
	static void task_proc process_function(void* arg)
	{
		SimpleWorker* worker = (SimpleWorker*)arg;
		if(worker)
			worker->Work();
	}
	task*			m_task;
};

#endif //__INCLUDE_PRIMITIVES_H_