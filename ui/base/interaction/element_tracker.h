// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_TRACKER_H_
#define UI_BASE_INTERACTION_ELEMENT_TRACKER_H_

#include <map>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

// Represents a UI element in a platform-agnostic manner.
//
// A pointer to this object may be stored after the element becomes visible, but
// is only valid until the "element hidden" event is called for this element;
// see `ElementTracker` below.
//
// You should derive a class for each UI framework whose elements you wish to
// track. See README.md for information on how to create your own framework
// implementations.
class COMPONENT_EXPORT(UI_BASE) ElementTrackerElement {
 public:
  // Used by IsA() and AsA() methods to do runtime type-checking.
  using FrameworkIdentifier = ElementIdentifier;

  virtual ~ElementTrackerElement();

  ElementIdentifier identifier() const { return identifier_; }
  ElementContext context() const { return context_; }

  // Returns whether this element is a specific subtype - for example, a
  // views::ViewsElementTrackerElement.
  template <typename T>
  bool IsA() const {
    return AsA<T>();
  }

  // Dynamically casts this element to a specific subtype, such as a
  // views::ViewsElementTrackerElement, returning null if the element is the
  // wrong type.
  template <typename T>
  T* AsA() {
    return GetInstanceFrameworkIdentifier() == T::GetFrameworkIdentifier()
               ? static_cast<T*>(this)
               : nullptr;
  }

  // Dynamically casts this element to a specific subtype, such as a
  // views::ViewsElementTrackerElement, returning null if the element is the
  // wrong type. This version converts const objects.
  template <typename T>
  const T* AsA() const {
    return GetInstanceFrameworkIdentifier() == T::GetFrameworkIdentifier()
               ? static_cast<const T*>(this)
               : nullptr;
  }

 protected:
  ElementTrackerElement(ElementIdentifier identifier, ElementContext context);

  // Override this in derived classes with a unique FrameworkIdentifier.
  // You must also define a static GetFrameworkIdentifier() method that returns
  // the same value.
  virtual FrameworkIdentifier GetInstanceFrameworkIdentifier() const = 0;

 private:
  // The identifier for this element that will be used by ElementTracker to
  // retrieve it.
  const ElementIdentifier identifier_;

  // The context of the element, corresponding to the main window the element is
  // associated with. See the ElementContext documentation in
  // element_identifier.h for more information on how to create appropriate
  // contexts for each UI framework.
  const ElementContext context_;
};

// These macros can be used to help define platform-specific subclasses of
// `ElementTrackerElement`.
#define DECLARE_ELEMENT_TRACKER_METADATA()             \
  static FrameworkIdentifier GetFrameworkIdentifier(); \
  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override;
#define DEFINE_ELEMENT_TRACKER_METADATA(ClassName)                   \
  ui::ElementTrackerElement::FrameworkIdentifier                     \
  ClassName::GetFrameworkIdentifier() {                              \
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(k##ClassName##Identifier); \
    return k##ClassName##Identifier;                                 \
  }                                                                  \
  ui::ElementTrackerElement::FrameworkIdentifier                     \
  ClassName::GetInstanceFrameworkIdentifier() const {                \
    return GetFrameworkIdentifier();                                 \
  }

// Provides a delegate for UI framework-specific implementations to notify of
// element tracker events.
//
// An element must be visible before events can be sent for that element;
// NotifyElementHidden() must be called before the element is destroyed or
// changes context or identifier.
class COMPONENT_EXPORT(UI_BASE) ElementTrackerFrameworkDelegate {
 public:
  virtual void NotifyElementShown(ElementTrackerElement* element) = 0;
  virtual void NotifyElementActivated(ElementTrackerElement* element) = 0;
  virtual void NotifyElementHidden(ElementTrackerElement* element) = 0;
};

// Tracks elements as they become visible, are activated by the user, and
// eventually become hidden. Tracks only visible elements.
//
// NOT THREAD SAFE. Should only be accessed from the main UI thread.
class COMPONENT_EXPORT(UI_BASE) ElementTracker
    : ElementTrackerFrameworkDelegate {
 public:
  using Callback = base::RepeatingCallback<void(ElementTrackerElement*)>;
  using Subscription = base::CallbackListSubscription;
  using ElementList = std::vector<ElementTrackerElement*>;

  // Gets the element tracker to be used by clients to subscribe to and receive
  // events.
  static ElementTracker* GetElementTracker();

  // Gets the delegate to be used by specific UI frameworks to send events.
  static ElementTrackerFrameworkDelegate* GetFrameworkDelegate();

  // Returns either the one element matching the given `id` and `context`, or
  // null if there are none. Will generate an error if there is more than one
  // element with `id|`in `context`. Only visible elements are returned.
  //
  // Use when you want to verify that there's only one matching element in the
  // given context.
  ElementTrackerElement* GetUniqueElement(ElementIdentifier id,
                                          ElementContext context);

  // Returns the same result as GetUniqueElement() except that no error is
  // generated if there is more than one matching element.
  //
  // Use when you just need *an* element in the given context, and don't care if
  // there's more than one.
  ElementTrackerElement* GetFirstMatchingElement(ElementIdentifier id,
                                                 ElementContext context);

  // Returns a list of all visible elements with identifier `id` in `context`.
  // The list may be empty.
  ElementList GetAllMatchingElements(ElementIdentifier id,
                                     ElementContext context);

  // Returns whether an element with identifier `id` in `context` is visible.
  bool IsElementVisible(ElementIdentifier id, ElementContext context);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` becomes visible.
  Subscription AddElementShownCallback(ElementIdentifier id,
                                       ElementContext context,
                                       Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` is activated by the user.
  Subscription AddElementActivatedCallback(ElementIdentifier id,
                                           ElementContext context,
                                           Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` is hidden.
  //
  // Note: the ElementTrackerElement* passed to the callback may not remain
  // valid after the call, even if the same element object in its UI framework
  // is re-shown (a new ElementTrackerElement may be generated).
  Subscription AddElementHiddenCallback(ElementIdentifier id,
                                        ElementContext context,
                                        Callback callback);

 private:
  friend class base::NoDestructor<ElementTracker>;
  class ElementData;
  using LookupKey = std::pair<ElementIdentifier, ElementContext>;
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerTest, CleanupAfterElementHidden);
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerTest, CleanupAfterCallbacksRemoved);

  ElementTracker();
  ~ElementTracker();

  // ElementTrackerFrameworkDelegate:
  void NotifyElementShown(ElementTrackerElement* element) override;
  void NotifyElementActivated(ElementTrackerElement* element) override;
  void NotifyElementHidden(ElementTrackerElement* element) override;

  ElementData* GetOrAddElementData(ElementIdentifier id,
                                   ElementContext context);

  void MaybeCleanup(ElementData* data);

  std::map<LookupKey, std::unique_ptr<ElementData>> element_data_;
  std::map<ElementTrackerElement*, ElementData*> element_to_data_lookup_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_TRACKER_H_
