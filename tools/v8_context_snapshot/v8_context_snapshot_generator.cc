// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "gin/v8_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_v8_context_snapshot.h"
#include "v8/include/v8.h"

namespace {

constexpr char kPredictableFlag[] = "--predictable";

class SnapshotPlatform final : public blink::Platform {
 public:
  bool IsTakingV8ContextSnapshot() override { return true; }
};

}  // namespace

// This program takes a snapshot of V8 contexts and writes it out as a file.
// The snapshot file is consumed by Blink.
//
// Usage:
// % v8_context_snapshot_generator --output_file=<filename>
int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  const bool kRemoveRecognizedFlags = true;
  v8::V8::SetFlagsFromCommandLine(&argc, argv, kRemoveRecognizedFlags);
  base::CommandLine::Init(argc, argv);

  // Initialize an empty feature list for gin startup.
  auto early_access_feature_list = std::make_unique<base::FeatureList>();
  // This should be called after CommandLine::Init().
  base::FeatureList::SetInstance(std::move(early_access_feature_list));
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

  // Set up environment to make Blink and V8 workable.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("TakeSnapshot");
  mojo::core::Init();

  // Set predictable flag in V8 to generate identical snapshot file.
  v8::V8::SetFlagsFromString(kPredictableFlag, sizeof(kPredictableFlag) - 1);

  // Take a snapshot.
  SnapshotPlatform platform;
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
                       base::as_bytes(base::make_span(
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
