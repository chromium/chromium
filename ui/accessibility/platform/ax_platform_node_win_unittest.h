// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_UNITTEST_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_UNITTEST_H_

#include "ui/accessibility/platform/ax_platform_node_unittest.h"

#include <memory>
#include <unordered_set>

#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/base/win/accessibility_misc_utils.h"

struct IAccessible;
struct IAccessible2;
struct IAccessible2_2;
struct IAccessibleTableCell;
struct IRawElementProviderFragment;
struct IRawElementProviderFragmentRoot;
struct IRawElementProviderSimple;
struct IUnknown;

namespace base {
namespace win {
class ScopedVariant;
}
}  // namespace base

namespace ui {

class AXFragmentRootWin;
class AXPlatformNode;

class TestFragmentRootDelegate : public AXFragmentRootDelegateWin {
 public:
  TestFragmentRootDelegate();
  virtual ~TestFragmentRootDelegate();
  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override;
  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override;
  bool IsAXFragmentRootAControlElement() override;
  gfx::NativeViewAccessible child_ = nullptr;
  gfx::NativeViewAccessible parent_ = nullptr;
  bool is_control_element_ = true;
};

class AXPlatformNodeWinTest : public ui::AXPlatformNodeTest {
 public:
  AXPlatformNodeWinTest();
  ~AXPlatformNodeWinTest() override;

  void SetUp() override;

  void TearDown() override;

 protected:
  AXPlatformNode* AXPlatformNodeFromNode(AXNode* node);
  template <typename T>
  Microsoft::WRL::ComPtr<T> QueryInterfaceFromNodeId(int32_t id);
  template <typename T>
  Microsoft::WRL::ComPtr<T> QueryInterfaceFromNode(AXNode* node);
  Microsoft::WRL::ComPtr<IRawElementProviderSimple>
  GetRootIRawElementProviderSimple();
  Microsoft::WRL::ComPtr<IRawElementProviderSimple>
  GetIRawElementProviderSimpleFromChildIndex(int child_index);
  Microsoft::WRL::ComPtr<IRawElementProviderFragment>
  GetRootIRawElementProviderFragment();
  Microsoft::WRL::ComPtr<IRawElementProviderFragment>
  IRawElementProviderFragmentFromNode(AXNode* node);
  Microsoft::WRL::ComPtr<IAccessible> IAccessibleFromNode(AXNode* node);
  Microsoft::WRL::ComPtr<IAccessible> GetRootIAccessible();
  Microsoft::WRL::ComPtr<IAccessible2> ToIAccessible2(
      Microsoft::WRL::ComPtr<IUnknown> unknown);
  Microsoft::WRL::ComPtr<IAccessible2> ToIAccessible2(
      Microsoft::WRL::ComPtr<IAccessible> accessible);
  Microsoft::WRL::ComPtr<IAccessible2_2> ToIAccessible2_2(
      Microsoft::WRL::ComPtr<IAccessible> accessible);
  void CheckVariantHasName(const base::win::ScopedVariant& variant,
                           const wchar_t* expected_name);
  void CheckIUnknownHasName(Microsoft::WRL::ComPtr<IUnknown> unknown,
                            const wchar_t* expected_name);
  Microsoft::WRL::ComPtr<IAccessibleTableCell> GetCellInTable();

  void InitFragmentRoot();
  AXFragmentRootWin* InitNodeAsFragmentRoot(AXNode* node,
                                            TestFragmentRootDelegate* delegate);
  Microsoft::WRL::ComPtr<IRawElementProviderFragmentRoot> GetFragmentRoot();

  using PatternSet = std::unordered_set<LONG>;
  PatternSet GetSupportedPatternsFromNodeId(int32_t id);

  std::unique_ptr<AXFragmentRootWin> ax_fragment_root_;

  std::unique_ptr<TestFragmentRootDelegate> test_fragment_root_delegate_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_WIN_UNITTEST_H_
