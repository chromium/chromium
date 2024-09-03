// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_ui_kit_element.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ui/accessibility/ax_common.h"
#import "ui/accessibility/ax_enum_util.h"
#import "ui/accessibility/ax_enums.mojom.h"
#import "ui/accessibility/ax_role_properties.h"
#import "ui/accessibility/platform/ax_platform_node_ios.h"
#import "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#import "ui/accessibility/platform/child_iterator_base.h"

@implementation AXPlatformNodeUIKitElement {
  // The AXPlatformNode corresponding to this wrapper instance.
  raw_ptr<ui::AXPlatformNodeIOS> _node;
  // An array of children of this object. Cached to avoid re-computing.
  NSMutableArray* _children;
  // Whether the children have changed and need to be updated.
  BOOL _needsToUpdateChildren;
  // Whether _children is currently being computed.
  BOOL _gettingChildren;
}

- (instancetype)initWithPlatformNode:(ui::AXPlatformNodeIOS*)platformNode {
  id container = platformNode->GetParent();
  // TODO(crbug.com/336611337): Sometimes container is null for new subframes.
  // We need a way to retry after the AXTreeManager is connected to its parent.
  if (!container) {
    return nil;
  }
  if ((self =
           [super initWithAccessibilityContainer:platformNode->GetParent()])) {
    _node = platformNode;
    _needsToUpdateChildren = YES;
    _gettingChildren = NO;
  }
  return self;
}

- (void)childrenChanged {
  if (!_node || _gettingChildren) {
    return;
  }
  _needsToUpdateChildren = YES;
  if (![self isIncludedInPlatformTree]) {
    ui::AXPlatformNode* parentNode =
        ui::AXPlatformNode::FromNativeViewAccessible(_node->GetParent());
    if (parentNode) {
      [parentNode->GetNativeViewAccessible() childrenChanged];
    }
  }
}

#pragma mark - AXPlatformNodeUIKit

- (void)detach {
  _node = nullptr;
}

- (ui::AXPlatformNodeIOS*)node {
  return _node.get();
}

#pragma mark - UIAccessibilityContainer

- (NSArray*)accessibilityElements {
  if (!_node) {
    return nil;
  }
  if (_needsToUpdateChildren) {
    base::AutoReset<BOOL> set_getting_children(&_gettingChildren, YES);
    uint32_t childCount = _node->GetChildCount();
    _children = [[NSMutableArray alloc] initWithCapacity:childCount];
    for (auto it = _node->GetDelegate()->ChildrenBegin();
         *it != *_node->GetDelegate()->ChildrenEnd(); ++(*it)) {
      AXPlatformNodeUIKitElement* child = it->GetNativeViewAccessible();
      if ([child isIncludedInPlatformTree]) {
        [_children addObject:child];
      } else {
        [_children addObjectsFromArray:[child accessibilityElements]];
      }
    }

    // Also, add indirect children (if any).
    const std::vector<int32_t>& indirectChildIds = _node->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int32_t child_id = indirectChildIds[i];
      ui::AXPlatformNode* child = _node->GetDelegate()->GetFromNodeID(child_id);

      if (child) {
        [_children addObject:child->GetNativeViewAccessible()];
      }
    }
    _needsToUpdateChildren = NO;
  }
  return _children;
}

#pragma mark - UIAccessibility

- (UIAccessibilityTraits)accessibilityTraits {
  // TODO(crbug.com/336611337): Choose appropriate traits based on node's role.
  return UIAccessibilityTraitLink;
}

- (CGRect)accessibilityFrame {
  if (!_node) {
    return CGRectZero;
  }

  gfx::Rect rect = _node->GetDelegate()->GetBoundsRect(
      ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kClipped);
  rect = ScaleToRoundedRect(
      rect, 1.f / _node->GetIOSDelegate()->GetDeviceScaleFactor());

  return rect.ToCGRect();
}

- (id)accessibilityFocusedUIElement {
  if (!_node) {
    return nil;
  }

  return _node->GetFocus();
}

- (BOOL)isAccessibilityElement {
  if (!_node) {
    return NO;
  }

  if (_node->GetRole() == ax::mojom::Role::kImage &&
      _node->GetNameFrom() == ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
    return NO;
  }

  // TODO(crbug.com/336611337): If a node is an accessibility element, then
  // VoiceOver will not visit its accessibilityElements (that is, its children).
  // If we ever need both a node and its descendants to be interactable using
  // VoiceOver, we will need to restructure the UIAccessibility tree by
  // inserting an additional node to act as a container.
  if ([self accessibilityElements].count) {
    return NO;
  }

  return (_node->GetRole() != ax::mojom::Role::kUnknown &&
          !_node->GetDelegate()->IsIgnored());
}

- (NSString*)accessibilityLabel {
  if (!_node) {
    return nil;
  }

  std::string name = _node->GetName();

  if (!name.empty()) {
    return base::SysUTF8ToNSString(name);
  }

  // Given an image where there's no other title, return the base part
  // of the filename as the description.
  if ([self isImage]) {
    std::string url;
    if (_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl, &url)) {
      // Given a url like http://foo.com/bar/baz.png, just return the
      // base name, e.g., "baz.png".
      size_t leftIndex = url.rfind('/');
      std::string basename =
          leftIndex != std::string::npos ? url.substr(leftIndex) : url;
      return base::SysUTF8ToNSString(basename);
    }
  }

  return @"";
}

#pragma mark - UIAccessibilityAction

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  // TODO(crbug.com/336611337): Wire up custom actions for each node, like
  // being able to tap or otherwise interact.
  return nil;
}

#pragma mark - Private

- (BOOL)isIncludedInPlatformTree {
  return _node && _node->GetRole() != ax::mojom::Role::kUnknown &&
         !_node->IsInvisibleOrIgnored();
}

- (BOOL)isImage {
  return ui::IsImage(_node->GetRole()) &&
         !_node->GetBoolAttribute(
             ax::mojom::BoolAttribute::kCanvasHasFallback) &&
         !_node->GetChildCount() &&
         _node->GetNameFrom() != ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
}

@end
