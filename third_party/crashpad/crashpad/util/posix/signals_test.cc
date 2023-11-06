// Copyright 2017 The Crashpad Authors
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

#include "util/posix/signals.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <iterator>
#include <limits>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "test/scoped_temp_dir.h"
#include "util/posix/scoped_mmap.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#include <sys/auxv.h>
#include <sys/prctl.h>

#if defined(ARCH_CPU_ARM64)
#ifndef HWCAP2_MTE
#define HWCAP2_MTE (1 << 18)
#endif
#ifndef SEGV_MTEAERR
#define SEGV_MTEAERR 8
#endif
#ifndef PROT_MTE
#define PROT_MTE 0x20
#endif
#ifndef PR_SET_TAGGED_ADDR_CTRL
#define PR_SET_TAGGED_ADDR_CTRL 55
#endif
#ifndef PR_TAGGED_ADDR_ENABLE
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
#endif
#ifndef PR_MTE_TCF_ASYNC
#define PR_MTE_TCF_ASYNC (1UL << 2)
#endif
#endif  // defined(ARCH_CPU_ARM64)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace crashpad {
namespace test {
namespace {

constexpr int kUnexpectedExitStatus = 3;

struct TestableSignal {
  int sig, code;
};

// Keep synchronized with CauseSignal().
std::vector<TestableSignal> TestableSignals() {
  std::vector<TestableSignal> signals;
  signals.push_back({SIGABRT, 0});
  signals.push_back({SIGALRM, 0});
  signals.push_back({SIGBUS, 0});
/* According to DDI0487D (Armv8 Architecture Reference Manual) the expected
 * behavior for division by zero (Section 3.4.8) is: "... results in a
 * zero being written to the destination register, without any
 * indication that the division by zero occurred.".
 * This applies to Armv8 (and not earlier) for both 32bit and 64bit app code.
 */
#if defined(ARCH_CPU_X86_FAMILY)
  signals.push_back({SIGFPE, 0});
#endif
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)
  signals.push_back({SIGILL, 0});
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)
  signals.push_back({SIGPIPE, 0});
  signals.push_back({SIGSEGV, 0});
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || \
     BUILDFLAG(IS_CHROMEOS)) &&                      \
    defined(ARCH_CPU_ARM64)
  if (getauxval(AT_HWCAP2) & HWCAP2_MTE) {
    signals.push_back({SIGSEGV, SEGV_MTEAERR});
  }
#endif
#if BUILDFLAG(IS_APPLE)
  signals.push_back({SIGSYS, 0});
#endif  // BUILDFLAG(IS_APPLE)
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
  signals.push_back({SIGTRAP, 0});
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
  return signals;
}

