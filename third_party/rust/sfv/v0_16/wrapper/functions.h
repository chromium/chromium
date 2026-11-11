// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_SFV_V0_16_WRAPPER_FUNCTIONS_H_
#define THIRD_PARTY_RUST_SFV_V0_16_WRAPPER_FUNCTIONS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "third_party/rust/cxx/v1/cxx.h"

namespace quiche::structured_headers {
class Item;
struct ParameterizedItem;
struct ParameterizedMember;
}  // namespace quiche::structured_headers

namespace sfv {

struct Dictionary;
struct InnerListParts;
struct ItemParts;

using BareItem = quiche::structured_headers::Item;
using Item = quiche::structured_headers::ParameterizedItem;
using Member = quiche::structured_headers::ParameterizedMember;
using InnerListItems = std::vector<Item>;
using List = std::vector<Member>;
using Parameters = std::vector<std::pair<std::string, BareItem>>;

ItemParts list_append_item(List&);
InnerListParts list_append_inner_list(List&);

ItemParts dictionary_append_item(Dictionary&, rust::Str key);
ItemParts dictionary_set_item(Dictionary&, size_t index);
InnerListParts dictionary_append_inner_list(Dictionary&, rust::Str key);
InnerListParts dictionary_set_inner_list(Dictionary&, size_t index);

void set_bare_item_boolean(BareItem&, bool);
void set_bare_item_integer(BareItem&, int64_t);
void set_bare_item_decimal(BareItem&, double);
void set_bare_item_string(BareItem&, rust::Str);
void set_bare_item_token(BareItem&, rust::Str);
void set_bare_item_byte_sequence(BareItem&, rust::Slice<const uint8_t>);

ItemParts inner_list_append_item(InnerListItems&);

BareItem& params_append(Parameters&, rust::Str key);
BareItem& params_get(Parameters&, size_t index);

}  // namespace sfv

#endif  // THIRD_PARTY_RUST_SFV_V0_16_WRAPPER_FUNCTIONS_H_
