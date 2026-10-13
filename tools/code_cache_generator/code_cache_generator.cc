// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
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
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8.h"

namespace {

class CodeCacheGenPlatform final : public blink::Platform {
 public:
  explicit CodeCacheGenPlatform(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // blink::Platform:
  bool DisallowV8FeatureFlagOverrides() const override { return true; }

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

// Returns true if the module code cache was generated and written successfully.
bool GenerateModuleCodeCache(v8::Isolate* isolate,
                             const base::FilePath& in_module_path,
                             const base::FilePath& out_code_cache_path) {
  v8::TryCatch trycatch(isolate);

  // Attempt to read the module file from disk.
  std::string module_file_string;
  if (!base::ReadFileToString(in_module_path, &module_file_string)) {
    LOG(ERROR) << "Error reading file " << in_module_path;
    return false;
  }

  const blink::WebBundledCodeCacheGenerator::SerializedCodeCacheData
      serialized_code_cache_data = blink::WebBundledCodeCacheGenerator::
          CreateSerializedCodeCacheForModule(
              isolate, blink::WebString::FromUtf8(module_file_string));

  // Attempt to write the cached metadata.
  if (!base::WriteFile(out_code_cache_path, serialized_code_cache_data)) {
    LOG(ERROR) << "Failed to write cached metadata to file.";
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  const bool kRemoveRecognizedFlags = true;
  v8::V8::SetFlagsFromCommandLine(&argc, argv, kRemoveRecognizedFlags);
  base::CommandLine::Init(argc, argv);

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

  const base::FilePath in_folder =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("in_folder");
  CHECK(!in_folder.empty());
  const std::vector<std::string> in_files = base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("in_files"),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CHECK(!in_files.empty());
  const base::FilePath out_folder =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("out_folder");
  CHECK(!out_folder.empty());
  const std::string out_file_suffix =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "out_file_suffix");

  for (const std::string_view in_file : in_files) {
    if (!GenerateModuleCodeCache(
            isolate, in_folder.AppendASCII(in_file),
            out_folder.AppendASCII(base::StrCat({in_file, out_file_suffix})))) {
      _exit(1);
    }
  }
  _exit(0);
}