// Keep synchronized with TestableSignals().
void CauseSignal(int sig, int code) {
  switch (sig) {
    case SIGABRT: {
      abort();
    }

    case SIGALRM: {
      struct itimerval itimer = {};
      itimer.it_value.tv_usec = 1E3;  // 1 millisecond
      if (setitimer(ITIMER_REAL, &itimer, nullptr) != 0) {
        PLOG(ERROR) << "setitimer";
        _exit(kUnexpectedExitStatus);
      }

      while (true) {
        sleep(std::numeric_limits<unsigned int>::max());
      }
    }

    case SIGBUS: {
      ScopedMmap mapped_file;
      {
        base::ScopedFD fd;
        {
          ScopedTempDir temp_dir;
          fd.reset(open(temp_dir.path().Append("empty").value().c_str(),
                        O_RDWR | O_CREAT | O_EXCL | O_NOCTTY | O_CLOEXEC,
                        0644));
          if (fd.get() < 0) {
            PLOG(ERROR) << "open";
          }
        }
        if (fd.get() < 0) {
          _exit(kUnexpectedExitStatus);
        }

        if (!mapped_file.ResetMmap(nullptr,
                                   getpagesize(),
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE,
                                   fd.get(),
                                   0)) {
          _exit(kUnexpectedExitStatus);
        }
      }

      *mapped_file.addr_as<char*>() = 0;

      _exit(kUnexpectedExitStatus);
    }

    case SIGFPE: {
/* Enabled only for x86, since a division by zero won't raise a signal
 * on Armv8, please see comment at the top of file concerning the
 * Arm architecture.
 */
#if defined(ARCH_CPU_X86_FAMILY)
      // Dividing by zero is undefined in C, so the compiler is permitted to
      // optimize out the division. Instead, divide using inline assembly. As
      // this instruction will trap anyway, we skip declaring any clobbers or
      // output registers.
      int a = 42, b = 0;
      asm volatile("idivl %2" : : "a"(0), "d"(a), "r"(b));
#endif
      break;
    }

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)
    case SIGILL: {
      // __builtin_trap() causes SIGTRAP on arm64 on Android.
      __builtin_trap();
    }
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)

    case SIGPIPE: {
      int pipe_fds[2];
      if (pipe(pipe_fds) != 0) {
        PLOG(ERROR) << "pipe";
        _exit(kUnexpectedExitStatus);
      }

      if (close(pipe_fds[0]) != 0) {
        PLOG(ERROR) << "close";
        _exit(kUnexpectedExitStatus);
      }

      char c = 0;
      ssize_t rv = write(pipe_fds[1], &c, sizeof(c));
      if (rv < 0) {
        PLOG(ERROR) << "write";
        _exit(kUnexpectedExitStatus);
      } else if (rv != sizeof(c)) {
        LOG(ERROR) << "write";
        _exit(kUnexpectedExitStatus);
      }
      break;
    }

    case SIGSEGV: {
      switch (code) {
        case 0: {
          volatile int* i = nullptr;
          *i = 0;
          break;
        }
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || \
     BUILDFLAG(IS_CHROMEOS)) &&                      \
    defined(ARCH_CPU_ARM64)
        case SEGV_MTEAERR: {
          ScopedMmap mapping;
          if (!mapping.ResetMmap(nullptr,
                                 getpagesize(),
                                 PROT_READ | PROT_WRITE | PROT_MTE,
                                 MAP_PRIVATE | MAP_ANON,
                                 -1,
                                 0)) {
            _exit(kUnexpectedExitStatus);
          }
          if (prctl(PR_SET_TAGGED_ADDR_CTRL,
                PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC,
                0,
                0,
                0) != 0) {
            _exit(kUnexpectedExitStatus);
          }
          mapping.addr_as<char*>()[1ULL << 56] = 0;
          break;
        }
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)) && defined(ARCH_CPU_ARM64)
      }
      break;
    }

#if BUILDFLAG(IS_APPLE)
    case SIGSYS: {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      int rv = syscall(4095);
#pragma clang diagnostic pop
      if (rv != 0) {
        PLOG(ERROR) << "syscall";
        _exit(kUnexpectedExitStatus);
      }
      break;
    }
#endif  // BUILDFLAG(IS_APPLE)

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
    case SIGTRAP: {
#if defined(ARCH_CPU_X86_FAMILY)
      asm("int3");
#elif defined(ARCH_CPU_ARM64)
      // bkpt #0 should work for 32-bit ARCH_CPU_ARMEL, but according to
      // https://crrev.com/f53167270c44, it only causes SIGTRAP on Linux under a
      // 64-bit kernel. For a pure 32-bit armv7 system, it generates SIGBUS.
      asm("brk #0");
#endif
      break;
    }
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)

    default: {
      LOG(ERROR) << "unexpected signal " << sig;
      _exit(kUnexpectedExitStatus);
    }
  }
}

class SignalsTest : public Multiprocess {
 public:
  enum class SignalSource {
    kCause,
    kRaise,
  };
  enum class TestType {
    kDefaultHandler,
    kHandlerExits,
    kHandlerReraisesToDefault,
    kHandlerReraisesToPrevious,
  };
  static constexpr int kExitingHandlerExitStatus = 2;

