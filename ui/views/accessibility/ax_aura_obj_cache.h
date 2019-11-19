// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/views/views_export.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace aura {
class Window;
}  // namespace aura

namespace views {
class AXAuraObjWrapper;
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

  // Get or create an entry in the cache.
  AXAuraObjWrapper* GetOrCreate(Widget* widget);
  AXAuraObjWrapper* GetOrCreate(aura::Window* window);

  // Gets an id given an Aura view.
  int32_t GetID(View* view) const;
  int32_t GetID(Widget* widget) const;
  int32_t GetID(aura::Window* window) const;

  // Removes an entry from this cache based on an Aura view.
  void Remove(View* view);
  void Remove(Widget* widget);

  // Removes |window| and optionally notifies delegate by sending an event on
  // the |parent| if provided.
  void Remove(aura::Window* window, aura::Window* parent);

  // Removes a view and all of its descendants from the cache.
  void RemoveViewSubtree(View* view);

  // Lookup a cached entry based on an id.
  AXAuraObjWrapper* Get(int32_t id);

  // Get all top level windows this cache knows about. Under classic ash and
  // SingleProcessMash this is a list of per-display root windows.
  void GetTopLevelWindows(std::vector<AXAuraObjWrapper*>* children);

  // Get the object that has focus.
  AXAuraObjWrapper* GetFocus();

  // Send a notification that the focused view may have changed.
  void OnFocusedViewChanged();

  // Tell our delegate to fire an event on a given object.
  void FireEvent(AXAuraObjWrapper* aura_obj, ax::mojom::Event event_type);

  // Notifies this cache of a change in root window.
  void OnRootWindowObjCreated(aura::Window* window);

  // Notifies this cache of a change in root window.
  void OnRootWindowObjDestroyed(aura::Window* window);

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  // Changes the behavior of GetFocusedView() so that it only considers
  // views within the given Widget, this enables making tests
  // involving focus reliable.
  void set_focused_widget_for_testing(views::Widget* widget) {
    focused_widget_for_testing_ = widget;
  }

 private:
  friend class base::NoDestructor<AXAuraObjCache>;

  View* GetFocusedView();

  // aura::client::FocusChangeObserver override.
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  template <typename AuraViewWrapper, typename AuraView>
  AXAuraObjWrapper* CreateInternal(
      AuraView* aura_view,
      std::map<AuraView*, int32_t>& aura_view_to_id_map);

  template <typename AuraView>
  int32_t GetIDInternal(
      AuraView* aura_view,
      const std::map<AuraView*, int32_t>& aura_view_to_id_map) const;

  template <typename AuraView>
  void RemoveInternal(AuraView* aura_view,
                      std::map<AuraView*, int32_t>& aura_view_to_id_map);

  std::map<views::View*, int32_t> view_to_id_map_;
  std::map<views::Widget*, int32_t> widget_to_id_map_;
  std::map<aura::Window*, int32_t> window_to_id_map_;

  std::map<int32_t, std::unique_ptr<AXAuraObjWrapper>> cache_;

  Delegate* delegate_ = nullptr;

  std::set<aura::Window*> root_windows_;

  views::Widget* focused_widget_for_testing_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
