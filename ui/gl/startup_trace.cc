// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/startup_trace.h"

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace gl {
namespace {
constexpr char kTraceCategory[] = "gpu,startup";
}  // namespace

std::atomic<bool> StartupTrace::startup_in_progress_ = false;

// static
StartupTrace* StartupTrace::GetInstance() {
  static base::NoDestructor<StartupTrace> g_instance;
  return g_instance.get();
}

// static
void StartupTrace::Startup() {
  startup_in_progress_.store(true);
}

// static
void StartupTrace::StarupDone() {
  startup_in_progress_.store(false);
  gl::StartupTrace::GetInstance()->RecordAndClearStages();
}

StartupTrace::StartupTrace() {
  BindToCurrentThread();
}

StartupTrace::~StartupTrace() = default;

void StartupTrace::BindToCurrentThread() {
  if (!task_runner_ && base::SequencedTaskRunner::HasCurrentDefault()) {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
}

StartupTrace::ScopedStage::ScopedStage(size_t size) : size(size) {}
StartupTrace::ScopedStage::~ScopedStage() {
  if (size) {
    StartupTrace::GetInstance()->stages_[size - 1].end = base::TimeTicks::Now();
  }
}

StartupTrace::ScopedStage StartupTrace::AddStage(const char* name) {
  DCHECK(!base::SequencedTaskRunner::HasCurrentDefault() ||
         task_runner_->RunsTasksInCurrentSequence());
  stages_.emplace_back(name, base::TimeTicks::Now(), base::TimeTicks());
  return StartupTrace::ScopedStage{stages_.size()};
}

void StartupTrace::RecordAndClearStages() {
  auto t = perfetto::ThreadTrack::Current();

  for (auto& stage : stages_) {
    TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString{stage.name}, t,
                      stage.start);
    TRACE_EVENT_END(kTraceCategory, t, stage.end);
  }

  stages_.clear();
}

}  // namespace gl