  SignalsTest(TestType test_type, SignalSource signal_source, int sig, int code)
      : Multiprocess(),
        sig_(sig),
        code_(code),
        test_type_(test_type),
        signal_source_(signal_source) {}

  SignalsTest(const SignalsTest&) = delete;
  SignalsTest& operator=(const SignalsTest&) = delete;

  ~SignalsTest() {}

 private:
  static void SignalHandler_Exit(int sig, siginfo_t* siginfo, void* context) {
    _exit(kExitingHandlerExitStatus);
  }

  static void SignalHandler_ReraiseToDefault(int sig,
                                             siginfo_t* siginfo,
                                             void* context) {
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
  }

  static void SignalHandler_ReraiseToPrevious(int sig,
                                              siginfo_t* siginfo,
                                              void* context) {
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, old_actions_.ActionForSignal(sig));
  }

  // Multiprocess:
  void MultiprocessParent() override {}

  void MultiprocessChild() override {
    bool (*install_handlers)(Signals::Handler, int, Signals::OldActions*);
    if (Signals::IsCrashSignal(sig_)) {
      install_handlers = [](Signals::Handler handler,
                            int flags,
                            Signals::OldActions* old_actions) {
        return Signals::InstallCrashHandlers(
            handler, flags, old_actions, nullptr);
      };
    } else if (Signals::IsTerminateSignal(sig_)) {
      install_handlers = Signals::InstallTerminateHandlers;
    } else {
      _exit(kUnexpectedExitStatus);
    }

    switch (test_type_) {
      case TestType::kDefaultHandler: {
        // Don’t rely on the default handler being active. Something may have
        // changed it (particularly on Android).
        struct sigaction action;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        action.sa_handler = SIG_DFL;
        ASSERT_EQ(sigaction(sig_, &action, nullptr), 0)
            << ErrnoMessage("sigaction");
        break;
      }

      case TestType::kHandlerExits: {
        ASSERT_TRUE(install_handlers(SignalHandler_Exit, 0, nullptr));
        break;
      }

      case TestType::kHandlerReraisesToDefault: {
        ASSERT_TRUE(
            install_handlers(SignalHandler_ReraiseToDefault, 0, nullptr));
        break;
      }

      case TestType::kHandlerReraisesToPrevious: {
        ASSERT_TRUE(install_handlers(SignalHandler_Exit, 0, nullptr));
        ASSERT_TRUE(install_handlers(
            SignalHandler_ReraiseToPrevious, 0, &old_actions_));
        break;
      }
    }

    switch (signal_source_) {
      case SignalSource::kCause:
        CauseSignal(sig_, code_);
        break;
      case SignalSource::kRaise:
        raise(sig_);
        break;
    }

    _exit(kUnexpectedExitStatus);
  }

  int sig_;
  int code_;
  TestType test_type_;
  SignalSource signal_source_;
  static Signals::OldActions old_actions_;
};

Signals::OldActions SignalsTest::old_actions_;

bool ShouldTestSignal(int sig) {
  return Signals::IsCrashSignal(sig) || Signals::IsTerminateSignal(sig);
}

