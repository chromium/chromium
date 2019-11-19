// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "client/crashpad_client.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "third_party/lss/lss.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/linux/scoped_pr_set_dumpable.h"
#include "util/linux/scoped_pr_set_ptracer.h"
#include "util/linux/socket.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/double_fork_and_exec.h"
#include "util/posix/signals.h"

namespace crashpad {

namespace {

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

std::string FormatArgumentAddress(const std::string& name, const void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

#if defined(OS_ANDROID)

std::vector<std::string> BuildAppProcessArgs(
    const std::string& class_name,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
#if defined(ARCH_CPU_64_BITS)
  static constexpr char kAppProcess[] = "/system/bin/app_process64";
#else
  static constexpr char kAppProcess[] = "/system/bin/app_process32";
#endif

  std::vector<std::string> argv;
  argv.push_back(kAppProcess);
  argv.push_back("/system/bin");
  argv.push_back("--application");
  argv.push_back(class_name);

  std::vector<std::string> handler_argv =
      BuildHandlerArgvStrings(base::FilePath(kAppProcess),
                              database,
                              metrics_dir,
                              url,
                              annotations,
                              arguments);

  if (socket != kInvalidFileHandle) {
    handler_argv.push_back(FormatArgumentInt("initial-client-fd", socket));
  }

  argv.insert(argv.end(), handler_argv.begin(), handler_argv.end());
  return argv;
}

std::vector<std::string> BuildArgsToLaunchWithLinker(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv;
  if (is_64_bit) {
    argv.push_back("/system/bin/linker64");
  } else {
    argv.push_back("/system/bin/linker");
  }
  argv.push_back(handler_trampoline);
  argv.push_back(handler_library);

  std::vector<std::string> handler_argv = BuildHandlerArgvStrings(
      base::FilePath(), database, metrics_dir, url, annotations, arguments);

  if (socket != kInvalidFileHandle) {
    handler_argv.push_back(FormatArgumentInt("initial-client-fd", socket));
  }

  argv.insert(argv.end(), handler_argv.begin() + 1, handler_argv.end());
  return argv;
}

#endif  // OS_ANDROID

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  // Returns the currently installed signal hander. May be `nullptr` if no
  // handler has been installed.
  static SignalHandler* Get() { return handler_; }

  // Disables any installed Crashpad signal handler for the calling thread. If a
  // crash signal is received, any previously installed (non-Crashpad) signal
  // handler will be restored and the signal reraised.
  static void DisableForThread() { disabled_for_thread_ = true; }

  void SetFirstChanceHandler(CrashpadClient::FirstChanceHandler handler) {
    first_chance_handler_ = handler;
  }

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  bool HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    if (disabled_for_thread_) {
      return false;
    }

    if (first_chance_handler_ &&
        first_chance_handler_(
            signo, siginfo, static_cast<ucontext_t*>(context))) {
      return true;
    }

    exception_information_.siginfo_address =
        FromPointerCast<decltype(exception_information_.siginfo_address)>(
            siginfo);
    exception_information_.context_address =
        FromPointerCast<decltype(exception_information_.context_address)>(
            context);
    exception_information_.thread_id = sys_gettid();

    ScopedPrSetDumpable set_dumpable(false);
    HandleCrashImpl();
    return false;
  }

 protected:
  SignalHandler() = default;

  bool Install(const std::set<int>* unhandled_signals) {
    DCHECK(!handler_);
    handler_ = this;
    return Signals::InstallCrashHandlers(
        HandleOrReraiseSignal, 0, &old_actions_, unhandled_signals);
  }

  const ExceptionInformation& GetExceptionInfo() {
    return exception_information_;
  }

  virtual void HandleCrashImpl() = 0;

 private:
  // The signal handler installed at OS-level.
  static void HandleOrReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    if (handler_->HandleCrash(signo, siginfo, context)) {
      return;
    }
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, handler_->old_actions_.ActionForSignal(signo));
  }

  Signals::OldActions old_actions_ = {};
  ExceptionInformation exception_information_ = {};
  CrashpadClient::FirstChanceHandler first_chance_handler_ = nullptr;

  static SignalHandler* handler_;

  static thread_local bool disabled_for_thread_;

  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};
SignalHandler* SignalHandler::handler_ = nullptr;
thread_local bool SignalHandler::disabled_for_thread_ = false;

// Launches a single use handler to snapshot this process.
class LaunchAtCrashHandler : public SignalHandler {
 public:
  static LaunchAtCrashHandler* Get() {
    static LaunchAtCrashHandler* instance = new LaunchAtCrashHandler();
    return instance;
  }

