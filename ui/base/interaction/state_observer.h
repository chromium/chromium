// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_STATE_OBSERVER_H_
#define UI_BASE_INTERACTION_STATE_OBSERVER_H_

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui::test {

// Base class for all state change observers. Observes some state of the system,
// which we can then wait on.
//
// Used with `InteractiveTestApi::ObserveState()` and `WaitForState()`.
//
// Value type `T` must be default-constructible and copy-assignable.
template <typename T>
class StateObserver {
 public:
  using StateChangedCallback = base::RepeatingCallback<void(T)>;
  using ValueType = T;

  StateObserver() = default;
  virtual ~StateObserver() { OnStateObserverStateChanged(T()); }

  // Copy and assignment are not allowed.
  StateObserver(const StateObserver&) = delete;
  void operator=(const StateObserver&) = delete;

  // Returns the initial state. Override to provide your own logic; returns
  // `T()` by default.
  virtual T GetStateObserverInitialState() const { return T(); }

  // Used by the owning test to set the state change callback. Do not call
  // directly. The caller should ensure the the new callback is issued with the
  // current state.
  void SetStateObserverStateChangedCallback(StateChangedCallback callback) {
    CHECK(!state_changed_callback_);
    state_changed_callback_ = std::move(callback);
  }

 protected:
  // Call to update the state.
  void OnStateObserverStateChanged(T state) {
    if (state_changed_callback_) {
      state_changed_callback_.Run(state);
    }
  }

 private:
  StateChangedCallback state_changed_callback_;
};

// State observer that uses a `ScopedObservation<T, Source, Observer>` to watch
// for state changes using an observer pattern.
//
// You will still need to override the specific observer methods to detect:
//  - The actual state change
//     - call `OnStateObserverStateChanged()`
//  - The "source destroyed" message (optional)
//     - call `OnObservationStateObserverSourceDestroyed()`
//
// If the initial state may vary, you can also override
// `GetStateObserverInitialState()`. Use `source()` to extract the relevant
// information.
//
// An example can be found in chrome/test/interaction/README.md.
template <typename T, typename Source, typename Observer>
class ObservationStateObserver : public StateObserver<T>, public Observer {
 public:
  explicit ObservationStateObserver(Source* source_object)
      : source_(source_object) {
    observation_.Observe(source_object);
  }

  ~ObservationStateObserver() override = default;

  Source* source() const { return source_; }

 protected:
  // Call to indicate that the `source` object is going away. If the object will
  // never go away during the scope of this object, or there is no callback to
  // detect destruction, you can ignore this method.
  //
  // Resets the state value to `T()`.
  void OnObservationStateObserverSourceDestroyed() {
    StateObserver<T>::OnStateObserverStateChanged(T());
    observation_.Reset();
    source_ = nullptr;
  }

 private:
  raw_ptr<Source> source_;
  base::ScopedObservation<Source, Observer> observation_{this};
};

// Uniquely identifies a state associated with `ObserverType`.
//
// Use the DECLARE/DEFINE macros below to create unique identifiers, similarly
// to how ElementIdentifier, etc. work.
template <typename ObserverType>
class StateIdentifier final {
 public:
  constexpr StateIdentifier() = default;

  explicit constexpr StateIdentifier(ElementIdentifier identifier)
      : identifier_(identifier) {}

  constexpr ElementIdentifier identifier() const { return identifier_; }

  constexpr explicit operator bool() const {
    return static_cast<bool>(identifier_);
  }

  constexpr bool operator!() const { return !identifier_; }

  constexpr bool operator==(const StateIdentifier<ObserverType>& other) const {
    return identifier_ == other.identifier_;
  }

  constexpr bool operator!=(const StateIdentifier<ObserverType>& other) const {
    return identifier_ != other.identifier_;
  }

  constexpr bool operator<(const StateIdentifier<ObserverType>& other) const {
    return identifier_ < other.identifier_;
  }

 private:
  ElementIdentifier identifier_;
};

template <typename T>
extern void PrintTo(StateIdentifier<T> state_identifier, std::ostream* os) {
  *os << "StateIdentifier " << state_identifier.identifier().GetRawValue()
      << " [" << state_identifier.identifier().GetName() << "]";
}

template <typename T>
extern std::ostream& operator<<(std::ostream& os,
                                StateIdentifier<T> state_identifier) {
  PrintTo(state_identifier, os);
  return os;
}

}  // namespace ui::test

// The following macros create a state identifier value for use in tests.
//
// The associated type of observer should be specified along with the unique
// identifier name:
// ```
// DECLARE_STATE_IDENTIFIER_VALUE(MyObserverType, kMyState);
// ```
//
// `DECLARE_STATE_IDENTIFIER_VALUE()` and `DEFINE_STATE_IDENTIFIER_VALUE()` are
// for use in .h and .cc files, respectively, as with declaring
// `ElementIdentifier`s.
//
// To declare a `StateIdentifier` local to a .cc file or method body, use
// DEFINE_LOCAL_STATE_IDENTIFIER_VALUE() instead. This will create a file- and
// line-mangled name that will not suffer name collisions with other
// identifiers.

#define DECLARE_STATE_IDENTIFIER_VALUE(ObserverType, Name) \
  DECLARE_ELEMENT_IDENTIFIER_VALUE(Name##Impl);            \
  extern const ui::test::StateIdentifier<ObserverType> Name

#define DEFINE_STATE_IDENTIFIER_VALUE(ObserverType, Name) \
  DEFINE_ELEMENT_IDENTIFIER_VALUE(Name##Impl);            \
  constexpr ui::test::StateIdentifier<ObserverType> Name(Name##Impl)

#define DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ObserverType, Name)          \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, Name##Impl); \
  constexpr ui::test::StateIdentifier<ObserverType> Name(Name##Impl)

#endif  // UI_BASE_INTERACTION_STATE_OBSERVER_H_
