// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_HIGHLIGHTER_H_
#define UI_BASE_INTERACTION_ELEMENT_HIGHLIGHTER_H_

#include <memory>

#include "base/no_destructor.h"
#include "ui/base/interaction/framework_specific_registration_list.h"

namespace ui {

class TrackedElement;

// Provides a ways of highlighting things used to anchor bubbles in
// a framework-agnostic way.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) ElementHighlighter {
 public:
  // A representation of an active highlight. Releases it when destroyed
  class Highlight {
   public:
    virtual ~Highlight() = default;
  };

  // Provides support for TrackedElements from given framework to do
  // highlighting.
  class Backend : public ui::FrameworkSpecificImplementation {
   public:
    virtual bool CanBeHighlighted(TrackedElement& element) const = 0;

    // Should return nullptr if not supported (including because `element` is
    // from a different framework).
    virtual std::unique_ptr<Highlight> AddHighlight(
        TrackedElement& element) = 0;
  };

  ~ElementHighlighter();

  static ElementHighlighter* GetElementHighlighter();

  static std::unique_ptr<ElementHighlighter> MakeInstanceForTesting();

  // Returns true if `element` can be actually highlighted; if so
  // AddHighlight() will return non-null.
  bool CanBeHighlighted(TrackedElement* element) const;

  // Request the element be highlighted. The highlight will remain as long
  // as any Highlight objects returned by this method are alive.
  //
  // Will return nullptr if it can't highlight `element` (or if `element` is
  // null).
  std::unique_ptr<Highlight> AddHighlight(TrackedElement* element);

  template <class T, typename... Args>
  void MaybeRegisterBackend(Args&&... args) {
    backends_.MaybeRegister<T>(std::forward<Args>(args)...);
  }

 private:
  friend class base::NoDestructor<ElementHighlighter>;
  ElementHighlighter();

  ui::FrameworkSpecificRegistrationList<Backend> backends_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_HIGHLIGHTER_H_