  bool Initialize(std::vector<std::string>* argv_in,
                  const std::vector<std::string>* envp,
                  const std::set<int>* unhandled_signals) {
    argv_strings_.swap(*argv_in);

    if (envp) {
      envp_strings_ = *envp;
      StringVectorToCStringVector(envp_strings_, &envp_);
      set_envp_ = true;
    }

    argv_strings_.push_back(FormatArgumentAddress("trace-parent-with-exception",
                                                  &GetExceptionInfo()));

    StringVectorToCStringVector(argv_strings_, &argv_);
    return Install(unhandled_signals);
  }

  void HandleCrashImpl() override {
    ScopedPrSetPtracer set_ptracer(sys_getpid(), /* may_log= */ false);

    pid_t pid = fork();
    if (pid < 0) {
      return;
    }
    if (pid == 0) {
      if (set_envp_) {
        execve(argv_[0],
               const_cast<char* const*>(argv_.data()),
               const_cast<char* const*>(envp_.data()));
      } else {
        execv(argv_[0], const_cast<char* const*>(argv_.data()));
      }
      _exit(EXIT_FAILURE);
    }

    int status;
    waitpid(pid, &status, 0);
  }

 private:
  LaunchAtCrashHandler() = default;

  ~LaunchAtCrashHandler() = delete;

  std::vector<std::string> argv_strings_;
  std::vector<const char*> argv_;
  std::vector<std::string> envp_strings_;
  std::vector<const char*> envp_;
  bool set_envp_ = false;

  DISALLOW_COPY_AND_ASSIGN(LaunchAtCrashHandler);
};

class RequestCrashDumpHandler : public SignalHandler {
 public:
  static RequestCrashDumpHandler* Get() {
    static RequestCrashDumpHandler* instance = new RequestCrashDumpHandler();
    return instance;
  }

  // pid < 0 indicates the handler pid should be determined by communicating
  // over the socket.
  // pid == 0 indicates it is not necessary to set the handler as this process'
  // ptracer. e.g. if the handler has CAP_SYS_PTRACE or if this process is in a
  // user namespace and the handler's uid matches the uid of the process that
  // created the namespace.
  // pid > 0 directly indicates what the handler's pid is expected to be, so
  // retrieving this information from the handler is not necessary.
  bool Initialize(ScopedFileHandle sock,
                  pid_t pid,
                  const std::set<int>* unhandled_signals) {
    ExceptionHandlerClient client(sock.get(), true);
    if (pid < 0) {
      ucred creds;
      if (!client.GetHandlerCredentials(&creds)) {
        return false;
      }
      pid = creds.pid;
    }
    if (pid > 0 && client.SetPtracer(pid) != 0) {
      LOG(ERROR) << "failed to set ptracer";
      return false;
    }
    sock_to_handler_.reset(sock.release());
    handler_pid_ = pid;
    return Install(unhandled_signals);
  }

  bool GetHandlerSocket(int* sock, pid_t* pid) {
    if (!sock_to_handler_.is_valid()) {
      return false;
    }
    if (sock) {
      *sock = sock_to_handler_.get();
    }
    if (pid) {
      *pid = handler_pid_;
    }
    return true;
  }

  void HandleCrashImpl() override {
    ExceptionHandlerProtocol::ClientInformation info = {};
    info.exception_information_address =
        FromPointerCast<VMAddress>(&GetExceptionInfo());
#if defined(OS_CHROMEOS)
    info.crash_loop_before_time = crash_loop_before_time_;
#endif

    ExceptionHandlerClient client(sock_to_handler_.get(), true);
    client.RequestCrashDump(info);
  }

#if defined(OS_CHROMEOS)
  void SetCrashLoopBefore(uint64_t crash_loop_before_time) {
    crash_loop_before_time_ = crash_loop_before_time;
  }
#endif

 private:
  RequestCrashDumpHandler() = default;

  ~RequestCrashDumpHandler() = delete;

  ScopedFileHandle sock_to_handler_;
  pid_t handler_pid_ = -1;

#if defined(OS_CHROMEOS)
  // An optional UNIX timestamp passed to us from Chrome.
  // This will pass to crashpad_handler and then to Chrome OS crash_reporter.
  // This should really be a time_t, but it's basically an opaque value (we
  // don't anything with it except pass it along).
  uint64_t crash_loop_before_time_ = 0;
#endif

