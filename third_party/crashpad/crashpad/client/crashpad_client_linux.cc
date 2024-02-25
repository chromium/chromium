// Copyright 2018 The Crashpad Authors
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "client/client_argv_handling.h"
#include "third_party/lss/lss.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/linux/scoped_pr_set_dumpable.h"
#include "util/linux/scoped_pr_set_ptracer.h"
#include "util/linux/socket.h"
#include "util/misc/address_sanitizer.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/scoped_mmap.h"
#include "util/posix/signals.h"
#include "util/posix/spawn_subprocess.h"

namespace crashpad {

namespace {

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

std::string FormatArgumentAddress(const std::string& name, const void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

#if BUILDFLAG(IS_ANDROID)

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

#endif  // BUILDFLAG(IS_ANDROID)

using LastChanceHandler = bool (*)(int, siginfo_t*, ucontext_t*);

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  SignalHandler(const SignalHandler&) = delete;
  SignalHandler& operator=(const SignalHandler&) = delete;

  // Returns the currently installed signal hander. May be `nullptr` if no
  // handler has been installed.
  static SignalHandler* Get() { return handler_; }

  // Disables any installed Crashpad signal handler. If a crash signal is
  // received, any previously installed (non-Crashpad) signal handler will be
  // restored and the signal reraised.
  static void Disable() {
    if (!handler_->disabled_.test_and_set()) {
      handler_->WakeThreads();
    }
  }

  void SetFirstChanceHandler(CrashpadClient::FirstChanceHandler handler) {
    first_chance_handler_ = handler;
  }

  void SetLastChanceExceptionHandler(LastChanceHandler handler) {
    last_chance_handler_ = handler;
  }

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    exception_information_.siginfo_address =
        FromPointerCast<decltype(exception_information_.siginfo_address)>(
            siginfo);
    exception_information_.context_address =
        FromPointerCast<decltype(exception_information_.context_address)>(
            context);
    exception_information_.thread_id = sys_gettid();

    ScopedPrSetDumpable set_dumpable(false);
    HandleCrashImpl();
  }

 protected:
  SignalHandler() = default;
  ~SignalHandler() = default;

  bool Install(const std::set<int>* unhandled_signals) {
    bool signal_stack_initialized =
        CrashpadClient::InitializeSignalStackForThread();
    DCHECK(signal_stack_initialized);

    DCHECK(!handler_);
    handler_ = this;
    return Signals::InstallCrashHandlers(HandleOrReraiseSignal,
                                         SA_ONSTACK | SA_EXPOSE_TAGBITS,
                                         &old_actions_,
                                         unhandled_signals);
  }

  const ExceptionInformation& GetExceptionInfo() {
    return exception_information_;
  }

  virtual void HandleCrashImpl() = 0;

 private:
  static constexpr int32_t kDumpNotDone = 0;
  static constexpr int32_t kDumpDone = 1;

  // The signal handler installed at OS-level.
  static void HandleOrReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    if (handler_->first_chance_handler_ &&
        handler_->first_chance_handler_(
            signo, siginfo, static_cast<ucontext_t*>(context))) {
      return;
    }

    // Only handle the first fatal signal observed. If another thread receives a
    // crash signal, it waits for the first dump to complete instead of
    // requesting another.
    if (!handler_->disabled_.test_and_set()) {
      handler_->HandleCrash(signo, siginfo, context);
      handler_->WakeThreads();
      if (handler_->last_chance_handler_ &&
          handler_->last_chance_handler_(
              signo, siginfo, static_cast<ucontext_t*>(context))) {
        return;
      }
    } else {
      // Processes on Android normally have several chained signal handlers that
      // co-operate to report crashes. e.g. WebView will have this signal
      // handler installed, the app embedding WebView may have a signal handler
      // installed, and Bionic will have a signal handler. Each signal handler
      // runs in succession, possibly managed by libsigchain. This wait is
      // intended to avoid ill-effects from multiple signal handlers from
      // different layers (possibly all trying to use ptrace()) from running
      // simultaneously. It does not block forever so that in most conditions,
      // those signal handlers will still have a chance to run and ensures
      // process termination in case the first crashing thread crashes again in
      // its signal handler. Though less typical, this situation also occurs on
      // other Linuxes, e.g. to produce in-process stack traces for debug
      // builds.
      handler_->WaitForDumpDone();
    }

    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, handler_->old_actions_.ActionForSignal(signo));
  }

  void WaitForDumpDone() {
    kernel_timespec timeout;
    timeout.tv_sec = 5;
    timeout.tv_nsec = 0;
    sys_futex(&dump_done_futex_,
              FUTEX_WAIT_PRIVATE,
              kDumpNotDone,
              &timeout,
              nullptr,
              0);
  }

  void WakeThreads() {
    dump_done_futex_ = kDumpDone;
    sys_futex(
        &dump_done_futex_, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
  }

  Signals::OldActions old_actions_ = {};
  ExceptionInformation exception_information_ = {};
  CrashpadClient::FirstChanceHandler first_chance_handler_ = nullptr;
  LastChanceHandler last_chance_handler_ = nullptr;
  int32_t dump_done_futex_ = kDumpNotDone;
#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization < 201911L
  std::atomic_flag disabled_ = ATOMIC_FLAG_INIT;
#else
  std::atomic_flag disabled_;
#endif

  static SignalHandler* handler_;
};
SignalHandler* SignalHandler::handler_ = nullptr;

