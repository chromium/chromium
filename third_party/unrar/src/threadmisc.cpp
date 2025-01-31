static inline bool CriticalSectionCreate(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  InitializeCriticalSection(CritSection);
  return true;
#elif defined(_UNIX)
  return pthread_mutex_init(CritSection,NULL)==0;
#endif
}


static inline void CriticalSectionDelete(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  DeleteCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_destroy(CritSection);
#endif
}


static inline void CriticalSectionStart(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  EnterCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_lock(CritSection);
#endif
}


static inline void CriticalSectionEnd(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  LeaveCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_unlock(CritSection);
#endif
}


static THREAD_HANDLE ThreadCreate(NATIVE_THREAD_PTR Proc,void *Data)
{
#ifdef _UNIX
/*
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
*/
  pthread_t pt;
  int Code=pthread_create(&pt,NULL/*&attr*/,Proc,Data);
  if (Code!=0)
  {
    wchar Msg[100];
    swprintf(Msg,ASIZE(Msg),L"\npthread_create failed, code %d\n",Code);
    ErrHandler.GeneralErrMsg(Msg);
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return pt;
#else
  DWORD ThreadId;
  HANDLE hThread=CreateThread(NULL,0x10000,Proc,Data,0,&ThreadId);
  if (hThread==NULL)
  {
    ErrHandler.GeneralErrMsg(L"CreateThread failed");
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return hThread;
#endif
}


static void ThreadClose(THREAD_HANDLE hThread)
{
#ifdef _UNIX
  pthread_join(hThread,NULL);
#else
  CloseHandle(hThread);
#endif
}


#ifdef _WIN_ALL
static void CWaitForSingleObject(HANDLE hHandle)
{
  DWORD rc=WaitForSingleObject(hHandle,INFINITE);
  if (rc==WAIT_FAILED)
  {
    ErrHandler.GeneralErrMsg(L"\nWaitForMultipleObjects error %d, GetLastError %d",rc,GetLastError());
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif


#ifdef _UNIX
static void cpthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  int rc=pthread_cond_wait(cond,mutex);
  if (rc!=0)
  {
    ErrHandler.GeneralErrMsg(L"\npthread_cond_wait error %d",rc);
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif


uint GetNumberOfCPU()
{
#ifndef RAR_SMP
  return 1;
#else
#ifdef _UNIX
#ifdef _SC_NPROCESSORS_ONLN
  uint Count=(uint)sysconf(_SC_NPROCESSORS_ONLN);
  return Count<1 ? 1:Count;
#elif defined(_APPLE)
  uint Count;
  size_t Size=sizeof(Count);
  return sysctlbyname("hw.ncpu",&Count,&Size,NULL,0)==0 ? Count:1;
#endif
#else // !_UNIX

#ifdef WIN32_CPU_GROUPS
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessaffinitymask
  // "Starting with Windows 11 and Windows Server 2022, on a system with
  //  more than 64 processors, process and thread affinities span all
  //  processors in the system, across all processor groups, by default."
  // Supposing there are 80 CPUs in 2 processor groups 40 CPUs each.
  // Looks like, beginning from Windows 11 an app can use them all by default,
  // not resorting to processor groups API. But if we use GetProcessAffinityMask
  // to count CPUs, we would be limited to 40 CPUs only. So we call 
  // GetActiveProcessorCount() if it is available anf if there are multiple
  // processor groups. For a single group we prefer the affinity aware
  // GetProcessAffinityMask(). Out thread pool code handles the case
  // with restricted processor group affinity. So we avoid the complicated
  // code to calculate all processor groups affinity here, such as using
  // GetLogicalProcessorInformationEx, and resort to GetActiveProcessorCount().
  HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
  if (hKernel!=nullptr)
  {
    typedef DWORD (WINAPI *GETACTIVEPROCESSORCOUNT)(WORD GroupNumber);
    GETACTIVEPROCESSORCOUNT pGetActiveProcessorCount=(GETACTIVEPROCESSORCOUNT)GetProcAddress(hKernel,"GetActiveProcessorCount");
    typedef WORD (WINAPI *GETACTIVEPROCESSORGROUPCOUNT)();
    GETACTIVEPROCESSORGROUPCOUNT pGetActiveProcessorGroupCount=(GETACTIVEPROCESSORGROUPCOUNT)GetProcAddress(hKernel,"GetActiveProcessorGroupCount");
    if (pGetActiveProcessorCount!=nullptr && pGetActiveProcessorGroupCount!=nullptr &&
        pGetActiveProcessorGroupCount()>1)
    {
      // Once the thread pool called SetThreadGroupAffinity(),
      // GetProcessAffinityMask() below will return 0. So we shall always
      // use GetActiveProcessorCount() here if there are multiple processor
      // groups, which makes SetThreadGroupAffinity() call possible.
      DWORD Count=pGetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
      return Count;
    }
  }
#endif

  DWORD_PTR ProcessMask;
  DWORD_PTR SystemMask;

  if (!GetProcessAffinityMask(GetCurrentProcess(),&ProcessMask,&SystemMask))
    return 1;
  uint Count=0;
  for (DWORD_PTR Mask=1;Mask!=0;Mask<<=1)
    if ((ProcessMask & Mask)!=0)
      Count++;
  return Count<1 ? 1:Count;
#endif

#endif // RAR_SMP
}


uint GetNumberOfThreads()
{
  uint NumCPU=GetNumberOfCPU();
  if (NumCPU<1)
    return 1;
  if (NumCPU>MaxPoolThreads)
    return MaxPoolThreads;
  return NumCPU;
}



