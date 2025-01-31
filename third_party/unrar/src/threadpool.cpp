#include "rar.hpp"


#ifdef RAR_SMP
#include "threadmisc.cpp"

#ifdef _WIN_ALL
int ThreadPool::ThreadPriority=THREAD_PRIORITY_NORMAL;
#endif

ThreadPool::ThreadPool(uint MaxThreads)
{
  MaxAllowedThreads = MaxThreads;
  if (MaxAllowedThreads>MaxPoolThreads)
    MaxAllowedThreads=MaxPoolThreads;
  if (MaxAllowedThreads==0)
    MaxAllowedThreads=1;

  ThreadsCreatedCount=0;

  // If we have more threads than queue size, we'll hang on pool destroying,
  // not releasing all waiting threads.
  if (MaxAllowedThreads>ASIZE(TaskQueue))
    MaxAllowedThreads=ASIZE(TaskQueue);

  Closing=false;

  bool Success = CriticalSectionCreate(&CritSection);
#ifdef _WIN_ALL
  QueuedTasksCnt=CreateSemaphore(NULL,0,ASIZE(TaskQueue),NULL);
  NoneActive=CreateEvent(NULL,TRUE,TRUE,NULL);
  Success=Success && QueuedTasksCnt!=NULL && NoneActive!=NULL;
#elif defined(_UNIX)
  AnyActive = false;
  QueuedTasksCnt = 0;
  Success=Success && pthread_cond_init(&AnyActiveCond,NULL)==0 &&
          pthread_mutex_init(&AnyActiveMutex,NULL)==0 &&
          pthread_cond_init(&QueuedTasksCntCond,NULL)==0 &&
          pthread_mutex_init(&QueuedTasksCntMutex,NULL)==0;
#endif
  if (!Success)
  {
    ErrHandler.GeneralErrMsg(L"\nThread pool initialization failed.");
    ErrHandler.Exit(RARX_FATAL);
  }

  QueueTop = 0;
  QueueBottom = 0;
  ActiveThreads = 0;
}


ThreadPool::~ThreadPool()
{
  WaitDone();
  Closing=true;

#ifdef _WIN_ALL
  ReleaseSemaphore(QueuedTasksCnt,ASIZE(TaskQueue),NULL);
#elif defined(_UNIX)
  // Threads still can access QueuedTasksCnt for a short time after WaitDone(),
  // so lock is required. We would occassionally hang without it.
  pthread_mutex_lock(&QueuedTasksCntMutex);
  QueuedTasksCnt+=ASIZE(TaskQueue);
  pthread_mutex_unlock(&QueuedTasksCntMutex);

  pthread_cond_broadcast(&QueuedTasksCntCond);
#endif

  for(uint I=0;I<ThreadsCreatedCount;I++)
  {
#ifdef _WIN_ALL
    // Waiting until the thread terminates.
    CWaitForSingleObject(ThreadHandles[I]);
#endif
    // Close the thread handle. In Unix it results in pthread_join call,
    // which also waits for thread termination.
    ThreadClose(ThreadHandles[I]);
  }

  CriticalSectionDelete(&CritSection);
#ifdef _WIN_ALL
  CloseHandle(QueuedTasksCnt);
  CloseHandle(NoneActive);
#elif defined(_UNIX)
  pthread_cond_destroy(&AnyActiveCond);
  pthread_mutex_destroy(&AnyActiveMutex);
  pthread_cond_destroy(&QueuedTasksCntCond);
  pthread_mutex_destroy(&QueuedTasksCntMutex);
#endif
}