// Launches a single use handler to snapshot this process.
class LaunchAtCrashHandler : public SignalHandler {
 public:
  LaunchAtCrashHandler(const LaunchAtCrashHandler&) = delete;
  LaunchAtCrashHandler& operator=(const LaunchAtCrashHandler&) = delete;

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
};

class RequestCrashDumpHandler : public SignalHandler {
 public:
  RequestCrashDumpHandler(const RequestCrashDumpHandler&) = delete;
  RequestCrashDumpHandler& operator=(const RequestCrashDumpHandler&) = delete;

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
    if (pid > 0) {
      pthread_atfork(nullptr, nullptr, SetPtracerAtFork);
      if (prctl(PR_SET_PTRACER, pid, 0, 0, 0) != 0) {
        PLOG(WARNING) << "prctl";
      }
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
    // Attempt to set the ptracer again, in case a crash occurs after a fork,
    // before SetPtracerAtFork() has been called. Ignore errors because the
    // system call may be disallowed if the sandbox is engaged.
    if (handler_pid_ > 0) {
      sys_prctl(PR_SET_PTRACER, handler_pid_, 0, 0, 0);
    }

    ExceptionHandlerProtocol::ClientInformation info = {};
    info.exception_information_address =
        FromPointerCast<VMAddress>(&GetExceptionInfo());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    info.crash_loop_before_time = crash_loop_before_time_;
#endif

    ExceptionHandlerClient client(sock_to_handler_.get(), true);
    client.RequestCrashDump(info);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetCrashLoopBefore(uint64_t crash_loop_before_time) {
    crash_loop_before_time_ = crash_loop_before_time;
  }
#endif

 private:
  RequestCrashDumpHandler() = default;

  ~RequestCrashDumpHandler() = delete;

  static void SetPtracerAtFork() {
    auto handler = RequestCrashDumpHandler::Get();
    if (handler->handler_pid_ > 0 &&
        prctl(PR_SET_PTRACER, handler->handler_pid_, 0, 0, 0) != 0) {
      PLOG(WARNING) << "prctl";
    }
  }

  ScopedFileHandle sock_to_handler_;
  pid_t handler_pid_ = -1;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // An optional UNIX timestamp passed to us from Chrome.
  // This will pass to crashpad_handler and then to Chrome OS crash_reporter.
  // This should really be a time_t, but it's basically an opaque value (we
  // don't anything with it except pass it along).
  uint64_t crash_loop_before_time_ = 0;
#endif
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
    bool asynchronous_start,
    const std::vector<base::FilePath>& attachments) {
  DCHECK(!asynchronous_start);

  ScopedFileHandle client_sock, handler_sock;
  if (!UnixCredentialSocket::CreateCredentialSocketpair(&client_sock,
                                                        &handler_sock)) {
    return false;
  }

  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments, attachments);

  argv.push_back(FormatArgumentInt("initial-client-fd", handler_sock.get()));
  argv.push_back("--shared-client-connection");
  if (!SpawnSubprocess(argv, nullptr, handler_sock.get(), false, nullptr)) {
    return false;
  }
  handler_sock.reset();

