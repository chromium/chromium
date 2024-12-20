// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLASS_PROPERTY_H_
#define UI_BASE_CLASS_PROPERTY_H_

#include <stdint.h>

#include <concepts>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

#include "base/bit_cast.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "base/types/is_complete.h"
#include "ui/base/ui_base_types.h"

// To define a new `ClassProperty`:
// ```
//  #include "foo/foo_export.h"
//  #include "ui/base/class_property.h"
//
//  DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(FOO_EXPORT, MyType)
//  namespace foo {
//    // A value type or a pointer you don't want automatically deleted.
//    // `MyType` must fit in 64 bits.
//    DEFINE_UI_CLASS_PROPERTY_KEY(MyType, kMyKey, MyDefault)
//
//    // An object of any size, which will be stored on the heap.
//    DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey)
//  }  // namespace foo
// ```
//
// To define a new type used for a `ClassProperty`:
// ```
//  // outside all namespaces:
//  DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(FOO_EXPORT, MyType)
// ```
// If a property type is not exported, use
// `DEFINE_UI_CLASS_PROPERTY_TYPE(MyType)`, which is shorthand for
// `DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(, MyType)`.
//
// Cascading properties:
//
// Use the `DEFINE_CASCADING_XXX` macros to create a class property type that
// will automatically search up an instance hierarchy for the first defined
// property. This only affects the `GetProperty()` call. `SetProperty()` will
// still explicitly set the value on the given instance. This is useful for
// hierarchies of instances which a single set property can effect a whole sub-
// tree of instances.
//
// In order to use this feature, you must override `GetParentHandler()` on the
// class that extends `PropertyHandler`.

namespace ui {

// Type of a function to delete a property that this window owns.
using PropertyDeallocator = void(*)(int64_t value);

template<typename T>
struct ClassProperty {
  T default_value;
  const char* name;
  bool cascading = false;
  PropertyDeallocator deallocator;
};

namespace subtle {
class PropertyHelper;
}

class COMPONENT_EXPORT(UI_BASE) PropertyHandler {
 public:
  PropertyHandler();
  PropertyHandler(PropertyHandler&&);
  PropertyHandler& operator=(PropertyHandler&&);
  virtual ~PropertyHandler();

  // Sets the `value` of the given unowned `property`. Setting to the default
  // value (e.g. null) removes the property. If `T` is a pointer, the lifetime
  // of the underlying object is managed by the caller. For caller convenience
  // (e.g. for tail calls), returns the new value.
  //
  // CAUTION: Do not use this to pass in a raw pointer for an owned property,
  // due to the risk of UAF/double-frees. Use the owned property setters below
  // that set `ClassProperty<T*>`s.
  template <typename T>
  T SetProperty(const ClassProperty<T>* property, T value);

  // Convenience wrapper for the above, for when the supplied value is not the
  // same type as the property.
  template <typename T, typename U>
    requires(std::constructible_from<T, U &&> &&
             !std::same_as<T, std::remove_cvref_t<U>>)
  T SetProperty(const ClassProperty<T>* property, U&& value);

  // Sets the `value` of the given owned `property`. Setting to null removes the
  // property. Manages a heap allocation for any set value, so callers can
  // mutate or destroy the supplied value without risk of UAF. For caller
  // convenience (e.g. for tail calls), returns (a non-owning pointer to) the
  // new value.
  template <typename T, typename U>
    requires(!std::same_as<T*, std::remove_cvref_t<U>> &&
             std::constructible_from<T, U &&> && std::assignable_from<T&, U &&>)
  T* SetProperty(const ClassProperty<T*>* property, U&& value);

  // As above, but for callers who already have a heap-allocated object.
  template <typename T, typename U>
    requires(std::convertible_to<U*, T*>)
  T* SetProperty(const ClassProperty<T*>* property, std::unique_ptr<U> value);

  // For unowned properties: Returns the value of the given `property`, or the
  // property-specific default value if the property is not currently set.
  // For owned properties: Returns (a non-owning pointer to) the value of the
  // given `property`, or null if the property is not currently set.
  template<typename T>
  T GetProperty(const ClassProperty<T>* property) const;

  // Removes any currently-set value for `property`. For unowned properties,
  // future calls to `GetProperty()` will return the property-specific default
  // value. For owned properties, runs the deallocator before returning.
  template <typename T>
  void ClearProperty(const ClassProperty<T>* property);

  // Takes the ownership of all the properties in |other|, overwriting any
  // similarly-keyed existing properties without affecting existing ones with
  // different keys.
  void AcquireAllPropertiesFrom(PropertyHandler&& other);

