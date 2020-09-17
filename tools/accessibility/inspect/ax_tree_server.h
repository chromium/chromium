// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AX_TREE_SERVER_H_
#define AX_TREE_SERVER_H_

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/public/browser/accessibility_tree_formatter.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

class AXTreeServer final {
 public:
  AXTreeServer(gfx::AcceleratedWidget widget,
               const base::FilePath& filters_path,
               bool use_json);
  AXTreeServer(const AccessibilityTreeFormatter::TreeSelector& selector,
               const base::FilePath& filters_path,
               bool use_json);

 private:
  using BuildTree = base::OnceCallback<std::unique_ptr<base::DictionaryValue>(
      AccessibilityTreeFormatter*)>;

  // Builds and formats the accessible tree.
  void Run(BuildTree build_tree,
           const base::FilePath& filters_path,
           bool use_json);

  // Generates property filters.
  std::vector<AccessibilityTreeFormatter::PropertyFilter> GetPropertyFilters(
      const base::FilePath& filters_path);

  // Formats and dumps into console the tree.
  void Format(AccessibilityTreeFormatter& formatter,
              const base::DictionaryValue& dict,
              bool use_json);

#if defined(OS_WIN)
  // Only one COM initializer per thread is permitted.
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AXTreeServer);
};

}  // namespace content

#endif  // AX_TREE_SERVER_H_
