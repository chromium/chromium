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

#include <dlfcn.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "client/annotation.h"
#include "client/annotation_list.h"
#include "client/crash_report_database.h"
#include "client/simulate_crash.h"
#include "gtest/gtest.h"
#include "snapshot/annotation_snapshot.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "snapshot/sanitized/sanitization_information.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "test/multiprocess_exec.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/linux/socket.h"
#include "util/misc/address_sanitizer.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/memory_sanitizer.h"
#include "util/posix/scoped_mmap.h"
#include "util/posix/signals.h"
#include "util/thread/thread.h"

#if defined(OS_ANDROID)
#include <android/set_abort_message.h>
#include "dlfcn_internal.h"

// Normally this comes from set_abort_message.h, but only at API level 21.
extern "C" void android_set_abort_message(const char* msg)
    __attribute__((weak));
#endif

namespace crashpad {
namespace test {
namespace {

enum class CrashType : uint32_t {
  kSimulated,
  kBuiltinTrap,
  kInfiniteRecursion,
};

struct StartHandlerForSelfTestOptions {
  bool start_handler_at_crash;
  bool set_first_chance_handler;
  bool crash_non_main_thread;
  bool client_uses_signals;
  CrashType crash_type;
};

class StartHandlerForSelfTest
    : public testing::TestWithParam<
          std::tuple<bool, bool, bool, bool, CrashType>> {
 public:
  StartHandlerForSelfTest() = default;
  ~StartHandlerForSelfTest() = default;

  void SetUp() override {
    std::tie(options_.start_handler_at_crash,
             options_.set_first_chance_handler,
             options_.crash_non_main_thread,
             options_.client_uses_signals,
             options_.crash_type) = GetParam();
  }

  const StartHandlerForSelfTestOptions& Options() const { return options_; }

 private:
  StartHandlerForSelfTestOptions options_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForSelfTest);
};

bool HandleCrashSuccessfully(int, siginfo_t*, ucontext_t*) {
  return true;
}

bool InstallHandler(CrashpadClient* client,
                    bool start_at_crash,
                    const base::FilePath& handler_path,
                    const base::FilePath& database_path,
                    const std::vector<base::FilePath>& attachments) {
  return start_at_crash
             ? client->StartHandlerAtCrash(handler_path,
                                           database_path,
                                           base::FilePath(),
                                           "",
                                           std::map<std::string, std::string>(),
                                           std::vector<std::string>(),
                                           attachments)
             : client->StartHandler(handler_path,
                                    database_path,
                                    base::FilePath(),
                                    "",
                                    std::map<std::string, std::string>(),
                                    std::vector<std::string>(),
                                    false,
                                    false,
                                    attachments);
}

constexpr char kTestAnnotationName[] = "name_of_annotation";
constexpr char kTestAnnotationValue[] = "value_of_annotation";
constexpr char kTestAttachmentName[] = "test_attachment";
constexpr char kTestAttachmentContent[] = "attachment_content";

#if defined(OS_ANDROID)
constexpr char kTestAbortMessage[] = "test abort message";
#endif

void ValidateAttachment(const CrashReportDatabase::UploadReport* report) {
  auto attachments = report->GetAttachments();
  ASSERT_EQ(attachments.size(), 1u);
  char buf[sizeof(kTestAttachmentContent)];
  attachments.at(kTestAttachmentName)->Read(buf, sizeof(buf));
  ASSERT_EQ(memcmp(kTestAttachmentContent, buf, sizeof(kTestAttachmentContent)),
            0);
}

void ValidateDump(const CrashReportDatabase::UploadReport* report) {
  ProcessSnapshotMinidump minidump_snapshot;
  ASSERT_TRUE(minidump_snapshot.Initialize(report->Reader()));

#if defined(OS_ANDROID)
  // This part of the test requires Q. The API level on Q devices will be 28
  // until the API is finalized, so we can't check API level yet. For now, test
  // for the presence of a libc symbol which was introduced in Q.
  if (crashpad::internal::Dlsym(RTLD_DEFAULT, "android_fdsan_close_with_tag")) {
    const auto& annotations = minidump_snapshot.AnnotationsSimpleMap();
    auto abort_message = annotations.find("abort_message");
    ASSERT_NE(annotations.end(), abort_message);
    EXPECT_EQ(kTestAbortMessage, abort_message->second);
  }
#endif
  ValidateAttachment(report);

  for (const ModuleSnapshot* module : minidump_snapshot.Modules()) {
    for (const AnnotationSnapshot& annotation : module->AnnotationObjects()) {
      if (static_cast<Annotation::Type>(annotation.type) !=
          Annotation::Type::kString) {
        continue;
      }

      if (annotation.name == kTestAnnotationName) {
        std::string value(
            reinterpret_cast<const char*>(annotation.value.data()),
            annotation.value.size());
        EXPECT_EQ(value, kTestAnnotationValue);
        return;
      }
    }
  }
  ADD_FAILURE();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
int RecurseInfinitely(int* ptr) {
  int buf[1 << 20];
  return *ptr + RecurseInfinitely(buf);
}
#pragma clang diagnostic pop

void DoCrash(const StartHandlerForSelfTestOptions& options,
             CrashpadClient* client) {
  switch (options.crash_type) {
    case CrashType::kSimulated:
      if (options.set_first_chance_handler) {
        client->SetFirstChanceExceptionHandler(HandleCrashSuccessfully);
      }
      CRASHPAD_SIMULATE_CRASH();
      break;

    case CrashType::kBuiltinTrap:
      __builtin_trap();
      break;

    case CrashType::kInfiniteRecursion:
      int val = 42;
      exit(RecurseInfinitely(&val));
      break;
  }
}

class ScopedAltSignalStack {
 public:
  ScopedAltSignalStack() = default;

  ~ScopedAltSignalStack() {
    if (stack_mem_.is_valid()) {
      stack_t stack;
      stack.ss_flags = SS_DISABLE;
      if (sigaltstack(&stack, nullptr) != 0) {
        ADD_FAILURE() << ErrnoMessage("sigaltstack");
      }
    }
  }

  void Initialize() {
    ScopedMmap local_stack_mem;
    constexpr size_t stack_size = MINSIGSTKSZ;
    ASSERT_TRUE(local_stack_mem.ResetMmap(nullptr,
                                          stack_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS,
                                          -1,
                                          0));

    stack_t stack;
    stack.ss_sp = local_stack_mem.addr();
    stack.ss_size = stack_size;
    stack.ss_flags = 0;
    ASSERT_EQ(sigaltstack(&stack, nullptr), 0) << ErrnoMessage("sigaltstack");
    stack_mem_.ResetAddrLen(local_stack_mem.release(), stack_size);
  }

 private:
  ScopedMmap stack_mem_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAltSignalStack);
};

class CrashThread : public Thread {
 public:
  CrashThread(const StartHandlerForSelfTestOptions& options,
              CrashpadClient* client)
      : client_signal_stack_(), options_(options), client_(client) {}

