// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AX_TREE_SERVER_H_
#define AX_TREE_SERVER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace ui {
struct AXTreeSelector;
class AXInspectScenario;
}  // namespace ui

namespace content {

class AXTreeServer final {
 public:
  AXTreeServer(const ui::AXTreeSelector& selector,
               const ui::AXInspectScenario& scenario);

  AXTreeServer(const AXTreeServer&) = delete;
  AXTreeServer& operator=(const AXTreeServer&) = delete;

 private:
#if defined(OS_WIN)
  // Only one COM initializer per thread is permitted.
  base::win::ScopedCOMInitializer com_initializer_;
#endif
};

}  // namespace content

#endif  // AX_TREE_SERVER_H_
