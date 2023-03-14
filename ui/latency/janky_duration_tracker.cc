// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/janky_duration_tracker.h"

#include <iomanip>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ui {
namespace {

// Singleton class to Advance() scroll jank duration counts. Watches changes in
// a directory, if specified by commandline flags. When a file is added or
// removed or replaced in the directory, prints the current jank sums to the
// log. Ignores the counts registered before the directory notification is set
// up.
class JankyDurationTracker {
 public:
  // Pointer to the singleton. Can be called on any thread.
  static JankyDurationTracker* Get();

  // Advance either the janky count xor the non-janky one. Can be called on any
  // thread.
  void Advance(bool janky, int count);

 private:
  friend class base::NoDestructor<JankyDurationTracker>;
  JankyDurationTracker();
  void InitializeOnThreadPool();
  void OnChanged(const base::FilePath& path, bool error);
  void ReportTotalsAndReset();

  std::atomic<uint64_t> janky_total_ = 0;
  std::atomic<uint64_t> non_janky_total_ = 0;
  std::atomic<bool> ready_ = false;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  base::FilePath file_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// static
JankyDurationTracker* JankyDurationTracker::Get() {
  static base::NoDestructor<JankyDurationTracker> tracker;
  return tracker.get();
}

JankyDurationTracker::JankyDurationTracker() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT});
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JankyDurationTracker::InitializeOnThreadPool,
                                base::Unretained(this)));
}

void JankyDurationTracker::Advance(bool janky, int count) {
  // Avoid counting jank when not requested by the flag. All of the object
  // members are initially written before posting the task for initialization,
  // which guarantees their consistent visibility from all posted tasks. A
  // relaxed load may ignore some early calls to this method without breaking
  // consistency.
  if (!ready_.load(std::memory_order_relaxed)) {
    return;
  }
  if (janky) {
    janky_total_.fetch_add(count, std::memory_order_relaxed);
  } else {
    non_janky_total_.fetch_add(count, std::memory_order_relaxed);
  }
}

void JankyDurationTracker::InitializeOnThreadPool() {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
  // Neither iOS nor Fuchsia use the base::FilePathWatcher. Avoid the dependency
  // on these platforms to save binary size.
  return;
#else
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  file_path_ =
      command_line->GetSwitchValuePath(switches::kWatchDirForScrollJankReport);
  if (file_path_.empty()) {
    return;
  }
  watcher_ = std::make_unique<base::FilePathWatcher>();
  if (!watcher_->Watch(file_path_, base::FilePathWatcher::Type::kNonRecursive,
                       base::BindRepeating(&JankyDurationTracker::OnChanged,
                                           base::Unretained(this)))) {
    LOG(ERROR) << "Cannot watch file '" << file_path_.value() << "'";
    watcher_.reset();
    return;
  }
  // See the comment in Advance() explaining why the relaxed memory ordering is
  // sufficient.
  ready_.store(true, std::memory_order_relaxed);
#endif
}

void JankyDurationTracker::OnChanged(const base::FilePath& path, bool error) {
  if (!error) {
    ReportTotalsAndReset();
  }
}

void JankyDurationTracker::ReportTotalsAndReset() {
  uint64_t janky_total = janky_total_.exchange(0, std::memory_order_relaxed);
  uint64_t non_janky_total =
      non_janky_total_.exchange(0, std::memory_order_relaxed);
  VLOG(0) << "JankyDurationTrackerCSV:" << janky_total << "," << non_janky_total
          << "," << std::setprecision(2)
          << ((double)janky_total) / (janky_total + non_janky_total);
}

}  // namespace

void AdvanceJankyDurationForBenchmarking(bool janky, int count) {
  JankyDurationTracker::Get()->Advance(janky, count);
}

}  // namespace ui
