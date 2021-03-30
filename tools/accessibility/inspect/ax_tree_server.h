// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AX_TREE_SERVER_H_
#define AX_TREE_SERVER_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

class AXTreeServer final {
 public:
  AXTreeServer(gfx::AcceleratedWidget widget,
               const base::FilePath& filters_path);
  AXTreeServer(const ui::AXTreeSelector& selector,
               const base::FilePath& filters_path);

 private:
  using BuildTree = base::OnceCallback<base::Value(const ui::AXTreeFormatter*)>;

  // Builds and formats the accessible tree.
  void Run(BuildTree build_tree, const base::FilePath& filters_path);

  // Generates property filters.
  std::vector<ui::AXPropertyFilter> GetPropertyFilters(
      const base::FilePath& filters_path);

#if defined(OS_WIN)
  // Only one COM initializer per thread is permitted.
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AXTreeServer);
};

}  // namespace content

#endif  // AX_TREE_SERVER_H_
