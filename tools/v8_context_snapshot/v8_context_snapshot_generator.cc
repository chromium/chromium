// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "gin/v8_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_bundled_code_cache_generator.h"
#include "third_party/blink/public/web/web_v8_context_snapshot.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8.h"

namespace {

class SnapshotPlatform final : public blink::Platform {
 public:
  explicit SnapshotPlatform(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  bool IsTakingV8ContextSnapshot() override { return true; }

  // Required for binders to work, to ensure deterministic snapshot generation
  // we have everything operate on a single thread.
  scoped_refptr<base::SequencedTaskRunner> MediaThreadTaskRunner() override {
    return task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class CodeCacheGenPlatform final : public blink::Platform {
 public:
  explicit CodeCacheGenPlatform(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // blink::Platform:
  bool DisallowV8FeatureFlagOverrides() const override { return true; }

  // Required for binders to work, to ensure deterministic code cache generation
  // we have everything operate on a single thread.
  scoped_refptr<base::SequencedTaskRunner> MediaThreadTaskRunner() override {
    return task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// Returns true if the module code cache was generated and written successfully.
bool GenerateModuleCodeCache(v8::Isolate* isolate,
                             const base::FilePath& in_module_path,
                             const base::FilePath& out_code_cache_path) {
  v8::TryCatch trycatch(isolate);

  std::string module_file_string;
  if (!base::ReadFileToString(in_module_path, &module_file_string)) {
    LOG(ERROR) << "Error reading file " << in_module_path;
    return false;
  }

  const blink::WebBundledCodeCacheGenerator::SerializedCodeCacheData
      serialized_code_cache_data = blink::WebBundledCodeCacheGenerator::
          CreateSerializedCodeCacheForModule(
              isolate, blink::WebString::FromUtf8(module_file_string));

  if (!base::WriteFile(out_code_cache_path, serialized_code_cache_data)) {
    LOG(ERROR) << "Failed to write cached metadata to file.";
    return false;
  }

  return true;
}

[[noreturn]] void RunSnapshotGenerator() {
  // Initialize an empty feature list for gin startup.
  auto early_access_feature_list = std::make_unique<base::FeatureList>();
  // This should be called after CommandLine::Init().
  base::FeatureList::SetInstance(std::move(early_access_feature_list));
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  static constexpr std::string_view kSnapshotBlobFlag("snapshot_blob");
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSnapshotBlobFlag)) {
    base::File file(base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
                        kSnapshotBlobFlag),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(file.IsValid());
    gin::V8Initializer::LoadV8SnapshotFromFile(
        std::move(file), nullptr, gin::V8SnapshotFileType::kDefault);
  } else {
    gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);
  }
#endif

  // Set up environment to make Blink and V8 workable.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("TakeSnapshot");
  mojo::core::Init();

  // Set predictable flag in V8 to generate identical snapshot file.
  static constexpr char kPredictableFlag[] = "--predictable";
  v8::V8::SetFlagsFromString(kPredictableFlag, sizeof(kPredictableFlag) - 1);

  // Take a snapshot.
  SnapshotPlatform platform(main_thread_task_executor.task_runner());
  mojo::BinderMap binders;
  blink::CreateMainThreadAndInitialize(&platform, &binders);
  auto* isolate = blink::CreateMainThreadIsolate();
  v8::StartupData blob = blink::WebV8ContextSnapshot::TakeSnapshot(isolate);

  // Save the snapshot as a file. Filename is given in a command line option.
  base::FilePath file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("output_file");
  CHECK(!file_path.empty());
  int error_code = 0;
  if (!base::WriteFile(file_path,
                       base::as_bytes(base::span(
                           blob.data, static_cast<size_t>(blob.raw_size))))) {
    fprintf(stderr, "Error: WriteFile of %d snapshot has failed.\n",
            blob.raw_size);
    error_code = 1;
  }

  delete[] blob.data;

  // v8::SnapshotCreator used in WebV8ContextSnapshot makes it complex how to
  // manage lifetime of v8::Isolate, gin::IsolateHolder, and
  // blink::V8PerIsolateData. Now we complete all works at this point, and can
  // exit without releasing all those instances correctly.
  _exit(error_code);
}

[[noreturn]] void RunCodeCacheGenerator() {
  // Setup the Blink environment.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("CompileBytecode");
  mojo::core::Init();

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  // When cross-compiling, this binary is built for the build host and the
  // build passes the target's snapshot explicitly. The code cache embeds the
  // snapshot's read-only checksum, so it must match the target's snapshot.
  static constexpr char kContextSnapshotSwitch[] = "context_snapshot";
  static constexpr char kSnapshotBlobSwitch[] = "snapshot_blob";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kContextSnapshotSwitch)) {
    gin::V8Initializer::LoadV8SnapshotFromFile(
        base::File(command_line.GetSwitchValuePath(kContextSnapshotSwitch),
                   base::File::FLAG_OPEN | base::File::FLAG_READ),
        nullptr, gin::V8SnapshotFileType::kWithAdditionalContext);
  } else if (command_line.HasSwitch(kSnapshotBlobSwitch)) {
    gin::V8Initializer::LoadV8SnapshotFromFile(
        base::File(command_line.GetSwitchValuePath(kSnapshotBlobSwitch),
                   base::File::FLAG_OPEN | base::File::FLAG_READ),
        nullptr, gin::V8SnapshotFileType::kDefault);
  }
  // No-op if a snapshot was already loaded above.
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  gin::V8Initializer::LoadV8Snapshot(
      gin::V8SnapshotFileType::kWithAdditionalContext);
#else
  gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  // Set predictable flag in V8 to generate consistent bytecode.
  static constexpr char kPredictableFlag[] = "--predictable";
  v8::V8::SetFlagsFromString(kPredictableFlag, sizeof(kPredictableFlag) - 1);

  CodeCacheGenPlatform platform(main_thread_task_executor.task_runner());
  mojo::BinderMap binders;
  blink::CreateMainThreadAndInitialize(&platform, &binders);
  auto* isolate = blink::CreateMainThreadIsolate();

  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  const base::FilePath in_folder = cmd.GetSwitchValuePath("in_folder");
  CHECK(!in_folder.empty());
  const std::vector<std::string> in_files =
      base::SplitString(cmd.GetSwitchValueASCII("in_files"), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CHECK(!in_files.empty());
  const base::FilePath out_folder = cmd.GetSwitchValuePath("out_folder");
  CHECK(!out_folder.empty());
  const std::string out_file_suffix =
      cmd.GetSwitchValueASCII("out_file_suffix");

  for (const std::string_view in_file : in_files) {
    if (!GenerateModuleCodeCache(
            isolate, in_folder.AppendASCII(in_file),
            out_folder.AppendASCII(base::StrCat({in_file, out_file_suffix})))) {
      _exit(1);
    }
  }
  _exit(0);
}

}  // namespace

// This program either takes a snapshot of V8 contexts (default mode) or
// compiles WebUI JS modules into V8 code caches (--generate_code_cache).
// Both modes are combined into a single executable to avoid linking Blink and
// V8 twice during the build.
//
// Snapshot usage:
// % v8_context_snapshot_generator
//     --snapshot_blob=<filename>
//     --output_file=<filename>
//
// Code cache usage:
// % v8_context_snapshot_generator --generate_code_cache
//     --in_folder=<dir> --in_files=<f1,f2,...>
//     --out_folder=<dir> --out_file_suffix=<suffix>
//     [--context_snapshot=<filename> | --snapshot_blob=<filename>]
int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  const bool kRemoveRecognizedFlags = true;
  v8::V8::SetFlagsFromCommandLine(&argc, argv, kRemoveRecognizedFlags);
  base::CommandLine::Init(argc, argv);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "generate_code_cache")) {
    RunCodeCacheGenerator();
  }
  RunSnapshotGenerator();
}