TEST(Signals, WillSignalReraiseAutonomously) {
  const struct {
    int sig;
    int code;
    bool result;
  } kTestData[] = {
      {SIGBUS, BUS_ADRALN, true},
      {SIGFPE, FPE_FLTDIV, true},
      {SIGILL, ILL_ILLOPC, true},
      {SIGSEGV, SEGV_MAPERR, true},
      {SIGBUS, 0, false},
      {SIGFPE, -1, false},
      {SIGILL, SI_USER, false},
      {SIGSEGV, SI_QUEUE, false},
      {SIGTRAP, TRAP_BRKPT, false},
      {SIGHUP, SEGV_MAPERR, false},
      {SIGINT, SI_USER, false},
  };
  for (size_t index = 0; index < std::size(kTestData); ++index) {
    const auto test_data = kTestData[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %zu, sig %d, code %d", index, test_data.sig, test_data.code));
    siginfo_t siginfo = {};
    siginfo.si_signo = test_data.sig;
    siginfo.si_code = test_data.code;
    EXPECT_EQ(Signals::WillSignalReraiseAutonomously(&siginfo),
              test_data.result);
  }
}

TEST(Signals, Cause_DefaultHandler) {
  for (TestableSignal s : TestableSignals()) {
    SCOPED_TRACE(base::StringPrintf(
        "sig %d (%s), code %d", s.sig, strsignal(s.sig), s.code));

    SignalsTest test(SignalsTest::TestType::kDefaultHandler,
                     SignalsTest::SignalSource::kCause,
                     s.sig,
                     s.code);
    test.SetExpectedChildTermination(Multiprocess::kTerminationSignal, s.sig);
    test.Run();
  }
}

TEST(Signals, Cause_HandlerExits) {
  for (TestableSignal s : TestableSignals()) {
    SCOPED_TRACE(base::StringPrintf(
        "sig %d (%s), code %d", s.sig, strsignal(s.sig), s.code));

    SignalsTest test(SignalsTest::TestType::kHandlerExits,
                     SignalsTest::SignalSource::kCause,
                     s.sig,
                     s.code);
    test.SetExpectedChildTermination(Multiprocess::kTerminationNormal,
                                     SignalsTest::kExitingHandlerExitStatus);
    test.Run();
  }
}

TEST(Signals, Cause_HandlerReraisesToDefault) {
  for (TestableSignal s : TestableSignals()) {
    SCOPED_TRACE(base::StringPrintf(
        "sig %d (%s), code %d", s.sig, strsignal(s.sig), s.code));

    SignalsTest test(SignalsTest::TestType::kHandlerReraisesToDefault,
                     SignalsTest::SignalSource::kCause,
                     s.sig,
                     s.code);
    test.SetExpectedChildTermination(Multiprocess::kTerminationSignal, s.sig);
    test.Run();
  }
}

TEST(Signals, Cause_HandlerReraisesToPrevious) {
  for (TestableSignal s : TestableSignals()) {
    SCOPED_TRACE(base::StringPrintf(
        "sig %d (%s), code %d", s.sig, strsignal(s.sig), s.code));

    SignalsTest test(SignalsTest::TestType::kHandlerReraisesToPrevious,
                     SignalsTest::SignalSource::kCause,
                     s.sig,
                     s.code);
    test.SetExpectedChildTermination(Multiprocess::kTerminationNormal,
                                     SignalsTest::kExitingHandlerExitStatus);
    test.Run();
  }
}

TEST(Signals, Raise_DefaultHandler) {
  for (int sig = 1; sig < NSIG; ++sig) {
    SCOPED_TRACE(base::StringPrintf("sig %d (%s)", sig, strsignal(sig)));

    if (!ShouldTestSignal(sig)) {
      continue;
    }

    SignalsTest test(SignalsTest::TestType::kDefaultHandler,
                     SignalsTest::SignalSource::kRaise,
                     sig,
                     0);
    test.SetExpectedChildTermination(Multiprocess::kTerminationSignal, sig);
    test.Run();
  }
}

TEST(Signals, Raise_HandlerExits) {
  for (int sig = 1; sig < NSIG; ++sig) {
    SCOPED_TRACE(base::StringPrintf("sig %d (%s)", sig, strsignal(sig)));

    if (!ShouldTestSignal(sig)) {
      continue;
    }

    SignalsTest test(SignalsTest::TestType::kHandlerExits,
                     SignalsTest::SignalSource::kRaise,
                     sig,
                     0);
    test.SetExpectedChildTermination(Multiprocess::kTerminationNormal,
                                     SignalsTest::kExitingHandlerExitStatus);
    test.Run();
  }
}

TEST(Signals, Raise_HandlerReraisesToDefault) {
  for (int sig = 1; sig < NSIG; ++sig) {
    SCOPED_TRACE(base::StringPrintf("sig %d (%s)", sig, strsignal(sig)));

    if (!ShouldTestSignal(sig)) {
      continue;
    }

#if BUILDFLAG(IS_APPLE)
    if (sig == SIGBUS
#if defined(ARCH_CPU_ARM64)
        || sig == SIGILL || sig == SIGSEGV
#endif  // defined(ARCH_CPU_ARM64)
       ) {
      // Signal handlers can’t distinguish between these signals arising out of
      // hardware faults and raised asynchronously.
      // Signals::RestoreHandlerAndReraiseSignalOnReturn() assumes that they
      // come from hardware faults, but this test uses raise(), so the re-raise
      // test must be skipped.
      continue;
    }
#endif  // BUILDFLAG(IS_APPLE)

    SignalsTest test(SignalsTest::TestType::kHandlerReraisesToDefault,
                     SignalsTest::SignalSource::kRaise,
                     sig,
                     0);
    test.SetExpectedChildTermination(Multiprocess::kTerminationSignal, sig);
    test.Run();
  }
}

TEST(Signals, Raise_HandlerReraisesToPrevious) {
  for (int sig = 1; sig < NSIG; ++sig) {
    SCOPED_TRACE(base::StringPrintf("sig %d (%s)", sig, strsignal(sig)));

    if (!ShouldTestSignal(sig)) {
      continue;
    }

#if BUILDFLAG(IS_APPLE)
    if (sig == SIGBUS
#if defined(ARCH_CPU_ARM64)
        || sig == SIGILL || sig == SIGSEGV
#endif  // defined(ARCH_CPU_ARM64)
       ) {
      // Signal handlers can’t distinguish between these signals arising out of
      // hardware faults and raised asynchronously.
      // Signals::RestoreHandlerAndReraiseSignalOnReturn() assumes that they
      // come from hardware faults, but this test uses raise(), so the re-raise
      // test must be skipped.
      continue;
    }
#endif  // BUILDFLAG(IS_APPLE)

    SignalsTest test(SignalsTest::TestType::kHandlerReraisesToPrevious,
                     SignalsTest::SignalSource::kRaise,
                     sig,
                     0);
    test.SetExpectedChildTermination(Multiprocess::kTerminationNormal,
                                     SignalsTest::kExitingHandlerExitStatus);
    test.Run();
  }
}

TEST(Signals, IsCrashSignal) {
  // Always crash signals.
  EXPECT_TRUE(Signals::IsCrashSignal(SIGABRT));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGBUS));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGFPE));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGILL));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGQUIT));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGSEGV));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGSYS));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGTRAP));

  // Always terminate signals.
  EXPECT_FALSE(Signals::IsCrashSignal(SIGALRM));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGHUP));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGINT));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGPIPE));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGPROF));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTERM));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGUSR1));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGUSR2));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGVTALRM));

  // Never crash or terminate signals.
  EXPECT_FALSE(Signals::IsCrashSignal(SIGCHLD));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGCONT));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTSTP));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTTIN));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTTOU));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGURG));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGWINCH));
}

TEST(Signals, IsTerminateSignal) {
  // Always terminate signals.
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGALRM));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGHUP));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGINT));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGPIPE));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGPROF));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGTERM));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGUSR1));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGUSR2));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGVTALRM));

  // Always crash signals.
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGABRT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGBUS));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGFPE));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGILL));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGQUIT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGSEGV));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGSYS));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTRAP));

  // Never crash or terminate signals.
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGCHLD));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGCONT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTSTP));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTTIN));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTTOU));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGURG));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGWINCH));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
