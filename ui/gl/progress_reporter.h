// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_PROGRESS_REPORTER_H_
#define UI_GL_PROGRESS_REPORTER_H_

namespace gl {

// ProgressReporter is used by ContextGroup and GrGLInterface to report when it
// is making forward progress in execution, delaying activation of the watchdog
// timeout.
class ProgressReporter {
 public:
  virtual ~ProgressReporter() = default;

  virtual void ReportProgress() = 0;
};

class ScopedProgressReporter {
 public:
  ScopedProgressReporter(ProgressReporter* progress_reporter)
      : progress_reporter_(progress_reporter) {
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }
  ~ScopedProgressReporter() {
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }

 private:
  ProgressReporter* progress_reporter_;
};

}  // namespace gl

#endif  // UI_GL_PROGRESS_REPORTER_H_
