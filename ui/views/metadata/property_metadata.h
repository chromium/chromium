// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_PROPERTY_METADATA_H_
#define UI_VIEWS_METADATA_PROPERTY_METADATA_H_

#include <string>
#include <type_traits>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/class_property.h"
#include "ui/views/metadata/metadata_cache.h"
#include "ui/views/metadata/metadata_types.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {
namespace metadata {
namespace internal {

template <typename TSource, typename TTarget, typename = void>
struct DeRefHelper {
  static TTarget Get(TSource value) { return value; }
};

template <typename TSource, typename TTarget>
struct DeRefHelper<
    TSource,
    TTarget,
    typename std::enable_if<!std::is_same<TSource, TTarget>::value>::type> {
  static TTarget Get(TSource value) { return *value; }
};

template <typename TKey, typename TValue>
struct ClassPropertyMetaDataTypeHelper;

template <typename TKValue_, typename TValue_>
struct ClassPropertyMetaDataTypeHelper<const ui::ClassProperty<TKValue_>* const,
                                       TValue_> {
  using TKValue = TKValue_;
  using TValue = TValue_;

  // Returns |value| when |TKValue| == |TValue|. Otherwise, TKValue must be the
  // pointer type to TValue, returns |*value| instead.
  // This is useful for owned propertyies like ui::ClassProperty<gfx::Insets*>
  // where we want to inspect the actual value, rather than the pointer.
  static TValue DeRef(TKValue value) {
    return DeRefHelper<TKValue, TValue>::Get(value);
  }
};

}  // namespace internal

// Represents meta data for a specific read-only property member of class
// |TClass|, with underlying type |TValue|, as the type of the actual member.
// Using a separate |TRet| type for the getter function's return type to allow
// it to return a type with qualifier and by reference.
template <typename TClass,
          typename TValue,
          typename TRet,
          TRet (TClass::*Get)() const,
          typename TConverter = TypeConverter<TValue>>
class ObjectPropertyReadOnlyMetaData : public MemberMetaDataBase {
 public:
  using MemberMetaDataBase::MemberMetaDataBase;
  ObjectPropertyReadOnlyMetaData(const ObjectPropertyReadOnlyMetaData&) =
      delete;
  ObjectPropertyReadOnlyMetaData& operator=(
      const ObjectPropertyReadOnlyMetaData&) = delete;
  ~ObjectPropertyReadOnlyMetaData() override = default;

  base::string16 GetValueAsString(View* obj) const override {
    if (!kTypeIsSerializable && !kTypeIsReadOnly)
      return base::string16();
    return TConverter::ToString((static_cast<TClass*>(obj)->*Get)());
  }

  PropertyFlags GetPropertyFlags() const override {
    return kTypeIsSerializable
               ? (PropertyFlags::kReadOnly | PropertyFlags::kSerializable)
               : PropertyFlags::kReadOnly;
  }

  const char* GetMemberNamePrefix() const override {
    return TConverter::PropertyNamePrefix();
  }

 private:
  static constexpr bool kTypeIsSerializable = TConverter::is_serializable;
  static constexpr bool kTypeIsReadOnly = TConverter::is_read_only;
};

// Represents meta data for a specific property member of class |TClass|, with
// underlying type |TValue|, as the type of the actual member.
// Allows for interaction with the property as if it were the underlying data
// type (|TValue|), but still uses the Property's functionality under the hood
// (so it will trigger things like property changed notifications).
template <typename TClass,
          typename TValue,
          typename TSig,
          TSig Set,
          typename TRet,
          TRet (TClass::*Get)() const,
          typename TConverter = TypeConverter<TValue>>
class ObjectPropertyMetaData
    : public ObjectPropertyReadOnlyMetaData<TClass,
                                            TValue,
                                            TRet,
                                            Get,
                                            TConverter> {
 public:
  using ObjectPropertyReadOnlyMetaData<TClass, TValue, TRet, Get, TConverter>::
      ObjectPropertyReadOnlyMetaData;
  ObjectPropertyMetaData(const ObjectPropertyMetaData&) = delete;
  ObjectPropertyMetaData& operator=(const ObjectPropertyMetaData&) = delete;
  ~ObjectPropertyMetaData() override = default;

  void SetValueAsString(View* obj, const base::string16& new_value) override {
    if (!kTypeIsSerializable || kTypeIsReadOnly)
      return;
    if (base::Optional<TValue> result = TConverter::FromString(new_value)) {
      (static_cast<TClass*>(obj)->*Set)(std::move(result.value()));
    }
  }

  MemberMetaDataBase::ValueStrings GetValidValues() const override {
    if (!kTypeIsSerializable)
      return {};
    return TConverter::GetValidStrings();
  }

  PropertyFlags GetPropertyFlags() const override {
    PropertyFlags flags = PropertyFlags::kEmpty;
    if (kTypeIsSerializable)
      flags = flags | PropertyFlags::kSerializable;
    if (kTypeIsReadOnly)
      flags = flags | PropertyFlags::kReadOnly;
    return flags;
  }

 private:
  static constexpr bool kTypeIsSerializable = TConverter::is_serializable;
  static constexpr bool kTypeIsReadOnly = TConverter::is_read_only;
};

// Represents metadata for a ui::ClassProperty attached on a class instance.
// Converts property value to |TValue| when possible. This allows inspecting
// the actual value when the property is a pointer of type |TValue*|.
template <typename TKey,
          typename TValue,
          typename TConverter = TypeConverter<TValue>>
class ClassPropertyMetaData : public MemberMetaDataBase {
 public:
  using TypeHelper = internal::ClassPropertyMetaDataTypeHelper<TKey, TValue>;
  ClassPropertyMetaData(TKey key, const std::string& property_type)
      : MemberMetaDataBase(key->name, property_type), key_(key) {}
  ClassPropertyMetaData(const ClassPropertyMetaData&) = delete;
  ClassPropertyMetaData& operator=(const ClassPropertyMetaData&) = delete;
  ~ClassPropertyMetaData() override = default;

  // Returns the property value as a string.
  // If the property value is an pointer of type |TKValue*| and
  // |TKValue| == |TValue|, dereferences the pointer.
  base::string16 GetValueAsString(View* obj) const override {
    typename TypeHelper::TKValue value = obj->GetProperty(key_);
    if (std::is_pointer<typename TypeHelper::TKValue>::value && !value) {
      return base::ASCIIToUTF16("(not assigned)");
    } else {
      // GetProperty() returns a pointer when this is an owned property.
      // If |TValue| is not pointer, DeRef() returns |*value|, otherwise
      // it returns |value| as it is.
      return TConverter::ToString(TypeHelper::DeRef(value));
    }
  }

  void SetValueAsString(View* obj, const base::string16& new_value) override {
    base::Optional<TValue> value = TConverter::FromString(new_value);
    if (value)
      obj->SetProperty(key_, *value);
  }

  PropertyFlags GetPropertyFlags() const override {
    PropertyFlags flags = PropertyFlags::kEmpty;
    if (kTypeIsSerializable)
      flags = flags | PropertyFlags::kSerializable;
    if (kTypeIsReadOnly)
      flags = flags | PropertyFlags::kReadOnly;
    return flags;
  }

 private:
  TKey key_;

  static constexpr bool kTypeIsSerializable = TConverter::is_serializable;
  static constexpr bool kTypeIsReadOnly = TConverter::is_read_only;
};

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_PROPERTY_METADATA_H_