 private:
  void ThreadMain() override {
    // It is only necessary to call InitializeSignalStackForThread() once, but
    // should be harmless to call multiple times and durable against the client
    // using sigaltstack() either before or after it is called.
    CrashpadClient::InitializeSignalStackForThread();
    if (options_.client_uses_signals) {
      client_signal_stack_.Initialize();
    }
    CrashpadClient::InitializeSignalStackForThread();

    DoCrash(options_, client_);
  }

  ScopedAltSignalStack client_signal_stack_;
  const StartHandlerForSelfTestOptions& options_;
  CrashpadClient* client_;

  DISALLOW_COPY_AND_ASSIGN(CrashThread);
};

CRASHPAD_CHILD_TEST_MAIN(StartHandlerForSelfTestChild) {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);

  VMSize temp_dir_length;
  CheckedReadFileExactly(in, &temp_dir_length, sizeof(temp_dir_length));

  std::string temp_dir(temp_dir_length, '\0');
  CheckedReadFileExactly(in, &temp_dir[0], temp_dir_length);

  StartHandlerForSelfTestOptions options;
  CheckedReadFileExactly(in, &options, sizeof(options));

  ScopedAltSignalStack client_signal_stack;
  if (options.client_uses_signals) {
    client_signal_stack.Initialize();

    static Signals::OldActions old_actions;
    static Signals::Handler client_handler =
        [](int signo, siginfo_t* siginfo, void*) {
          FileHandle out = StdioFileHandle(StdioStream::kStandardOutput);
          char c = 0;
          WriteFile(out, &c, sizeof(c));

          Signals::RestoreHandlerAndReraiseSignalOnReturn(
              siginfo, old_actions.ActionForSignal(signo));
        };

    CHECK(Signals::InstallCrashHandlers(
        client_handler, SA_ONSTACK, &old_actions));
  }

  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler"));

  crashpad::AnnotationList::Register();

  static StringAnnotation<32> test_annotation(kTestAnnotationName);
  test_annotation.Set(kTestAnnotationValue);

  const std::vector<base::FilePath> attachments = {
      base::FilePath(temp_dir).Append(kTestAttachmentName)};