  DISALLOW_COPY_AND_ASSIGN(RequestCrashDumpHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    bool restartable,
    bool asynchronous_start) {
  DCHECK(!asynchronous_start);

  ScopedFileHandle client_sock, handler_sock;
  if (!UnixCredentialSocket::CreateCredentialSocketpair(&client_sock,
                                                        &handler_sock)) {
    return false;
  }

  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  argv.push_back(FormatArgumentInt("initial-client-fd", handler_sock.get()));
  argv.push_back("--shared-client-connection");
  if (!DoubleForkAndExec(argv, nullptr, handler_sock.get(), false, nullptr)) {
    return false;
  }

  pid_t handler_pid = -1;
  if (!IsRegularFile(base::FilePath("/proc/sys/kernel/yama/ptrace_scope"))) {
    handler_pid = 0;
  }

  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(
      std::move(client_sock), handler_pid, &unhandled_signals_);
}

#if defined(OS_ANDROID) || defined(OS_LINUX)
// static
bool CrashpadClient::GetHandlerSocket(int* sock, pid_t* pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->GetHandlerSocket(sock, pid);
}

bool CrashpadClient::SetHandlerSocket(ScopedFileHandle sock, pid_t pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(std::move(sock), pid, &unhandled_signals_);
}
#endif  // OS_ANDROID || OS_LINUX

#if defined(OS_ANDROID)

bool CrashpadClient::StartJavaHandlerAtCrash(
    const std::string& class_name,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv = BuildAppProcessArgs(class_name,
                                                      database,
                                                      metrics_dir,
                                                      url,
                                                      annotations,
                                                      arguments,
                                                      kInvalidFileHandle);

  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, env, &unhandled_signals_);
}

// static
bool CrashpadClient::StartJavaHandlerForClient(
    const std::string& class_name,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv = BuildAppProcessArgs(
      class_name, database, metrics_dir, url, annotations, arguments, socket);
  return DoubleForkAndExec(argv, env, socket, false, nullptr);
}

bool CrashpadClient::StartHandlerWithLinkerAtCrash(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv =
      BuildArgsToLaunchWithLinker(handler_trampoline,
                                  handler_library,
                                  is_64_bit,
                                  database,
                                  metrics_dir,
                                  url,
                                  annotations,
                                  arguments,
                                  kInvalidFileHandle);
  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, env, &unhandled_signals_);
}

// static
bool CrashpadClient::StartHandlerWithLinkerForClient(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv =
      BuildArgsToLaunchWithLinker(handler_trampoline,
                                  handler_library,
                                  is_64_bit,
                                  database,
                                  metrics_dir,
                                  url,
                                  annotations,
                                  arguments,
                                  socket);
  return DoubleForkAndExec(argv, env, socket, false, nullptr);
}

#endif

bool CrashpadClient::StartHandlerAtCrash(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, nullptr, &unhandled_signals_);
}

// static
bool CrashpadClient::StartHandlerForClient(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  argv.push_back(FormatArgumentInt("initial-client-fd", socket));

  return DoubleForkAndExec(argv, nullptr, socket, true, nullptr);
}

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  if (!SignalHandler::Get()) {
    DLOG(ERROR) << "Crashpad isn't enabled";
    return;
  }

#if defined(ARCH_CPU_ARMEL)
  memset(context->uc_regspace, 0, sizeof(context->uc_regspace));
#elif defined(ARCH_CPU_ARM64)
  memset(context->uc_mcontext.__reserved,
         0,
         sizeof(context->uc_mcontext.__reserved));
#endif

  siginfo_t siginfo;
  siginfo.si_signo = Signals::kSimulatedSigno;
  siginfo.si_errno = 0;
  siginfo.si_code = 0;
  SignalHandler::Get()->HandleCrash(
      siginfo.si_signo, &siginfo, reinterpret_cast<void*>(context));
}

// static
void CrashpadClient::CrashWithoutDump(const std::string& message) {
  SignalHandler::DisableForThread();
  LOG(FATAL) << message;
}

// static
void CrashpadClient::SetFirstChanceExceptionHandler(
    FirstChanceHandler handler) {
  DCHECK(SignalHandler::Get());
  SignalHandler::Get()->SetFirstChanceHandler(handler);
}

void CrashpadClient::SetUnhandledSignals(const std::set<int>& signals) {
  DCHECK(!SignalHandler::Get());
  unhandled_signals_ = signals;
}

#if defined(OS_CHROMEOS)
// static
void CrashpadClient::SetCrashLoopBefore(uint64_t crash_loop_before_time) {
  auto request_crash_dump_handler = RequestCrashDumpHandler::Get();
  request_crash_dump_handler->SetCrashLoopBefore(crash_loop_before_time);
}
#endif

}  // namespace crashpad
