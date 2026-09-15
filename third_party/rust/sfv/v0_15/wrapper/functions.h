// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_
#define THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_

#include <stdint.h>

#include <vector>

#include "third_party/rust/cxx/v1/cxx.h"

namespace quiche::structured_headers {
class Dictionary;
class Item;
struct InnerList;
struct ParameterizedItem;
struct ParameterizedMember;
} // namespace quiche::structured_headers

namespace sfv {

class Parameters;

using BareItem = quiche::structured_headers::Item;
using Dictionary = quiche::structured_headers::Dictionary;
using InnerList = quiche::structured_headers::InnerList;
using Item = quiche::structured_headers::ParameterizedItem;
using List = std::vector<quiche::structured_headers::ParameterizedMember>;

Item& list_append_item(List&);
InnerList& list_append_inner_list(List&);

Item& dictionary_set_item(Dictionary&, rust::Str key);
InnerList& dictionary_set_inner_list(Dictionary&, rust::Str key);

void set_bare_item_boolean(BareItem&, bool);
void set_bare_item_integer(BareItem&, int64_t);
void set_bare_item_decimal(BareItem&, double);
void set_bare_item_string(BareItem&, rust::Str);
void set_bare_item_token(BareItem&, rust::Str);
void set_bare_item_byte_sequence(BareItem&, rust::Slice<const uint8_t>);

Item& inner_list_append_item(InnerList&);
Parameters& get_inner_list_params(InnerList&);

BareItem& get_item_bare_item(Item&);
Parameters& get_item_params(Item&);

BareItem& get_or_insert_param(Parameters&, rust::Str key);

}  // namespace sfv

#endif  // THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_