  crashpad::CrashpadClient client;
  if (!InstallHandler(&client,
                      options.start_handler_at_crash,
                      handler_path,
                      base::FilePath(temp_dir),
                      attachments)) {
    return EXIT_FAILURE;
  }

#if defined(OS_ANDROID)
  if (android_set_abort_message) {
    android_set_abort_message(kTestAbortMessage);
  }
#endif

  if (options.crash_non_main_thread) {
    CrashThread thread(options, &client);
    thread.Start();
    thread.Join();
  } else {
    DoCrash(options, &client);
  }

  return EXIT_SUCCESS;
}

class StartHandlerForSelfInChildTest : public MultiprocessExec {
 public:
  StartHandlerForSelfInChildTest(const StartHandlerForSelfTestOptions& options)
      : MultiprocessExec(), options_(options) {
    SetChildTestMainFunction("StartHandlerForSelfTestChild");
    switch (options.crash_type) {
      case CrashType::kSimulated:
        // kTerminationNormal, EXIT_SUCCESS
        break;
      case CrashType::kBuiltinTrap:
        SetExpectedChildTerminationBuiltinTrap();
        break;
      case CrashType::kInfiniteRecursion:
        SetExpectedChildTermination(TerminationReason::kTerminationSignal,
                                    SIGSEGV);
        break;
    }
  }

 private:
  void MultiprocessParent() override {
    ScopedTempDir temp_dir;
    VMSize temp_dir_length = temp_dir.path().value().size();
    ASSERT_TRUE(LoggingWriteFile(
        WritePipeHandle(), &temp_dir_length, sizeof(temp_dir_length)));
    ASSERT_TRUE(LoggingWriteFile(
        WritePipeHandle(), temp_dir.path().value().data(), temp_dir_length));
    ASSERT_TRUE(
        LoggingWriteFile(WritePipeHandle(), &options_, sizeof(options_)));

    FileWriter writer;
    base::FilePath test_attachment_path =
        temp_dir.path().Append(kTestAttachmentName);
    bool is_created = writer.Open(test_attachment_path,
                                  FileWriteMode::kCreateOrFail,
                                  FilePermissions::kOwnerOnly);
    ASSERT_TRUE(is_created);
    writer.Write(kTestAttachmentContent, sizeof(kTestAttachmentContent));
    writer.Close();

    if (options_.client_uses_signals && !options_.set_first_chance_handler &&
        options_.crash_type != CrashType::kSimulated) {
      // Wait for child's client signal handler.
      char c;
      EXPECT_TRUE(LoggingReadFileExactly(ReadPipeHandle(), &c, sizeof(c)));
    }

    // Wait for child to finish.
    CheckedReadFileAtEOF(ReadPipeHandle());

    auto database = CrashReportDatabase::Initialize(temp_dir.path());
    ASSERT_TRUE(database);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetCompletedReports(&reports),
              CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    reports.clear();
    ASSERT_EQ(database->GetPendingReports(&reports),
              CrashReportDatabase::kNoError);
    ASSERT_EQ(reports.size(), options_.set_first_chance_handler ? 0u : 1u);

    if (options_.set_first_chance_handler) {
      return;
    }

    std::unique_ptr<const CrashReportDatabase::UploadReport> report;
    ASSERT_EQ(database->GetReportForUploading(reports[0].uuid, &report),
              CrashReportDatabase::kNoError);
    ValidateDump(report.get());
  }

  StartHandlerForSelfTestOptions options_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForSelfInChildTest);
};

TEST_P(StartHandlerForSelfTest, StartHandlerInChild) {
  if (Options().set_first_chance_handler &&
      Options().crash_type != CrashType::kSimulated) {
    // TODO(jperaza): test first chance handlers with real crashes.
    return;
  }
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
  if (Options().crash_type == CrashType::kInfiniteRecursion) {
    GTEST_SKIP();
  }
#endif  // defined(ADDRESS_SANITIZER)
  StartHandlerForSelfInChildTest test(Options());
  test.Run();
}

INSTANTIATE_TEST_SUITE_P(
    StartHandlerForSelfTestSuite,
    StartHandlerForSelfTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(CrashType::kSimulated,
                                     CrashType::kBuiltinTrap,
                                     CrashType::kInfiniteRecursion)));

// Test state for starting the handler for another process.
class StartHandlerForClientTest {
 public:
  StartHandlerForClientTest() = default;
  ~StartHandlerForClientTest() = default;

  bool Initialize(bool sanitize) {
    sanitize_ = sanitize;
    return UnixCredentialSocket::CreateCredentialSocketpair(&client_sock_,
                                                            &server_sock_);
  }

