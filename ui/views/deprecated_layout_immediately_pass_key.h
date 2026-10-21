// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEPRECATED_LAYOUT_IMMEDIATELY_PASS_KEY_H_
#define UI_VIEWS_DEPRECATED_LAYOUT_IMMEDIATELY_PASS_KEY_H_

namespace views {

class MenuItemView;
class ScrollView;
class TableView;
class TreeView;

// Grants access to View::DeprecatedLayoutImmediately. See crbug.com/1521108.
//
// Synchronous layout is deprecated; prefer View::InvalidateLayout (see
// docs/ui/learn/bestpractices/layout.md#don_t-invoke-layout_directly). Tests
// should use InvalidateLayout + views::test::RunScheduledLayout instead.
class DeprecatedLayoutImmediatelyPassKey {
 private:
  // DO NOT ADD TO THIS LIST without ui/views/OWNERS approval!
  // Callers of the no-arg View::DeprecatedLayoutImmediately are being migrated
  // here only when synchronous layout is truly required.
  friend class MenuItemView;
  friend class ScrollView;
  friend class TableView;
  friend class TreeView;

  DeprecatedLayoutImmediatelyPassKey() = default;
};

}  // namespace views

#endif  // UI_VIEWS_DEPRECATED_LAYOUT_IMMEDIATELY_PASS_KEY_H_
