// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script analyzes interrupt wakeups caused by the execution of a process
// and all its children.
//
// Execute like this:
//   sudo dtrace -s iwakeups.d $(pgrep -x "Google Chrome")

// Note on results produced:
//
// This script will produce a data file suitable to be converted to a pprof by
// export_dtrace.py. Care needs to be taken in the analysis of the data. As
// stated in the probe comments, this script emits the stacks of the code that
// ran because of the interrupt wakeup, not the code that triggered the wakeup.
// The iwakeup probe fires in the Darwin kernel
// (see github.com/apple/darwin-xnu/blob/2ff845c2e033bd0ff64b5b6aa6063a1f8f65aa32/osfmk/kern/sched_prim.c#L709).
// The first user space thread that will come on-cpu as a result of the wakeup
// will be in the state that made the thread go to sleep in the first place.
//
// Example:
//
// libsystem_kernel.dylib`mach_msg_trap+0xa
// Google Chrome Framework`base::WaitableEvent::TimedWait(base::TimeDelta const&)+0x172
// Google Chrome Framework`base::internal::WorkerThread::RunWorker()+0x382
// Google Chrome Framework`base::internal::WorkerThread::RunPooledWorker()+0xd
// Google Chrome Framework`base::(anonymous namespace)::ThreadFunc(void*)+0x75
// libsystem_pthread.dylib`_pthread_start+0x7d
// libsystem_pthread.dylib`thread_start+0xf
//
// In this example the stack was emitted because the cpu was interrupted to
// allow the code to exit the TimedWait() invocation. This kind of data could
// be called "wakee" stacks. In contrast, the "waker" stack could be captured
// using stack() in the iwakeup probe. It cannot be captured using ustack()
// since the first wakeup is not caused by user space code.

// This probe fires when the CPU is interrupted to wake up a thread.
sched:::iwakeup
{
  // Make note that the thread's stack should be captured the next time it
  // goes on-cpu.
  wakees[((thread_t)arg0)->thread_id] = 1;
}

// This probe fires when a thread begins execution on a cpu.
// Additional conditions are used to execute the body only for threads in our
// processes of interest, when they've previously caused an interrupt wake up.
sched:::on-cpu/(pid == $1 || ppid == $1) && wakees[curthread->thread_id] == 1/
{
  @wakes[ustack(512)] = count();

  // Don't monitor this thread again until the next iwakeup.
  wakees[curthread->thread_id] = 0;
}

// This probe fires when a thread begins execution on a cpu.
// Additional conditions are used to execute the body only for threads in
// processes of no interest, when they've previously caused an interrupt wake up.
sched:::on-cpu/pid != $1 && ppid != $1 && wakees[curthread->thread_id] == 1/
{
  // This thread does not belong to a process of interest. Remove it from |wakees|
  // to avoid capturing stacks if thread id reuse causes a thread with the same
  // id to go on-cpu on a process of interest.
  wakees[curthread->thread_id] = 0;
}
