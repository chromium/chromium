// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_window_obj_wrapper.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#endif

namespace views {
namespace {

Widget* GetWidgetForWindow(aura::Window* window) {
  Widget* widget = Widget::GetWidgetForNativeView(window);
  if (!widget)
    return nullptr;

  // Under mus/mash both the WindowTreeHost's root aura::Window and the content
  // aura::Window will return the same Widget for GetWidgetForNativeView(). Only
  // return the Widget for the content window, not the root, since otherwise
  // we'll end up with two children in the AX node tree that have the same
  // parent.
  if (widget->GetNativeWindow() != window) {
    DCHECK(window->IsRootWindow());
    return nullptr;
  }

  return widget;
}

// Fires |event| on the |window|, and the Widget and RootView associated with
// |window|.
void FireEventOnWindowChildWidgetAndRootView(aura::Window* window,
                                             ax::mojom::Event event,
                                             AXAuraObjCache* cache) {
  cache->FireEvent(cache->GetOrCreate(window), event);
  Widget* widget = GetWidgetForWindow(window);
  if (widget) {
    cache->FireEvent(cache->GetOrCreate(widget), event);

    views::View* root_view = widget->GetRootView();
    if (root_view)
      root_view->NotifyAccessibilityEvent(event, true);
  }
}

// Fires location change events on a window, taking into account its
// associated widget, that widget's root view, and descendant windows.
void FireLocationChangesRecursively(aura::Window* window,
                                    AXAuraObjCache* cache) {
  FireEventOnWindowChildWidgetAndRootView(
      window, ax::mojom::Event::kLocationChanged, cache);

  for (aura::Window* child : window->children()) {
    FireLocationChangesRecursively(child, cache);
  }
}

std::string GetWindowName(aura::Window* window) {
  std::string class_name = window->GetName();
  if (class_name.empty())
    class_name = "aura::Window";
  return class_name;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::string GetPlatformWindowId(aura::Window* window) {
  // Ignore non-top level windows.
  if (!window->IsRootWindow() || window->parent())
    return std::string();

  // On desktop aura there is one WindowTreeHost per top-level window.
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  if (!window_tree_host)
    return std::string();

  // Prefer the DesktopWindowTreeHostPlatform if it exists.
  DesktopWindowTreeHostPlatform* desktop_window_tree_host_platform =
      DesktopWindowTreeHostPlatform::GetHostForWidget(
          window_tree_host->GetAcceleratedWidget());
  if (!desktop_window_tree_host_platform)
    return window_tree_host->GetUniqueId();

  while (desktop_window_tree_host_platform->window_parent()) {
    desktop_window_tree_host_platform =
        desktop_window_tree_host_platform->window_parent();
  }

  return desktop_window_tree_host_platform->GetUniqueId();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

AXWindowObjWrapper::AXWindowObjWrapper(AXAuraObjCache* aura_obj_cache,
                                       aura::Window* window)
    : AXAuraObjWrapper(aura_obj_cache),
      window_(window),
      is_root_window_(window->IsRootWindow()) {
  observation_.Observe(window);

  if (is_root_window_)
    aura_obj_cache_->OnRootWindowObjCreated(window);

  // This is a top level root window.
  if (window->IsRootWindow() && !window->parent()) {
    // On desktop aura there is one WindowTreeHost per top-level window.
    aura::WindowTreeHost* window_tree_host = window->GetHost();
    if (window_tree_host)
      ime_observation_.Observe(window_tree_host->GetInputMethod());
  }
}

AXWindowObjWrapper::~AXWindowObjWrapper() = default;

bool AXWindowObjWrapper::HandleAccessibleAction(
    const ui::AXActionData& action) {
  if (action.action == ax::mojom::Action::kFocus) {
    window_->Focus();
    return true;
  }
  return false;
}

AXAuraObjWrapper* AXWindowObjWrapper::GetParent() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const std::string& window_id = GetPlatformWindowId(window_);

  // In Lacros, the presence of a platform window id means it is parented to the
  // Ash tree via the app id.
  if (!window_id.empty())
    return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  aura::Window* parent = window_->parent();
  if (!parent)
    return nullptr;

  if (parent->GetProperty(ui::kChildAXTreeID) &&
      ui::AXTreeID::FromString(*(parent->GetProperty(ui::kChildAXTreeID))) !=
          ui::AXTreeIDUnknown()) {
    return nullptr;
  }

  return aura_obj_cache_->GetOrCreate(parent);
}

void AXWindowObjWrapper::GetChildren(
    std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>* out_children) {
  // Ignore this window's descendants if it has a child tree.
  if (window_->GetProperty(ui::kChildAXTreeID) &&
      ui::AXTreeID::FromString(*(window_->GetProperty(ui::kChildAXTreeID))) !=
          ui::AXTreeIDUnknown()) {
    return;
  }

  // Ignore this window's children if it is forced to be invisible.
  if (window_->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren))
    return;

  for (aura::Window* child : window_->children()) {
    out_children->push_back(aura_obj_cache_->GetOrCreate(child));
  }

  // Also consider any associated widgets as children.
  Widget* widget = GetWidgetForWindow(window_);
  if (widget && widget->IsVisible())
    out_children->push_back(aura_obj_cache_->GetOrCreate(widget));
}

void AXWindowObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This app id connects this node with a node in the Ash tree (an
  // components/exo shell surface).
  const std::string& window_id = GetPlatformWindowId(window_);
  if (!window_id.empty())
    out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                      window_id);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (window_->IsRootWindow() && !window_->parent() && window_->GetHost()) {
    ui::TextInputClient* client =
        window_->GetHost()->GetInputMethod()->GetTextInputClient();
    // Only set caret bounds if input caret is in an editable node.
    if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      gfx::Rect caret_bounds_in_screen = client->GetCaretBounds();
      out_node_data->AddIntListAttribute(
          ax::mojom::IntListAttribute::kCaretBounds,
          {caret_bounds_in_screen.x(), caret_bounds_in_screen.y(),
           caret_bounds_in_screen.width(), caret_bounds_in_screen.height()});
    }
  }

