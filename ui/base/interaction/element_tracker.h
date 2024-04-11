// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_TRACKER_H_
#define UI_BASE_INTERACTION_ELEMENT_TRACKER_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// Represents a unique type of event, you may create these as needed using the
// DECLARE_CUSTOM_ELEMENT_EVENT_TYPE() and DEFINE_CUSTOM_ELEMENT_EVENT_TYPE()
// macros (see definitions at the bottom of this file).
//
// For testing purposes, if you need a local event type guaranteed to avoid
// global name collisions, use DEFINE_LOCAL_ELEMENT_EVENT_TYPE() instead.
//
// Currently, custom event types are imlpemented using ElementIdentifier, since
// both have the same API requirements.
using CustomElementEventType = ElementIdentifier;

// Represents a visible UI element in a platform-agnostic manner.
//
// A pointer to this object may be stored after the element becomes visible, but
// is only valid until the "element hidden" event is called for this element;
// see `ElementTracker` below. If you want to hold a pointer that will be valid
// only as long as the element is visible, use a SafeElementReference.
//
// You should derive a class for each UI framework whose elements you wish to
// track. See README.md for information on how to create your own framework
// implementations.
class COMPONENT_EXPORT(UI_BASE) TrackedElement
    : public FrameworkSpecificImplementation {
 public:
  ~TrackedElement() override;

  ElementIdentifier identifier() const { return identifier_; }
  ElementContext context() const { return context_; }

  // Returns the bounds of the element on the screen, or an empty rect if it
  // cannot be determined.
  //
  // Note: it is not yet necessary to set up a general method for listening to
  // bounds changes, as they are (a) somewhat difficult to track and (b) tend to
  // be handled correctly by most frameworks in terms of element positioning
  // (e.g. anchoring logic for User Education help bubbles). Specific
  // implementations that need to do additional tracking can implement their own
  // methods.
  virtual gfx::Rect GetScreenBounds() const;

  // FrameworkSpecificImplementation:
  std::string ToString() const override;

 protected:
  TrackedElement(ElementIdentifier identifier, ElementContext context);

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

// Provides a delegate for UI framework-specific implementations to notify of
// element tracker events.
//
// An element must be visible before events can be sent for that element;
// NotifyElementHidden() must be called before the element is destroyed or
// changes context or identifier.
class COMPONENT_EXPORT(UI_BASE) ElementTrackerFrameworkDelegate {
 public:
  virtual void NotifyElementShown(TrackedElement* element) = 0;
  virtual void NotifyElementActivated(TrackedElement* element) = 0;
  virtual void NotifyElementHidden(TrackedElement* element) = 0;
  virtual void NotifyCustomEvent(TrackedElement* element,
                                 CustomElementEventType event_type) = 0;
};

// Tracks elements as they become visible, are activated by the user, and
// eventually become hidden. Tracks only visible elements.
//
// NOT THREAD SAFE. Should only be accessed from the main UI thread.
class COMPONENT_EXPORT(UI_BASE) ElementTracker
    : ElementTrackerFrameworkDelegate {
 public:
  // Callback that subscribers receive when the specified event occurs.
  // Note that if an element is destroyed in the middle of calling callbacks,
  // some callbacks may not be called and others may be called with a null
  // argument, so please check the validity of the element pointer.
  using Callback = base::RepeatingCallback<void(TrackedElement*)>;
  using Subscription = base::CallbackListSubscription;
  using ElementList = std::vector<TrackedElement*>;
  using Contexts = std::set<ElementContext>;

  // Identifier that should be used by each framework to create a
  // TrackedElement from an element that does not alreayd have an identifier.
  //
  // Currently, the identifier is not removed when the code that needs the
  // element completes, but in the future we may implement a ref-counting
  // system for systems that use a temporary identifier so that it does not
  // persist longer than it is needed.
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTemporaryIdentifier);

  // Gets the element tracker to be used by clients to subscribe to and receive
  // events.
  static ElementTracker* GetElementTracker();

  // Gets the delegate to be used by specific UI frameworks to send events.
  static ElementTrackerFrameworkDelegate* GetFrameworkDelegate();

  // Returns either the one element matching the given `id` and `context`, or
  // null if there are none. Will generate an error if there is more than one
  // element with `id` in `context`. Only visible elements are returned.
  //
  // Use when you want to verify that there's only one matching element in the
  // given context.
  TrackedElement* GetUniqueElement(ElementIdentifier id,
                                   ElementContext context);

  // Returns the same result as GetUniqueElement() except that no error is
  // generated if there is more than one matching element.
  //
  // Use when you just need *an* element in the given context, and don't care if
  // there's more than one.
  TrackedElement* GetFirstMatchingElement(ElementIdentifier id,
                                          ElementContext context);

  // Returns an element with identifier `id` from any context, or null if not
  // found. Contexts are not guaranteed to be searched in any particular order.
  TrackedElement* GetElementInAnyContext(ElementIdentifier id);

  // Returns a list of all visible elements with identifier `id` in `context`.
  // The list may be empty.
  ElementList GetAllMatchingElements(ElementIdentifier id,
                                     ElementContext context);

  // Returns all known elements with the given `id`. The context for each can
  // be retrieved from the TrackedElement itself. No order is guaranteed.
  ElementList GetAllMatchingElementsInAnyContext(ElementIdentifier id);

  // Returns whether an element with identifier `id` in `context` is visible.
  bool IsElementVisible(ElementIdentifier id, ElementContext context);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` becomes visible.
  Subscription AddElementShownCallback(ElementIdentifier id,
                                       ElementContext context,
                                       Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` becomes visible in any context.
  Subscription AddElementShownInAnyContextCallback(ElementIdentifier id,
                                                   Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` is activated by the user.
  Subscription AddElementActivatedCallback(ElementIdentifier id,
                                           ElementContext context,
                                           Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` is activated in any context.
  Subscription AddElementActivatedInAnyContextCallback(ElementIdentifier id,
                                                       Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` in `context` is hidden.
  //
  // Note: the TrackedElement* passed to the callback may not remain
  // valid after the call, even if the same element object in its UI framework
  // is re-shown (a new TrackedElement may be generated).
  Subscription AddElementHiddenCallback(ElementIdentifier id,
                                        ElementContext context,
                                        Callback callback);

  // Adds a callback that will be called whenever an element with identifier
  // `id` is hidden in any context.
  //
  // Note: the TrackedElement* passed to the callback may not remain
  // valid after the call, even if the same element object in its UI framework
  // is re-shown (a new TrackedElement may be generated).
  Subscription AddElementHiddenInAnyContextCallback(ElementIdentifier id,
                                                    Callback callback);

  // Adds a callback that will be called whenever an event of `event_type` is
  // generated within `context` by any element.
  Subscription AddCustomEventCallback(CustomElementEventType event_type,
                                      ElementContext context,
                                      Callback callback);

  // Adds a callback that will be called whenever an event of `event_type` is
  // generated within any context.
  Subscription AddCustomEventInAnyContextCallback(
      CustomElementEventType event_type,
      Callback callback);

  // Returns all known contexts.
  Contexts GetAllContextsForTesting() const;

  // Returns a list of all elements. Should only be used in a meta-testing
  // context, e.g. for testing the tracker itself, or for getting lists of
  // candidate elements for fuzzing input.
  //
  // If `in_context` is specified, only elements in that context will be
  // returned.
  ElementList GetAllElementsForTesting(
      std::optional<ElementContext> in_context = std::nullopt);

  // Adds a callback when any element is shown.
  Subscription AddAnyElementShownCallbackForTesting(Callback callback);

 private:
  friend class base::NoDestructor<ElementTracker>;
  class ElementData;
  class GarbageCollector;
  using LookupKey = std::pair<ElementIdentifier, ElementContext>;
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerTest, CleanupAfterElementHidden);
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerTest, CleanupAfterCallbacksRemoved);
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerTest, HideDuringShowCallback);

  ElementTracker();
  ~ElementTracker();

  // ElementTrackerFrameworkDelegate:
  void NotifyElementShown(TrackedElement* element) override;
  void NotifyElementActivated(TrackedElement* element) override;
  void NotifyElementHidden(TrackedElement* element) override;
  void NotifyCustomEvent(TrackedElement* element,
                         CustomElementEventType event_type) override;

  ElementData* GetOrAddElementData(ElementIdentifier id,
                                   ElementContext context);

  void MaybeCleanup(ElementData* data);

  // Use a list to keep track of elements we're in the process of sending
  // notifications for; this allows us to zero out the reference in realtime if
  // the element is deleted. We use a list because the individual elements need
  // to be memory-stable.
  std::list<raw_ptr<TrackedElement, CtnExperimental>> notification_elements_;
  std::map<LookupKey, ElementData> element_data_;
  base::RepeatingCallbackList<void(TrackedElement*)>
      any_element_shown_callbacks_;
  std::unique_ptr<GarbageCollector> gc_;
};

// Holds an TrackedElement reference and nulls it out if the element goes
// away. In other words, acts as a weak reference for TrackedElements.
class COMPONENT_EXPORT(UI_BASE) SafeElementReference {
 public:
  SafeElementReference();
  explicit SafeElementReference(TrackedElement* element);
  SafeElementReference(SafeElementReference&& other);
  SafeElementReference(const SafeElementReference& other);
  SafeElementReference& operator=(SafeElementReference&& other);
  SafeElementReference& operator=(const SafeElementReference& other);
  ~SafeElementReference();

  TrackedElement* get() const { return element_; }
  explicit operator bool() const { return element_; }
  bool operator!() const { return !element_; }
  bool operator==(const SafeElementReference& other) const {
    return element_ == other.element_;
  }
  bool operator!=(const SafeElementReference& other) const {
    return element_ != other.element_;
  }
  bool operator==(const TrackedElement* other) const {
    return element_ == other;
  }
  bool operator!=(const TrackedElement* other) const {
    return element_ != other;
  }

 private:
  void Subscribe();
  void OnElementHidden(TrackedElement* element);

  ElementTracker::Subscription subscription_;
  raw_ptr<TrackedElement> element_ = nullptr;
};

}  // namespace ui

// Macros for declaring custom element event types. Put the DECLARE call in
// your public header file and the DEFINE in corresponding .cc file. For local
// values to be used in tests, use DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE()
// defined below instead.
//
// Note: if you need to use the identifier outside the current component, use
// DECLARE/DEFINE_EXPORTED_... below.
#define DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(EventName) \
  DECLARE_ELEMENT_IDENTIFIER_VALUE(EventName)
#define DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(EventName) \
  DEFINE_ELEMENT_IDENTIFIER_VALUE(EventName)

// Macros for declaring custom element event types that can be accessed in other
// components. Put the DECLARE call in your public header file and the DEFINE
// call in the corresponding .cc file.
#define DECLARE_EXPORTED_CUSTOM_ELEMENT_EVENT_TYPE(ExportName, EventName) \
  DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ExportName, EventName)
#define DEFINE_EXPORTED_CUSTOM_ELEMENT_EVENT_TYPE(EventName) \
  DEFINE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(EventName)

// Macros for declaring custom class element event type. Put the DECLARE call in
// your .h file in your class declaration, and the DEFINE in the corresponding
// .cc file.
#define DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(EventName) \
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(EventName)
#define DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(ClassName, EventName) \
  DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ClassName, EventName)

// This produces a unique, mangled name that can safely be used in macros called
// by tests without having to worry about global name collisions. For production
// code, use DECLARE/DEFINE above instead. You should pass __FILE__ and __LINE__
// for `File`, and `Line`, respectively.
#define DEFINE_MACRO_CUSTOM_ELEMENT_EVENT_TYPE(File, Line, EventName) \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(File, Line, EventName)

// This produces a unique, mangled name that can safely be used in tests
// without having to worry about global name collisions. For production code,
// use DECLARE/DEFINE above instead.
#define DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(EventName) \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, EventName)

#endif  // UI_BASE_INTERACTION_ELEMENT_TRACKER_H_
