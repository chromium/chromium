// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ANCHORED_DIALOG_REGISTRY_H_
#define UI_BASE_INTERACTION_ANCHORED_DIALOG_REGISTRY_H_

#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/base/interaction/implementation_list.h"
#include "ui/base/interaction/safe_castable.h"

namespace ui {

class TrackedElement;

// Lets a dialog that is anchored to a TrackedElement (typically a bubble) take
// part in the sequential focus navigation (Tab / Shift+Tab) of the UI that
// hosts the anchor, in a framework-agnostic way: the dialog is inserted into
// the focus order right after the anchor and everything inside it.
//
// This is the counterpart of views::kAnchoredDialogKey, which does the same
// for dialogs anchored to a views::View. Each UI framework that can host
// anchors that are not Views (e.g. WebUI) provides a Backend.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) AnchoredDialogRegistry {
 public:
  // Implemented by the anchored dialog.
  class FocusTarget {
   public:
    // Moves focus into the dialog: to its first focusable item, or to its last
    // one if `reverse` is true. Returns whether focus was moved.
    virtual bool FocusFromAnchor(bool reverse) = 0;

   protected:
    virtual ~FocusTarget() = default;
  };

  // The dialog stays in the anchor's focus order while this is alive.
  class Registration {
   public:
    virtual ~Registration() = default;
  };

  // Provides support for TrackedElements from a given UI framework.
  class Backend : public SafeCastable {
   public:
    // Should return null if `anchor` is not supported (including because it is
    // from a different framework). If more than one dialog is registered for
    // the same anchor, the most recently registered one should be entered.
    [[nodiscard]] virtual std::unique_ptr<Registration> Register(
        TrackedElement& anchor,
        FocusTarget& target) = 0;
  };

  AnchoredDialogRegistry(const AnchoredDialogRegistry&) = delete;
  AnchoredDialogRegistry& operator=(const AnchoredDialogRegistry&) = delete;
  ~AnchoredDialogRegistry();

  static AnchoredDialogRegistry* GetInstance();

  static std::unique_ptr<AnchoredDialogRegistry> MakeInstanceForTesting();

  // Inserts `target` into the focus order of `anchor` until the returned
  // Registration is destroyed. `target` must outlive the Registration.
  //
  // Returns null if `anchor` is null or no backend supports it.
  [[nodiscard]] std::unique_ptr<Registration> Register(TrackedElement* anchor,
                                                       FocusTarget& target);

  // Adds a backend of type `T` if one isn't already present.
  template <class T, typename... Args>
  void MaybeRegisterBackend(Args&&... args) {
    backends_.MaybeRegister<T>(std::forward<Args>(args)...);
  }

  // Returns the backend of type `T`, or null if none is registered.
  template <class T>
  T* GetBackend() {
    return backends_.GetImplementation<T>();
  }

 private:
  friend class base::NoDestructor<AnchoredDialogRegistry>;

  AnchoredDialogRegistry();

  ImplementationList<Backend> backends_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ANCHORED_DIALOG_REGISTRY_H_