  out_node_data->id = GetUniqueId();
  ax::mojom::Role role = window_->GetProperty(ui::kAXRoleOverride);
  if (role != ax::mojom::Role::kNone)
    out_node_data->role = role;
  else
    out_node_data->role = ax::mojom::Role::kWindow;
  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                    base::UTF16ToUTF8(window_->GetTitle()));
  if (!window_->IsVisible() ||
      window_->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren)) {
    out_node_data->AddState(ax::mojom::State::kInvisible);
  }

  out_node_data->relative_bounds.bounds =
      gfx::RectF(window_->GetBoundsInScreen());

  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                    GetWindowName(window_));

  std::string* child_ax_tree_id_ptr = window_->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id_ptr) {
    ui::AXTreeID child_ax_tree_id =
        ui::AXTreeID::FromString(*child_ax_tree_id_ptr);
    if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
      // Most often, child AX trees are parented to Views. We need to handle
      // the case where they're not here, but we don't want the same AX tree
      // to be a child of two different parents.
      //
      // To avoid this double-parenting, only add the child tree ID of this
      // window if the top-level window doesn't have an associated Widget.
      //
      // Also, if this window is not visible, its child tree should also be
      // non-visible so prune it.
      if (!window_->GetToplevelWindow() ||
          GetWidgetForWindow(window_->GetToplevelWindow()) ||
          !window_->IsVisible() ||
          window_->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren)) {
        return;
      }

      out_node_data->AddChildTreeId(child_ax_tree_id);

      const float scale_factor =
          window_->GetToplevelWindow()->layer()->device_scale_factor();
      out_node_data->AddFloatAttribute(
          ax::mojom::FloatAttribute::kChildTreeScale, scale_factor);
    }
  }
}

ui::AXNodeID AXWindowObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
}

std::string AXWindowObjWrapper::ToString() const {
  return GetWindowName(window_);
}

void AXWindowObjWrapper::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  FireEvent(ax::mojom::Event::kTreeChanged);
}

void AXWindowObjWrapper::OnWindowDestroyed(aura::Window* window) {
  if (is_root_window_)
    aura_obj_cache_->OnRootWindowObjDestroyed(window_);

  aura_obj_cache_->Remove(window, nullptr);
}

void AXWindowObjWrapper::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  window_destroying_ = true;
}

void AXWindowObjWrapper::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.phase == WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED)
    aura_obj_cache_->Remove(params.target, params.old_parent);
}

void AXWindowObjWrapper::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (window_destroying_)
    return;

  if (window == window_)
    FireLocationChangesRecursively(window_, aura_obj_cache_);
}

void AXWindowObjWrapper::OnWindowPropertyChanged(aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (window_destroying_ || window != window_)
    return;

  if (key == ui::kChildAXTreeID ||
      key == ui::kAXConsiderInvisibleAndIgnoreChildren) {
    FireEvent(ax::mojom::Event::kChildrenChanged);
  }
}

void AXWindowObjWrapper::OnWindowVisibilityChanged(aura::Window* window,
                                                   bool visible) {
  if (window_destroying_)
    return;

  FireEvent(ax::mojom::Event::kStateChanged);
}

void AXWindowObjWrapper::OnWindowTransformed(aura::Window* window,
                                             ui::PropertyChangeReason reason) {
  if (window_destroying_)
    return;

  if (window == window_)
    FireLocationChangesRecursively(window_, aura_obj_cache_);
}

void AXWindowObjWrapper::OnWindowTitleChanged(aura::Window* window) {
  if (window_destroying_)
    return;

  FireEventOnWindowChildWidgetAndRootView(
      window_, ax::mojom::Event::kTreeChanged, aura_obj_cache_);
}

void AXWindowObjWrapper::FireEvent(ax::mojom::Event event_type) {
  aura_obj_cache_->FireEvent(aura_obj_cache_->GetOrCreate(window_), event_type);
}

}  // namespace views