void ThreadPool::CreateThreads()
{
#ifdef WIN32_CPU_GROUPS
  // 2024.12.28: Implement processor group support for pre-Windows 11 systems
  // with number of CPUs exceeding 64. For example, for 72 CPU the single
  // processor group size would be 36 and this is what RAR would use without
  // processor group support.

  uint GroupCount=0;
  uint CurGroupNumber=(uint)-1; // We'll increment it to 0 later.
  uint CurGroupSize=0,CumulativeGroupSize=0;

  typedef DWORD (WINAPI *GETACTIVEPROCESSORCOUNT)(WORD GroupNumber);
  GETACTIVEPROCESSORCOUNT pGetActiveProcessorCount=nullptr;
  typedef BOOL (WINAPI *GETTHREADGROUPAFFINITY)(HANDLE hThread,PGROUP_AFFINITY GroupAffinity);
  GETTHREADGROUPAFFINITY pGetThreadGroupAffinity=nullptr;
  typedef BOOL (WINAPI *SETTHREADGROUPAFFINITY)(HANDLE hThread,const GROUP_AFFINITY *GroupAffinity,PGROUP_AFFINITY PreviousGroupAffinity);
  SETTHREADGROUPAFFINITY pSetThreadGroupAffinity=nullptr;

  // Doesn't seem to be needed in Windows 11 and newer.
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessaffinitymask
  // "Starting with Windows 11 and Windows Server 2022, on a system with
  //  more than 64 processors, process and thread affinities span all
  //  processors in the system, across all processor groups, by default."
  if (!IsWindows11OrGreater())
  {
    HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
    if (hKernel!=nullptr)
    {
      typedef WORD (WINAPI *GETACTIVEPROCESSORGROUPCOUNT)();
      GETACTIVEPROCESSORGROUPCOUNT pGetActiveProcessorGroupCount=(GETACTIVEPROCESSORGROUPCOUNT)GetProcAddress(hKernel,"GetActiveProcessorGroupCount");

      pGetActiveProcessorCount=(GETACTIVEPROCESSORCOUNT)GetProcAddress(hKernel,"GetActiveProcessorCount");
      pGetThreadGroupAffinity=(GETTHREADGROUPAFFINITY)GetProcAddress(hKernel,"GetThreadGroupAffinity");
      pSetThreadGroupAffinity=(SETTHREADGROUPAFFINITY)GetProcAddress(hKernel,"SetThreadGroupAffinity");

      if (pGetActiveProcessorCount!=nullptr && pGetActiveProcessorGroupCount!=nullptr &&
          pGetThreadGroupAffinity!=nullptr && pSetThreadGroupAffinity!=nullptr)
        GroupCount=pGetActiveProcessorGroupCount();
    }
  }
#endif

  for (uint I=0;I<MaxAllowedThreads;I++)
  {
    THREAD_HANDLE hThread=ThreadCreate(PoolThread, this);
    ThreadHandles[I] = hThread;
    ThreadsCreatedCount++;
#ifdef _WIN_ALL
#ifdef WIN32_CPU_GROUPS
    if (GroupCount>1) // If we have multiple processor groups.
    {
      if (I>=CumulativeGroupSize) // Filled the processor group, go to next.
      {
        if (++CurGroupNumber>=GroupCount)
        {
          // If we exceeded the group number, such as when user specified
          // -mt64 for lower core count, start assigning from beginning.
          CurGroupNumber=0;
          CumulativeGroupSize=0;
        }
        // Current group size.
        CurGroupSize=pGetActiveProcessorCount(CurGroupNumber);
        // Size of all preceding groups including the current.
        CumulativeGroupSize+=CurGroupSize;
      }
      GROUP_AFFINITY GroupAffinity;
      pGetThreadGroupAffinity(hThread,&GroupAffinity);

      // Since normally before Windows 11 all threads belong to same source
      // group, we could set this value only once. But we set it every time
      // in case we'll decide for some reason to use it to rearrange threads
      // from different source groups in Windows 11+.
      uint SrcGroupSize=pGetActiveProcessorCount(GroupAffinity.Group);

      // Shifting by 64 would be the undefined behavior, so we treat 64 separately.
      KAFFINITY SrcGroupMask=(KAFFINITY)(SrcGroupSize==64 ? (uint64)0xffffffffffffffff:(uint64(1)<<SrcGroupSize)-1);

      // Here we check that process affinity for existing thread group
      // matches the entire group size. If user limited the process
      // affinity, we prefer to not extend the process to other groups,
      // because user might want to restrict the resource usage.
      // Also if source processor group is larger than required number
      // of threads, we do not need to move threads between groups.
      if (SrcGroupSize!=0 && GroupAffinity.Mask==SrcGroupMask &&
          GroupAffinity.Group!=CurGroupNumber && SrcGroupSize<MaxAllowedThreads)
      {
        // Shifting by 64 would be the undefined behavior, so we treat 64 separately.
        KAFFINITY CurGroupMask=(KAFFINITY)(CurGroupSize==64 ? (uint64)0xffffffffffffffff:(uint64(1)<<CurGroupSize)-1);
        GroupAffinity.Mask=CurGroupMask; // Use the entire group.
        GroupAffinity.Group=CurGroupNumber;

        // Assign the thread to a new group.
        pSetThreadGroupAffinity(hThread,&GroupAffinity,NULL);
      }
    }
#endif
    
    // Set the thread priority if needed.
    if (ThreadPool::ThreadPriority!=THREAD_PRIORITY_NORMAL)
      SetThreadPriority(ThreadHandles[I],ThreadPool::ThreadPriority);
#endif
  }
}