  // Returns the value of all properties with a non-default value.
  std::set<const void*> GetAllPropertyKeys() const;

 protected:
  friend class subtle::PropertyHelper;

  // TODO(pkasting): Consider calling this only when the values differ.
  virtual void AfterPropertyChange(const void* key, int64_t old_value) {}

  // Override this function when inheriting this class on a class or classes
  // in which instances are arranged in a parent-child relationship and
  // the intent is to use cascading properties.
  virtual PropertyHandler* GetParentHandler() const;

  void ClearProperties();

  // Implements `SetProperty()`. Returns the old value of the property; for
  // owned properties, the caller must deallocate this.
  int64_t SetPropertyInternal(const void* key,
                              const char* name,
                              PropertyDeallocator deallocator,
                              int64_t value,
                              int64_t default_value);

  // Implements `GetProperty()`. If `search_parent` is true, uses
  // `GetParentHandler()` to traverse cascading values.
  int64_t GetPropertyInternal(const void* key,
                              int64_t default_value,
                              bool search_parent) const;

 private:
  // Value struct to keep the name and deallocator for this property.
  // Key cannot be used for this purpose because it can be char* or
  // ClassProperty<>.
  struct Value {
    const char* name;
    int64_t value;
    PropertyDeallocator deallocator;
  };
  using PropMap = std::map<const void*, Value>;

  PropMap prop_map_;
};

// No single new-style cast works for every conversion to/from int64_t, so we
// need this helper class.
template<typename T>
class ClassPropertyCaster {
 public:
  static int64_t ToInt64(T x)
    requires(sizeof(T) <= sizeof(int64_t))
  {
    return static_cast<int64_t>(x);
  }
  static T FromInt64(int64_t x) { return static_cast<T>(x); }
};
template<typename T>
class ClassPropertyCaster<T*> {
 public:
  static int64_t ToInt64(T* x) {
    static_assert(sizeof(T*) <= sizeof(int64_t));
    return reinterpret_cast<int64_t>(x);
  }
  static T* FromInt64(int64_t x) { return reinterpret_cast<T*>(x); }
};
template <>
class ClassPropertyCaster<base::TimeDelta> {
 public:
  static int64_t ToInt64(base::TimeDelta x) { return x.InMicroseconds(); }
  static base::TimeDelta FromInt64(int64_t x) { return base::Microseconds(x); }
};
template <>
class ClassPropertyCaster<float> {
 public:
  static int64_t ToInt64(float x) {
    static_assert(sizeof(float) == sizeof(int32_t));
    return base::bit_cast<int32_t>(x);
  }
  static float FromInt64(int64_t x) {
    return base::bit_cast<float>(static_cast<int32_t>(x));
  }
};

namespace subtle {

class COMPONENT_EXPORT(UI_BASE) PropertyHelper {
 public:
  template <typename T>
  static T Set(::ui::PropertyHandler* handler,
               const ::ui::ClassProperty<T>* property,
               T value) {
    const int64_t old = handler->SetPropertyInternal(
        property, property->name, property->deallocator,
        ClassPropertyCaster<T>::ToInt64(value),
        ClassPropertyCaster<T>::ToInt64(property->default_value));
    if (property->deallocator &&
        old != ClassPropertyCaster<T>::ToInt64(property->default_value)) {
      (*property->deallocator)(old);
    }
    return value;
  }
  template <typename T>
  static T Get(const ::ui::PropertyHandler* handler,
               const ::ui::ClassProperty<T>* property,
               bool allow_cascade) {
    return ClassPropertyCaster<T>::FromInt64(handler->GetPropertyInternal(
        property, ClassPropertyCaster<T>::ToInt64(property->default_value),
        property->cascading && allow_cascade));
  }
  template <typename T>
  static void Clear(::ui::PropertyHandler* handler,
                    const ::ui::ClassProperty<T>* property) {
    Set(handler, property, property->default_value);
  }
};

}  // namespace subtle

template <typename T, typename U>
  requires(std::constructible_from<T, U &&> &&
           !std::same_as<T, std::remove_cvref_t<U>>)
T PropertyHandler::SetProperty(const ClassProperty<T>* property, U&& value) {
  return SetProperty(property, T(std::forward<U>(value)));
}

template <typename T, typename U>
  requires(!std::same_as<T*, std::remove_cvref_t<U>> &&
           std::constructible_from<T, U &&> && std::assignable_from<T&, U &&>)
T* PropertyHandler::SetProperty(const ClassProperty<T*>* property, U&& value) {
  // Reuse any existing allocation.
  if (T* const old = subtle::PropertyHelper::Get<T*>(this, property, false)) {
    T temp = std::exchange(*old, std::forward<U>(value));
    AfterPropertyChange(property, ClassPropertyCaster<T*>::ToInt64(&temp));
    return old;
  }
  return SetProperty(property, std::make_unique<T>(std::forward<U>(value)));
}

template <typename T, typename U>
  requires(std::convertible_to<U*, T*>)
T* PropertyHandler::SetProperty(const ClassProperty<T*>* property,
                                std::unique_ptr<U> value) {
  DCHECK(property->deallocator);
  return subtle::PropertyHelper::Set<T*>(this, property, value.release());
}

}  // namespace ui

