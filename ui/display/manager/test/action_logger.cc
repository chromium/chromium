// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/action_logger.h"

namespace display::test {

ActionLogger::ActionLogger() {}

ActionLogger::~ActionLogger() {}

void ActionLogger::AppendAction(const std::string& action) {
  if (!actions_.empty())
    actions_ += ",";
  actions_ += action;
}

std::string ActionLogger::GetActionsAndClear() {
  std::string actions = actions_;
  actions_.clear();
  return actions;
}

}  // namespace display::test
