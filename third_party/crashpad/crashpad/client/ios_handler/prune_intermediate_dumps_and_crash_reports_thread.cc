// Copyright 2021 The Crashpad Authors
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

#include "client/ios_handler/prune_intermediate_dumps_and_crash_reports_thread.h"

#include <utility>

#include "client/prune_crash_reports.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"
#include "util/ios/scoped_background_task.h"

namespace crashpad {

namespace {

// The file extension used to indicate a file is locked.
constexpr char kLockedExtension[] = ".locked";

// Prune onces a day.
constexpr time_t prune_interval = 60 * 60 * 24;

// If the client finds a locked file matching it's own bundle id, unlock it
// after 24 hours.
constexpr time_t matching_bundle_locked_ttl = 60 * 60 * 24;

// Unlock any locked intermediate dump after 60 days.
constexpr time_t max_locked_ttl = 60 * 60 * 24 * 60;

// The initial thread delay for applications.  Delay the thread's file i/o to
// not interfere with application startup.
constexpr double app_delay = 60;

// The initial thread delay for app extensions. Because iOS extensions are often
// very short lived, do not wait the full |app_delay|, and instead use a shorter
// time.
constexpr double extension_delay = 5;


//! \brief Unlocks old intermediate dumps.
//!
//! This function can unlock (remove the .locked extension) intermediate dumps
//! that are either too old to be useful, or are likely leftover dumps from
//! clean app exits.
//!
//! \param[in] pending_path The path to any locked intermediate dump files.
//! \param[in] bundle_identifier_and_seperator The identifier for this client,
//!     used to determine when locked files are considered stale.
void UnlockOldIntermediateDumps(base::FilePath pending_path,
                                std::string bundle_identifier_and_seperator) {
  DirectoryReader reader;
  std::vector<base::FilePath> files;
  if (!reader.Open(pending_path)) {
    return;
  }
  base::FilePath file;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&file)) ==
         DirectoryReader::Result::kSuccess) {
    if (file.FinalExtension() != kLockedExtension)
      continue;

    const base::FilePath file_path(pending_path.Append(file));
    timespec file_time;
    time_t now = time(nullptr);
    if (!FileModificationTime(file_path, &file_time)) {
      continue;
    }

    if ((file.value().compare(0,
                              bundle_identifier_and_seperator.size(),
                              bundle_identifier_and_seperator) == 0 &&
         file_time.tv_sec <= now - matching_bundle_locked_ttl) ||
        (file_time.tv_sec <= now - max_locked_ttl)) {
      base::FilePath new_path = file_path.RemoveFinalExtension();
      MoveFileOrDirectory(file_path, new_path);
      continue;
    }
  }
}

}  // namespace

PruneIntermediateDumpsAndCrashReportsThread::
    PruneIntermediateDumpsAndCrashReportsThread(
        CrashReportDatabase* database,
        std::unique_ptr<PruneCondition> condition,
        base::FilePath pending_path,
        std::string bundle_identifier_and_seperator,
        bool is_extension)
    : thread_(prune_interval, this),
      condition_(std::move(condition)),
      pending_path_(pending_path),
      bundle_identifier_and_seperator_(bundle_identifier_and_seperator),
      initial_work_delay_(is_extension ? extension_delay : app_delay),
      last_start_time_(0),
      database_(database) {}

PruneIntermediateDumpsAndCrashReportsThread::
    ~PruneIntermediateDumpsAndCrashReportsThread() {}

void PruneIntermediateDumpsAndCrashReportsThread::Start() {
  thread_.Start(initial_work_delay_);
}

void PruneIntermediateDumpsAndCrashReportsThread::Stop() {
  thread_.Stop();
}

void PruneIntermediateDumpsAndCrashReportsThread::DoWork(
    const WorkerThread* thread) {
  // This thread may be stopped and started a number of times throughout the
  // lifetime of the process to prevent 0xdead10cc kills (see
  // crbug.com/crashpad/400), but it should only run once per prune_interval
  // after initial_work_delay_.
  time_t now = time(nullptr);
  if (now - last_start_time_ < prune_interval)
    return;
  last_start_time_ = now;

  internal::ScopedBackgroundTask scoper("PruneThread");
  database_->CleanDatabase(60 * 60 * 24 * 3);

  // Here and below, respect Stop() being called after each task.
  if (!thread_.is_running())
    return;
  PruneCrashReportDatabase(database_, condition_.get());

  if (!thread_.is_running())
    return;
  if (!clean_old_intermediate_dumps_) {
    clean_old_intermediate_dumps_ = true;
    UnlockOldIntermediateDumps(pending_path_, bundle_identifier_and_seperator_);
  }
}

}  // namespace crashpad
