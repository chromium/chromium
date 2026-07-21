// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_
#define THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "third_party/rust/cxx/v1/cxx.h"

namespace quiche::structured_headers {
class Dictionary;
struct ParameterizedItem;
struct ParameterizedMember;
} // namespace quiche::structured_headers

namespace sfv {

class Parameters;

using Dictionary = quiche::structured_headers::Dictionary;
using List = std::vector<quiche::structured_headers::ParameterizedMember>;
using Member = quiche::structured_headers::ParameterizedMember;

Member& list_append_member(List&);

Member& dictionary_reset_key(Dictionary&, rust::Str key);

void set_member_boolean(Member&, bool);
void set_member_integer(Member&, int64_t);
void set_member_decimal(Member&, double);
void set_member_string(Member&, rust::Str);
void set_member_token(Member&, rust::Str);
void set_member_byte_sequence(Member&, rust::Slice<const uint8_t>);

void set_member_inner_list(Member&);

Parameters& get_member_params(Member&);
Parameters& get_item_params(Member&);

void set_parameter_boolean(Parameters&, rust::Str key, bool);
void set_parameter_integer(Parameters&, rust::Str key, int64_t);
void set_parameter_decimal(Parameters&, rust::Str key, double);
void set_parameter_string(Parameters&, rust::Str key, rust::Str);
void set_parameter_token(Parameters&, rust::Str key, rust::Str);
void set_parameter_byte_sequence(Parameters&, rust::Str key, rust::Slice<const uint8_t>);

}  // namespace sfv

#endif  // THIRD_PARTY_RUST_SFV_V0_14_WRAPPER_FUNCTIONS_H_
