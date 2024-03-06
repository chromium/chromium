// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class AXAuraObjWrapper;
class AXVirtualView;
class View;
class Widget;

// A cache responsible for assigning id's to a set of interesting Aura views.
class VIEWS_EXPORT AXAuraObjCache : public aura::client::FocusChangeObserver {
 public:
  AXAuraObjCache();
  AXAuraObjCache(const AXAuraObjCache&) = delete;
  AXAuraObjCache& operator=(const AXAuraObjCache&) = delete;
  ~AXAuraObjCache() override;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnChildWindowRemoved(AXAuraObjWrapper* parent) = 0;
    virtual void OnEvent(AXAuraObjWrapper* aura_obj,
                         ax::mojom::Event event_type) = 0;
  };

  // Get or create an entry in the cache. May return null if the View is not
  // associated with a Widget.
  AXAuraObjWrapper* GetOrCreate(View* view);
  AXAuraObjWrapper* GetOrCreate(AXVirtualView* virtual_view);

  // Get or create an entry in the cache.
  AXAuraObjWrapper* GetOrCreate(Widget* widget);
  AXAuraObjWrapper* GetOrCreate(aura::Window* window);

  // Creates an entry in this cache given a wrapper object. Use this method when
  // creating a wrapper not backed by any of the supported views above. This
  // cache will take ownership. Will replace an existing entry with the same id.
  void CreateOrReplace(std::unique_ptr<AXAuraObjWrapper> obj);

  // Gets an id given an Aura view.
  ui::AXNodeID GetID(View* view) const;
  ui::AXNodeID GetID(AXVirtualView* view) const;
  ui::AXNodeID GetID(Widget* widget) const;
  ui::AXNodeID GetID(aura::Window* window) const;

  // Removes an entry from this cache based on an Aura view.
  void Remove(View* view);
  void Remove(AXVirtualView* view);
  void Remove(Widget* widget);

  // Removes |window| and optionally notifies delegate by sending an event on
  // the |parent| if provided.
  void Remove(aura::Window* window, aura::Window* parent);

  // Removes a view and all of its descendants from the cache.
  void RemoveViewSubtree(View* view);

  // Lookup a cached entry based on an id.
  AXAuraObjWrapper* Get(ui::AXNodeID id);

  // Get all top level windows this cache knows about. Under classic ash and
  // SingleProcessMash this is a list of per-display root windows.
  void GetTopLevelWindows(
      std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>* children);

  // Get the object that has focus.
  AXAuraObjWrapper* GetFocus();

  // Tell our delegate to fire an event on a given object.
  void FireEvent(AXAuraObjWrapper* aura_obj, ax::mojom::Event event_type);

  // Notifies this cache of a change in root window.
  void OnRootWindowObjCreated(aura::Window* window);

  // Notifies this cache of a change in root window.
  void OnRootWindowObjDestroyed(aura::Window* window);

  // Sets a window to take a11y focus. This is for windows that need to work
  // with accessibility clients that consume accessibility APIs, but cannot take
  // real focus themselves. |a11y_override_window_| will be set to null when
  // destroyed, or can be set back to null using this function.
  // TODO(sammiequon): Merge this with set_focused_widget_for_testing().
  void SetA11yOverrideWindow(aura::Window* a11y_override_window);

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  // Changes the behavior of GetFocusedView() so that it only considers
  // views within the given Widget, this enables making tests
  // involving focus reliable.
  void set_focused_widget_for_testing(views::Widget* widget) {
    focused_widget_for_testing_ = widget;
  }

 private:
  friend class base::NoDestructor<AXAuraObjCache>;
  class A11yOverrideWindowObserver;

  View* GetFocusedView();

  // Send a notification that the focused view may have changed.
  void OnFocusedViewChanged();

  // aura::client::FocusChangeObserver override.
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  template <typename AuraViewWrapper, typename AuraView>
  AXAuraObjWrapper* CreateInternal(
      AuraView* aura_view,
      std::map<AuraView*, ui::AXNodeID>* aura_view_to_id_map);

  template <typename AuraView>
  ui::AXNodeID GetIDInternal(
      AuraView* aura_view,
      const std::map<AuraView*, ui::AXNodeID>& aura_view_to_id_map) const;

  template <typename AuraView>
  void RemoveInternal(AuraView* aura_view,
                      std::map<AuraView*, ui::AXNodeID>* aura_view_to_id_map);

  // The window that should take a11y focus. This is for a window that needs to
  // work with accessibility features, but cannot take real focus. Gets set to
  // null if the window is destroyed.
  raw_ptr<aura::Window> a11y_override_window_ = nullptr;

  // Observes |a11y_override_window_| for destruction and sets it to null in
  // that case.
  std::unique_ptr<A11yOverrideWindowObserver> a11y_override_window_observer_;

  std::map<views::View*, ui::AXNodeID> view_to_id_map_;
  std::map<views::AXVirtualView*, ui::AXNodeID> virtual_view_to_id_map_;
  std::map<views::Widget*, ui::AXNodeID> widget_to_id_map_;
  std::map<aura::Window*, ui::AXNodeID> window_to_id_map_;

  std::map<ui::AXNodeID, std::unique_ptr<AXAuraObjWrapper>> cache_;

  raw_ptr<Delegate> delegate_ = nullptr;

  std::vector<raw_ptr<aura::Window, VectorExperimental>> root_windows_;

  raw_ptr<aura::Window> focused_window_ = nullptr;

  raw_ptr<views::Widget> focused_widget_for_testing_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
