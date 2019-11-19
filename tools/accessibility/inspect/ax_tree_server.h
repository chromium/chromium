// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AX_TREE_SERVER_H_
#define AX_TREE_SERVER_H_

#include <string>

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/public/browser/accessibility_tree_formatter.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

class AXTreeServer final {
 public:
  AXTreeServer(base::ProcessId pid,
               const base::FilePath& filters_path,
               bool use_json);
  AXTreeServer(gfx::AcceleratedWidget widget,
               const base::FilePath& filters_path,
               bool use_json);
  AXTreeServer(const base::StringPiece& pattern,
               const base::FilePath& filters_path,
               bool use_json);

 private:
  void Format(AccessibilityTreeFormatter& formatter,
              const base::DictionaryValue& dict,
              const base::FilePath& filters_path,
              bool use_json);

#if defined(OS_WIN)
  // Only one COM initializer per thread is permitted.
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AXTreeServer);
};

}  // namespace content

#endif  // AX_TREE_SERVER_H_
