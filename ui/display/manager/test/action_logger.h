// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_H_
#define UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_H_

#include <string>

namespace display::test {

class ActionLogger {
 public:
  ActionLogger();

  ActionLogger(const ActionLogger&) = delete;
  ActionLogger& operator=(const ActionLogger&) = delete;

  ~ActionLogger();

  void AppendAction(const std::string& action);

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActionsAndClear() (i.e.
  // results are non-repeatable).
  std::string GetActionsAndClear();

 private:
  std::string actions_;
};

}  // namespace display::test

#endif  // UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_H_
