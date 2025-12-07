// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ATTRIBUTES_H_
#define UI_ACCESSIBILITY_AX_ATTRIBUTES_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// A sorted mapping of attribute identifiers to values.
template <typename Identifier, typename Traits>
class AX_BASE_EXPORT AXAttributes {
 public:
  // The type of a value in the collection.
  using ValueType = Traits::ValueType;

  // The type returned by `Get` -- either a value for fundamental types or a
  // const-ref to an empty value for complex types.
  using ReturnType = Traits::ReturnType;

  // The type of the underlying sorted container.
  using AttributeMap = base::flat_map<Identifier, ValueType>;

  // Returns true if the attribute `id` is present in the collection.
  bool Has(Identifier id) const { return attributes_.contains(id); }

  // Returns the attribute `id` if it is in the collection, or the collection's
  // default value if it is not present. In the case of a collection holding
  // complex objects (e.g., std::string), the returned reference is valid only
  // until the collection is modified.
  ReturnType Get(Identifier id) const {
    if (auto iter = attributes_.find(id); iter != attributes_.end()) {
      return iter->second;
    }
    return Traits::GetDefault();
  }

  // Sets the value for the attribute `id` in the collection.
  void Set(Identifier id, ValueType value) {
    attributes_.insert_or_assign(id, std::move(value));
  }

  // Removes the attribute `id` from the collection.
  void Remove(Identifier id) { attributes_.erase(id); }

  // Operations on the collection.
  AttributeMap::size_type size() const { return attributes_.size(); }
  AttributeMap::iterator begin() { return attributes_.begin(); }
  AttributeMap::iterator end() { return attributes_.end(); }
  AttributeMap::const_iterator begin() const { return attributes_.begin(); }
  AttributeMap::const_iterator end() const { return attributes_.end(); }
  void clear() { attributes_.clear(); }

  // Raw access to the underlying container.
  const AttributeMap& container() const { return attributes_; }
  AttributeMap& container() { return attributes_; }

  friend bool operator==(const AXAttributes& lhs,
                         const AXAttributes& rhs) = default;

 private:
  AttributeMap attributes_;
};

// Traits for fundamental value types (e.g., int32_t, float, bool). `Default` is
// the value to be returned when an attribute is not present in a collection.
template <typename Value, Value Default = Value()>
struct FundamentalAttributeTraits {
  using ValueType = Value;
  using ReturnType = Value;
  static constexpr ReturnType GetDefault() { return Default; }
};

// Traits for complex value types (e.g., std::string, std::vector<>). An empty
// instance is returned when an attribute is not present in a collection.
template <typename Value>
struct AX_BASE_EXPORT ObjectAttributeTraits {
  using ValueType = Value;
  using ReturnType = const Value&;
  static ReturnType GetDefault();
};

// The various collection types.
using AXIntAttributes =
    AXAttributes<ax::mojom::IntAttribute, FundamentalAttributeTraits<int32_t>>;
using AXStringAttributes = AXAttributes<ax::mojom::StringAttribute,
                                        ObjectAttributeTraits<std::string>>;
using AXFloatAttributes =
    AXAttributes<ax::mojom::FloatAttribute, FundamentalAttributeTraits<float>>;
using AXBoolAttributes =
    AXAttributes<ax::mojom::BoolAttribute, FundamentalAttributeTraits<bool>>;
using AXIntListAttributes =
    AXAttributes<ax::mojom::IntListAttribute,
                 ObjectAttributeTraits<std::vector<int32_t>>>;
using AXStringListAttributes =
    AXAttributes<ax::mojom::StringListAttribute,
                 ObjectAttributeTraits<std::vector<std::string>>>;

// Getters for the (global) default values returned for complex collections.

template <>
const std::string& ObjectAttributeTraits<std::string>::GetDefault();

template <>
const std::vector<int32_t>&
ObjectAttributeTraits<std::vector<int32_t>>::GetDefault();

template <>
const std::vector<std::string>&
ObjectAttributeTraits<std::vector<std::string>>::GetDefault();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ATTRIBUTES_H_
