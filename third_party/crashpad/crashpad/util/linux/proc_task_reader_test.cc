// Copyright 2019 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/linux/proc_task_reader.h"

#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "third_party/lss/lss.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

bool FindThreadID(pid_t tid, const std::vector<pid_t>& threads) {
  for (const auto& thread : threads) {
    if (thread == tid) {
      return true;
    }
  }
  return false;
}

class ScopedBlockingThread : public Thread {
 public:
  ScopedBlockingThread() : tid_sem_(0), join_sem_(0), tid_(-1) {}

  ~ScopedBlockingThread() {
    join_sem_.Signal();
    Join();
  }

  pid_t ThreadID() {
    tid_sem_.Wait();
    return tid_;
  }

 private:
  void ThreadMain() override {
    tid_ = sys_gettid();
    tid_sem_.Signal();
    join_sem_.Wait();
  }

  Semaphore tid_sem_;
  Semaphore join_sem_;
  pid_t tid_;
};

TEST(ProcTaskReader, Self) {
  std::vector<pid_t> tids;
  ASSERT_TRUE(ReadThreadIDs(getpid(), &tids));
  EXPECT_TRUE(FindThreadID(getpid(), tids));
  EXPECT_TRUE(FindThreadID(sys_gettid(), tids));

  ScopedBlockingThread thread1;
  thread1.Start();

  ScopedBlockingThread thread2;
  thread2.Start();

  pid_t thread1_tid = thread1.ThreadID();
  pid_t thread2_tid = thread2.ThreadID();

  tids.clear();
  ASSERT_TRUE(ReadThreadIDs(getpid(), &tids));
  EXPECT_TRUE(FindThreadID(getpid(), tids));
  EXPECT_TRUE(FindThreadID(thread1_tid, tids));
  EXPECT_TRUE(FindThreadID(thread2_tid, tids));
}

TEST(ProcTaskReader, BadPID) {
  std::vector<pid_t> tids;
  EXPECT_FALSE(ReadThreadIDs(-1, &tids));

  tids.clear();
  EXPECT_FALSE(ReadThreadIDs(0, &tids));
}

CRASHPAD_CHILD_TEST_MAIN(ProcTaskTestChild) {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);
  FileHandle out = StdioFileHandle(StdioStream::kStandardOutput);

  pid_t tid = getpid();
  CheckedWriteFile(out, &tid, sizeof(tid));

  tid = sys_gettid();
  CheckedWriteFile(out, &tid, sizeof(tid));

  ScopedBlockingThread thread1;
  thread1.Start();

  ScopedBlockingThread thread2;
  thread2.Start();

  tid = thread1.ThreadID();
  CheckedWriteFile(out, &tid, sizeof(tid));

  tid = thread2.ThreadID();
  CheckedWriteFile(out, &tid, sizeof(tid));

  CheckedReadFileAtEOF(in);
  return 0;
}

class ProcTaskTest : public MultiprocessExec {
 public:
  ProcTaskTest() : MultiprocessExec() {
    SetChildTestMainFunction("ProcTaskTestChild");
  }

  ProcTaskTest(const ProcTaskTest&) = delete;
  ProcTaskTest& operator=(const ProcTaskTest&) = delete;

 private:
  bool ReadIDFromChild(std::vector<pid_t>* threads) {
    pid_t tid;
    if (!LoggingReadFileExactly(ReadPipeHandle(), &tid, sizeof(tid))) {
      return false;
    }
    threads->push_back(tid);
    return true;
  }

  void MultiprocessParent() override {
    std::vector<pid_t> ids_to_find;
    for (size_t id_count = 0; id_count < 4; ++id_count) {
      ASSERT_TRUE(ReadIDFromChild(&ids_to_find));
    }

    std::vector<pid_t> threads;
    ASSERT_TRUE(ReadThreadIDs(ChildPID(), &threads));
    for (size_t index = 0; index < ids_to_find.size(); ++index) {
      SCOPED_TRACE(
          base::StringPrintf("index %zd, tid %d", index, ids_to_find[index]));
      EXPECT_TRUE(FindThreadID(ids_to_find[index], threads));
    }
  }
};

TEST(ProcTaskReader, ReadChild) {
  ProcTaskTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
