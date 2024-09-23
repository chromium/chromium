// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_PROPERTY_METADATA_H_
#define UI_BASE_METADATA_PROPERTY_METADATA_H_

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

#include "base/component_export.h"
#include "base/notreached.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_cache.h"
#include "ui/base/metadata/metadata_types.h"

namespace ui {
namespace metadata {
namespace internal {

template <typename TKey, typename TValue>
struct ClassPropertyMetaDataTypeHelper;

template <typename TKValue_, typename TValue_>
  requires(std::same_as<TKValue_, TValue_> || std::same_as<TKValue_, TValue_*>)
struct ClassPropertyMetaDataTypeHelper<const ui::ClassProperty<TKValue_>* const,
                                       TValue_> {
  using TKValue = TKValue_;
  using TValue = TValue_;

  // Returns |value| when |TKValue| == |TValue|. Otherwise, TKValue must be the
  // pointer type to TValue, returns |*value| instead.
  // This is useful for owned propertyies like ui::ClassProperty<gfx::Insets*>
  // where we want to inspect the actual value, rather than the pointer.
  static TValue DeRef(TKValue value) {
    if constexpr (std::same_as<TKValue, TValue*>) {
      return *value;
    }
    if constexpr (std::same_as<TKValue, TValue>) {
      return value;
    }
    NOTREACHED();
  }
};

// Works around static casting issues related to the void*. See the comment on
// the METADATA_ACCESSORS_INTERNAL_BASE macro for more information about why
// this is necessary. NOTE: The reinterpret_cast<> here is merely to bring
// ReinterpretToBaseClass() into scope and make it callable. The body of that
// function does not access |this|, so this call is safe.
template <typename TClass>
TClass* AsClass(void* obj) {
  return static_cast<TClass*>(
      reinterpret_cast<TClass*>(obj)->ReinterpretToBaseClass(obj));
}

}  // namespace internal

// Represents meta data for a specific read-only property member of class
// |TClass|, with underlying type |TValue|, as the type of the actual member.
// Using a separate |TRet| type for the getter function's return type to allow
// it to return a type with qualifier and by reference.
template <typename TClass,
          typename TValue,
          typename TRet,
          TRet (TClass::*Get)() const,
          typename TConverter = ui::metadata::TypeConverter<TValue>>
class ObjectPropertyReadOnlyMetaData : public ui::metadata::MemberMetaDataBase {
 public:
  using MemberMetaDataBase::MemberMetaDataBase;
  ObjectPropertyReadOnlyMetaData(const ObjectPropertyReadOnlyMetaData&) =
      delete;
  ObjectPropertyReadOnlyMetaData& operator=(
      const ObjectPropertyReadOnlyMetaData&) = delete;
  ~ObjectPropertyReadOnlyMetaData() override = default;

  std::u16string GetValueAsString(void* obj) const override {
    if constexpr (kTypeIsSerializable || kTypeIsReadOnly) {
      return TConverter::ToString((internal::AsClass<TClass>(obj)->*Get)());
    }
    return std::u16string();
  }

  ui::metadata::PropertyFlags GetPropertyFlags() const override {
    if constexpr (kTypeIsSerializable) {
      return ui::metadata::PropertyFlags::kReadOnly |
             ui::metadata::PropertyFlags::kSerializable;
    }
    return ui::metadata::PropertyFlags::kReadOnly;
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
          typename TConverter = ui::metadata::TypeConverter<TValue>>
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

  void SetValueAsString(void* obj, const std::u16string& new_value) override {
    if constexpr (kTypeIsSerializable && !kTypeIsReadOnly) {
      if (std::optional<TValue> result = TConverter::FromString(new_value)) {
        (internal::AsClass<TClass>(obj)->*Set)(std::move(result.value()));
      }
    }
  }

  ui::metadata::MemberMetaDataBase::ValueStrings GetValidValues()
      const override {
    if constexpr (kTypeIsSerializable) {
      return TConverter::GetValidStrings();
    }
    return {};
  }

  ui::metadata::PropertyFlags GetPropertyFlags() const override {
    ui::metadata::PropertyFlags flags = ui::metadata::PropertyFlags::kEmpty;
    if constexpr (kTypeIsSerializable) {
      flags = flags | ui::metadata::PropertyFlags::kSerializable;
    }
    if constexpr (kTypeIsReadOnly) {
      flags = flags | ui::metadata::PropertyFlags::kReadOnly;
    }
    return flags;
  }

 private:
  static constexpr bool kTypeIsSerializable = TConverter::is_serializable;
  static constexpr bool kTypeIsReadOnly = TConverter::is_read_only;
};

// Represents metadata for a ui::ClassProperty attached on a class instance.
// Converts property value to |TValue| when possible. This allows inspecting
// the actual value when the property is a pointer of type |TValue*|.
template <typename TClass,
          typename TKey,
          typename TValue,
          typename TConverter = ui::metadata::TypeConverter<TValue>>
class ClassPropertyMetaData : public ui::metadata::MemberMetaDataBase {
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
  std::u16string GetValueAsString(void* obj) const override {
    typename TypeHelper::TKValue value =
        internal::AsClass<TClass>(obj)->GetProperty(key_);
    if (std::is_pointer<typename TypeHelper::TKValue>::value && !value) {
      return u"(not assigned)";
    } else {
      // GetProperty() returns a pointer when this is an owned property.
      // If |TValue| is not pointer, DeRef() returns |*value|, otherwise
      // it returns |value| as it is.
      return TConverter::ToString(TypeHelper::DeRef(value));
    }
  }

  void SetValueAsString(void* obj, const std::u16string& new_value) override {
    std::optional<TValue> value = TConverter::FromString(new_value);
    if (value)
      internal::AsClass<TClass>(obj)->SetProperty(key_, *value);
  }

  ui::metadata::PropertyFlags GetPropertyFlags() const override {
    ui::metadata::PropertyFlags flags = ui::metadata::PropertyFlags::kEmpty;
    if constexpr (kTypeIsSerializable) {
      flags = flags | ui::metadata::PropertyFlags::kSerializable;
    }
    if constexpr (kTypeIsReadOnly) {
      flags = flags | ui::metadata::PropertyFlags::kReadOnly;
    }
    return flags;
  }

 private:
  TKey key_;

  static constexpr bool kTypeIsSerializable = TConverter::is_serializable;
  static constexpr bool kTypeIsReadOnly = TConverter::is_read_only;
};

}  // namespace metadata
}  // namespace ui

#endif  // UI_BASE_METADATA_PROPERTY_METADATA_H_
