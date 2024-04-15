// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_child_launcher.h"

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <vector>

#include "reference_drivers/handle_eintr.h"
#include "test/multinode_test.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "util/safe_math.h"

namespace ipcz::test {

namespace {

// NOTE: This switch name must be identical to Chromium's kTestChildProcess
// switch in //base/base_switches.h in order for these tests to work properly as
// part of any base::TestSuite based unit tests.
constexpr std::string_view kTestChildProcess = "test-child-process";

// Used to tell a forked child process which open file descriptor corresponds to
// its ipcz driver's SocketTransport.
constexpr std::string_view kSocketFd = "test-child-socket-fd";

using ArgList = std::vector<std::string>;
ArgList& GetArgList() {
  static ArgList* list = new ArgList();
  return *list;
}

std::string& GetTestNodeName() {
  static std::string* name = new std::string();
  return *name;
}

reference_drivers::FileDescriptor& GetSocketFd() {
  static reference_drivers::FileDescriptor* descriptor =
      new reference_drivers::FileDescriptor();
  return *descriptor;
}

template <typename ValueType>
std::string MakeSwitch(std::string_view name, ValueType value) {
  return absl::StrCat("--", name.data(), "=", value);
}

// Produces an argv-style data representation of a vector of strings.
std::vector<char*> MakeExecArgv(ArgList& args) {
  // +1 to NULL-terminate.
  std::vector<char*> argv(args.size() + 1);
  for (size_t i = 0; i < args.size(); ++i) {
    argv[i] = args[i].data();
  }
  return argv;
}

int GetMaxFds() {
  struct rlimit num_files;
  int result = getrlimit(RLIMIT_NOFILE, &num_files);
  ABSL_ASSERT(result == 0);
  return checked_cast<int>(num_files.rlim_cur);
}

}  // namespace

TestChildLauncher::TestChildLauncher() = default;

TestChildLauncher::~TestChildLauncher() = default;

// static
void TestChildLauncher::Initialize(int argc, char** argv) {
  // This implements extremely cheesy command-line "parsing" by scanning for the
  // only two switches we'll ever care about.

  const std::string kTestChildSwitchPrefix =
      absl::StrCat("--", kTestChildProcess.data(), "=");
  const std::string kSocketFdSwitchPrefix =
      absl::StrCat("--", kSocketFd.data(), "=");

  // We stash a complete copy of the command line in GetArgList(), modulo the
  // switches above which are specific to a single child process. This copy is
  // used to stamp out new command lines for future fork/exec'd processes.
  ArgList& args = GetArgList();
  args.resize(argc);
  for (int i = 0; i < argc; ++i) {
    std::string_view value(argv[i]);
    if (value.rfind(kTestChildSwitchPrefix) != std::string::npos) {
      GetTestNodeName() = value.substr(kTestChildSwitchPrefix.size());
    } else if (value.rfind(kSocketFdSwitchPrefix) != std::string::npos) {
      int fd;
      const bool ok = absl::SimpleAtoi(
          value.substr(kSocketFdSwitchPrefix.size()).data(), &fd);
      ABSL_HARDENING_ASSERT(ok);
      GetSocketFd() = reference_drivers::FileDescriptor(fd);
    } else {
      args[i] = value;
    }
  }
}

// static
bool TestChildLauncher::RunTestChild(int& exit_code) {
  if (GetTestNodeName().empty()) {
    return false;
  }

  // Run the function emitted by named test node's MUTLTIPROCESS_TEST_MAIN()
  // invocation. Note that this only occurs in upstream ipcz_tests. If these
  // tests are run as part of a base::TestSuite in the Chromium repository,
  // the TestSuite itself is responsible for invoking this function in child
  // processes. See base::TestSuite::Run() for that.
  exit_code =
      multi_process_function_list::InvokeChildProcessTest(GetTestNodeName());
  return true;
}

// static
reference_drivers::FileDescriptor
TestChildLauncher::TakeChildSocketDescriptor() {
  reference_drivers::FileDescriptor& fd = GetSocketFd();
  ABSL_HARDENING_ASSERT(fd.is_valid());
  return std::move(fd);
}

// static
bool TestChildLauncher::WaitForSuccessfulProcessTermination(pid_t pid) {
  int status;
  pid_t result = HANDLE_EINTR(waitpid(pid, &status, 0));
  return result == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

pid_t TestChildLauncher::Launch(std::string_view node_name,
                                std::string_view feature_set,
                                reference_drivers::FileDescriptor socket) {
  pid_t child_pid = fork();
  ABSL_HARDENING_ASSERT(child_pid >= 0);

  if (child_pid > 0) {
    // In the parent.
    socket.reset();
    return child_pid;
  }

  // In the child. First clean up all file descriptors other than the ones we
  // want to keep open.
  for (int i = STDERR_FILENO + 1; i < GetMaxFds(); ++i) {
    if (i != socket.get()) {
      close(i);
    }
  }

  // Execute the test binary with an extra command-line switch that circumvents
  // the normal test runner path and instead runs the named TestNode's body.
  ArgList child_args = GetArgList();
  std::string test_main_name =
      absl::StrCat(node_name.data(), "/", internal::kMultiprocessTestDriverName,
                   "_", feature_set);
  child_args.push_back(MakeSwitch(kTestChildProcess, test_main_name));
  child_args.push_back(MakeSwitch(kSocketFd, socket.release()));

  std::vector<char*> child_argv = MakeExecArgv(child_args);
  execv(child_argv[0], child_argv.data());

  // Should never be reached.
  ABSL_HARDENING_ASSERT(false);
  return 0;
}

}  // namespace ipcz::test