  pid_t handler_pid = -1;
  if (!IsRegularFile(base::FilePath("/proc/sys/kernel/yama/ptrace_scope"))) {
    handler_pid = 0;
  }

  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(
      std::move(client_sock), handler_pid, &unhandled_signals_);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
bool CrashpadClient::GetHandlerSocket(int* sock, pid_t* pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->GetHandlerSocket(sock, pid);
}

bool CrashpadClient::SetHandlerSocket(ScopedFileHandle sock, pid_t pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(std::move(sock), pid, &unhandled_signals_);
}

// static
bool CrashpadClient::InitializeSignalStackForThread() {
  stack_t stack;
  if (sigaltstack(nullptr, &stack) != 0) {
    PLOG(ERROR) << "sigaltstack";
    return false;
  }

  DCHECK_EQ(stack.ss_flags & SS_ONSTACK, 0);

  const size_t page_size = getpagesize();
#if defined(ADDRESS_SANITIZER)
  const size_t kStackSize = 2 * ((SIGSTKSZ + page_size - 1) & ~(page_size - 1));
#else
  const size_t kStackSize = (SIGSTKSZ + page_size - 1) & ~(page_size - 1);
#endif  // ADDRESS_SANITIZER
  if (stack.ss_flags & SS_DISABLE || stack.ss_size < kStackSize) {
    const size_t kGuardPageSize = page_size;
    const size_t kStackAllocSize = kStackSize + 2 * kGuardPageSize;

    static void (*stack_destructor)(void*) = [](void* stack_mem) {
      const size_t page_size = getpagesize();
      const size_t kGuardPageSize = page_size;
#if defined(ADDRESS_SANITIZER)
      const size_t kStackSize =
          2 * ((SIGSTKSZ + page_size - 1) & ~(page_size - 1));
#else
      const size_t kStackSize = (SIGSTKSZ + page_size - 1) & ~(page_size - 1);
#endif  // ADDRESS_SANITIZER
      const size_t kStackAllocSize = kStackSize + 2 * kGuardPageSize;

      stack_t stack;
      stack.ss_flags = SS_DISABLE;
      if (sigaltstack(&stack, &stack) != 0) {
        PLOG(ERROR) << "sigaltstack";
      } else if (stack.ss_sp !=
                 static_cast<char*>(stack_mem) + kGuardPageSize) {
        PLOG_IF(ERROR, sigaltstack(&stack, nullptr) != 0) << "sigaltstack";
      }

      if (munmap(stack_mem, kStackAllocSize) != 0) {
        PLOG(ERROR) << "munmap";
      }
    };

    static pthread_key_t stack_key;
    static int key_error = []() {
      errno = pthread_key_create(&stack_key, stack_destructor);
      PLOG_IF(ERROR, errno) << "pthread_key_create";
      return errno;
    }();
    if (key_error) {
      return false;
    }

    auto old_stack = static_cast<char*>(pthread_getspecific(stack_key));
    if (old_stack) {
      stack.ss_sp = old_stack + kGuardPageSize;
    } else {
      ScopedMmap stack_mem;
      if (!stack_mem.ResetMmap(nullptr,
                               kStackAllocSize,
                               PROT_NONE,
                               MAP_PRIVATE | MAP_ANONYMOUS,
                               -1,
                               0)) {
        return false;
      }

      if (mprotect(stack_mem.addr_as<char*>() + kGuardPageSize,
                   kStackSize,
                   PROT_READ | PROT_WRITE) != 0) {
        PLOG(ERROR) << "mprotect";
        return false;
      }

      stack.ss_sp = stack_mem.addr_as<char*>() + kGuardPageSize;

      errno = pthread_setspecific(stack_key, stack_mem.release());
      PCHECK(errno == 0) << "pthread_setspecific";
    }

    stack.ss_size = kStackSize;
    stack.ss_flags =
        (stack.ss_flags & SS_DISABLE) ? 0 : stack.ss_flags & SS_AUTODISARM;
    if (sigaltstack(&stack, nullptr) != 0) {
      PLOG(ERROR) << "sigaltstack";
      return false;
    }
  }
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)

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
  return SpawnSubprocess(argv, env, socket, false, nullptr);
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
  return SpawnSubprocess(argv, env, socket, false, nullptr);
}

#endif

bool CrashpadClient::StartHandlerAtCrash(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    const std::vector<base::FilePath>& attachments) {
  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments, attachments);

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

  return SpawnSubprocess(argv, nullptr, socket, true, nullptr);
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
  SignalHandler::Disable();
  LOG(FATAL) << message;
}

// static
void CrashpadClient::SetFirstChanceExceptionHandler(
    FirstChanceHandler handler) {
  DCHECK(SignalHandler::Get());
  SignalHandler::Get()->SetFirstChanceHandler(handler);
}

// static
void CrashpadClient::SetLastChanceExceptionHandler(LastChanceHandler handler) {
  DCHECK(SignalHandler::Get());
  SignalHandler::Get()->SetLastChanceExceptionHandler(handler);
}

void CrashpadClient::SetUnhandledSignals(const std::set<int>& signals) {
  DCHECK(!SignalHandler::Get());
  unhandled_signals_ = signals;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void CrashpadClient::SetCrashLoopBefore(uint64_t crash_loop_before_time) {
  auto request_crash_dump_handler = RequestCrashDumpHandler::Get();
  request_crash_dump_handler->SetCrashLoopBefore(crash_loop_before_time);
}
#endif

}  // namespace crashpad