  bool StartHandlerOnDemand() {
    char c;
    if (!LoggingReadFileExactly(server_sock_.get(), &c, sizeof(c))) {
      ADD_FAILURE();
      return false;
    }

    base::FilePath handler_path = TestPaths::Executable().DirName().Append(
        FILE_PATH_LITERAL("crashpad_handler"));

    CrashpadClient client;
    if (!client.StartHandlerForClient(handler_path,
                                        temp_dir_.path(),
                                        base::FilePath(),
                                        "",
                                        std::map<std::string, std::string>(),
                                        std::vector<std::string>(),
                                        server_sock_.get())) {
      ADD_FAILURE();
      return false;
    }

    return true;
  }

  void ExpectReport() {
    auto database =
        CrashReportDatabase::InitializeWithoutCreating(temp_dir_.path());
    ASSERT_TRUE(database);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetCompletedReports(&reports),
              CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    reports.clear();
    ASSERT_EQ(database->GetPendingReports(&reports),
              CrashReportDatabase::kNoError);
    if (sanitize_) {
      EXPECT_EQ(reports.size(), 0u);
    } else {
      EXPECT_EQ(reports.size(), 1u);
    }
  }

  bool InstallHandler() {
    auto signal_handler = SandboxedHandler::Get();
    return signal_handler->Initialize(client_sock_.get(), sanitize_);
  }

 private:
  // A signal handler that defers handler process startup to another, presumably
  // more privileged, process.
  class SandboxedHandler {
   public:
    static SandboxedHandler* Get() {
      static SandboxedHandler* instance = new SandboxedHandler();
      return instance;
    }

    bool Initialize(FileHandle client_sock, bool sanitize) {
      client_sock_ = client_sock;
      sanitize_ = sanitize;
      return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
    }

   private:
    SandboxedHandler() = default;
    ~SandboxedHandler() = delete;

    static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
      auto state = Get();

      char c = 0;
      CHECK(LoggingWriteFile(state->client_sock_, &c, sizeof(c)));

      ExceptionInformation exception_information;
      exception_information.siginfo_address =
          FromPointerCast<decltype(exception_information.siginfo_address)>(
              siginfo);
      exception_information.context_address =
          FromPointerCast<decltype(exception_information.context_address)>(
              context);
      exception_information.thread_id = syscall(SYS_gettid);

      ExceptionHandlerProtocol::ClientInformation info;
      info.exception_information_address =
          FromPointerCast<decltype(info.exception_information_address)>(
              &exception_information);

      SanitizationInformation sanitization_info = {};
      if (state->sanitize_) {
        info.sanitization_information_address =
            FromPointerCast<VMAddress>(&sanitization_info);
        // Target a non-module address to prevent a crash dump.
        sanitization_info.target_module_address =
            FromPointerCast<VMAddress>(&sanitization_info);
      }

      ExceptionHandlerClient handler_client(state->client_sock_, false);
      CHECK_EQ(handler_client.RequestCrashDump(info), 0);

      Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
    }

    FileHandle client_sock_;
    bool sanitize_;

    DISALLOW_COPY_AND_ASSIGN(SandboxedHandler);
  };

  ScopedTempDir temp_dir_;
  ScopedFileHandle client_sock_;
  ScopedFileHandle server_sock_;
  bool sanitize_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForClientTest);
};

// Tests starting the handler for a child process.
class StartHandlerForChildTest : public Multiprocess {
 public:
  StartHandlerForChildTest() = default;
  ~StartHandlerForChildTest() = default;

  bool Initialize(bool sanitize) {
    SetExpectedChildTerminationBuiltinTrap();
    return test_state_.Initialize(sanitize);
  }

 private:
  void MultiprocessParent() {
    ASSERT_TRUE(test_state_.StartHandlerOnDemand());

    // Wait for chlid to finish.
    CheckedReadFileAtEOF(ReadPipeHandle());

    test_state_.ExpectReport();
  }

  void MultiprocessChild() {
    CHECK(test_state_.InstallHandler());

    __builtin_trap();

    NOTREACHED();
  }

  StartHandlerForClientTest test_state_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForChildTest);
};

TEST(CrashpadClient, StartHandlerForChild) {
  StartHandlerForChildTest test;
  ASSERT_TRUE(test.Initialize(/* sanitize= */ false));
  test.Run();
}

TEST(CrashpadClient, SanitizedChild) {
  StartHandlerForChildTest test;
  ASSERT_TRUE(test.Initialize(/* sanitize= */ true));
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