// Macros to declare the property getter/setter template functions.
#define DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(EXPORT, T)                \
  namespace ui {                                                          \
  template <>                                                             \
  EXPORT T PropertyHandler::SetProperty(const ClassProperty<T>* property, \
                                        T value);                         \
  template <>                                                             \
  EXPORT T                                                                \
  PropertyHandler::GetProperty(const ClassProperty<T>* property) const;   \
  template <>                                                             \
  EXPORT void PropertyHandler::ClearProperty(                             \
      const ClassProperty<T>* property);                                  \
  }  // namespace ui

// Macros to instantiate the property getter/setter template functions.
#define DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(EXPORT, T)                    \
  namespace ui {                                                             \
  template <>                                                                \
  EXPORT T PropertyHandler::SetProperty(const ClassProperty<T>* property,    \
                                        T value) {                           \
    /* TODO(kylixrd, pbos): Once all the call-sites are fixed to only use */ \
    /* the unique_ptr version for owned properties, add the following */     \
    /* DCHECK to guard against passing raw pointers for owned properties. */ \
    /* DCHECK(!std::is_pointer<T>::value || !property->deallocator); */      \
    return subtle::PropertyHelper::Set<T>(this, property, value);            \
  }                                                                          \
  template <>                                                                \
  EXPORT T                                                                   \
  PropertyHandler::GetProperty(const ClassProperty<T>* property) const {     \
    return subtle::PropertyHelper::Get<T>(this, property, true);             \
  }                                                                          \
  template <>                                                                \
  EXPORT void PropertyHandler::ClearProperty(                                \
      const ClassProperty<T>* property) {                                    \
    subtle::PropertyHelper::Clear<T>(this, property);                        \
  }                                                                          \
  }  // namespace ui

#define DEFINE_UI_CLASS_PROPERTY_TYPE(T) \
  DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(, T)

#define DEFINE_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, DEFAULT, CASCADES)     \
  static_assert(sizeof(TYPE) <= sizeof(int64_t), "property type too large"); \
  const ::ui::ClassProperty<TYPE> NAME##_Value = {DEFAULT, #NAME, CASCADES,  \
                                                  nullptr};                  \
  const ::ui::ClassProperty<TYPE>* const NAME = &NAME##_Value;

#define DEFINE_UI_CLASS_PROPERTY_KEY(TYPE, NAME, DEFAULT) \
  DEFINE_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, DEFAULT, false)

#define DEFINE_CASCADING_UI_CLASS_PROPERTY_KEY(TYPE, NAME, DEFAULT) \
  DEFINE_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, DEFAULT, true)

#define DEFINE_OWNED_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, CASCADES) \
  static_assert(base::IsComplete<TYPE>);                              \
  static_assert(sizeof(TYPE*) <= sizeof(int64_t));                    \
  namespace {                                                         \
  void Deallocator##NAME(int64_t p) {                                 \
    delete ::ui::ClassPropertyCaster<TYPE*>::FromInt64(p);            \
  }                                                                   \
  constexpr ::ui::ClassProperty<TYPE*> NAME##_Value = {               \
      nullptr, #NAME, CASCADES, &Deallocator##NAME};                  \
  } /* namespace */                                                   \
  constexpr const ::ui::ClassProperty<TYPE*>* NAME = &NAME##_Value;

#define DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(TYPE, NAME) \
  DEFINE_OWNED_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, false)

#define DEFINE_CASCADING_OWNED_UI_CLASS_PROPERTY_KEY(TYPE, NAME) \
  DEFINE_OWNED_UI_CLASS_PROPERTY_KEY_IMPL(TYPE, NAME, true)

#endif  // UI_BASE_CLASS_PROPERTY_H_