NATIVE_THREAD_TYPE ThreadPool::PoolThread(void *Param)
{
  ((ThreadPool*)Param)->PoolThreadLoop();
  return 0;
}


void ThreadPool::PoolThreadLoop()
{
  QueueEntry Task;
  while (GetQueuedTask(&Task))
  {
    Task.Proc(Task.Param);
    
    CriticalSectionStart(&CritSection); 
    if (--ActiveThreads == 0)
    {
#ifdef _WIN_ALL
      SetEvent(NoneActive);
#elif defined(_UNIX)
      pthread_mutex_lock(&AnyActiveMutex);
      AnyActive=false;
      pthread_cond_signal(&AnyActiveCond);
      pthread_mutex_unlock(&AnyActiveMutex);
#endif
    }
    CriticalSectionEnd(&CritSection); 
  }
}


bool ThreadPool::GetQueuedTask(QueueEntry *Task)
{
#ifdef _WIN_ALL
  CWaitForSingleObject(QueuedTasksCnt);
#elif defined(_UNIX)
  pthread_mutex_lock(&QueuedTasksCntMutex);
  while (QueuedTasksCnt==0)
    cpthread_cond_wait(&QueuedTasksCntCond,&QueuedTasksCntMutex);
  QueuedTasksCnt--;
  pthread_mutex_unlock(&QueuedTasksCntMutex);
#endif

  if (Closing)
    return false;

  CriticalSectionStart(&CritSection); 

  *Task = TaskQueue[QueueBottom];
  QueueBottom = (QueueBottom + 1) % ASIZE(TaskQueue);

  CriticalSectionEnd(&CritSection); 

  return true;
}


// Add task to queue. We assume that it is always called from main thread,
// it allows to avoid any locks here. We process collected tasks only
// when WaitDone is called.
void ThreadPool::AddTask(PTHREAD_PROC Proc,void *Data)
{
  if (ThreadsCreatedCount == 0)
    CreateThreads();
  
  // If queue is full, wait until it is empty.
  if (ActiveThreads>=ASIZE(TaskQueue))
    WaitDone();

  TaskQueue[QueueTop].Proc = Proc;
  TaskQueue[QueueTop].Param = Data;
  QueueTop = (QueueTop + 1) % ASIZE(TaskQueue);
  ActiveThreads++;
}


// Start queued tasks and wait until all threads are inactive.
// We assume that it is always called from main thread, when pool threads
// are sleeping yet.
void ThreadPool::WaitDone()
{
  if (ActiveThreads==0)
    return;
#ifdef _WIN_ALL
  ResetEvent(NoneActive);
  ReleaseSemaphore(QueuedTasksCnt,ActiveThreads,NULL);
  CWaitForSingleObject(NoneActive);
#elif defined(_UNIX)
  AnyActive=true;

  // Threads reset AnyActive before accessing QueuedTasksCnt and even
  // preceding WaitDone() call does not guarantee that some slow thread
  // is not accessing QueuedTasksCnt now. So lock is necessary.
  pthread_mutex_lock(&QueuedTasksCntMutex);
  QueuedTasksCnt+=ActiveThreads;
  pthread_mutex_unlock(&QueuedTasksCntMutex);

  pthread_cond_broadcast(&QueuedTasksCntCond);

  pthread_mutex_lock(&AnyActiveMutex);
  while (AnyActive)
    cpthread_cond_wait(&AnyActiveCond,&AnyActiveMutex);
  pthread_mutex_unlock(&AnyActiveMutex);
#endif
}
#endif // RAR_SMP
