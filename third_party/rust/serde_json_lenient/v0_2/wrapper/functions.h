// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_
#define THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_

#include <stdint.h>

#include "third_party/rust/cxx/v1/cxx.h"

namespace base {
class DictValue;
class ListValue;
}  // namespace base

namespace serde_json_lenient {

using Dict = base::DictValue;
using List = base::ListValue;

// C++ functions that provide the functionality for each stop by the JSON
// deserializer in order to construct the output in C++ from the JSON
// deserialization.
//
// TODO(danakj): CXX does not support function pointers, so we need to use a
// struct of methods (this struct) for it to call instead of a structure of
// function pointers which could be defined directly in the Rust code.
struct Functions {
  void (*list_append_none_fn)(List&);
  void (*list_append_bool_fn)(List&, bool);
  void (*list_append_i32_fn)(List&, int32_t);
  void (*list_append_f64_fn)(List&, double);
  void (*list_append_str_fn)(List&, rust::Str);
  // The returned List reference is given to the visitor functions as
  // an argument for all nodes visited in this list.
  List& (*list_append_list_fn)(List&);
  // The returned Dict reference is given to the visitor functions as
  // an argument for all nodes visited in this dict.
  Dict& (*list_append_dict_fn)(List&);

  void (*dict_set_none_fn)(Dict&, rust::Str);
  void (*dict_set_bool_fn)(Dict&, rust::Str, bool);
  void (*dict_set_i32_fn)(Dict&, rust::Str, int32_t);
  void (*dict_set_f64_fn)(Dict&, rust::Str, double);
  void (*dict_set_str_fn)(Dict&, rust::Str, rust::Str);
  // The returned List reference is given to the visitor functions as
  // an argument for all nodes visited in this list.
  List& (*dict_set_list_fn)(Dict&, rust::Str);
  // The returned Dict reference is given to the visitor functions as
  // an argument for all nodes visited in this dict.
  Dict& (*dict_set_dict_fn)(Dict&, rust::Str);

  void list_append_none(List& c) const {
    list_append_none_fn(c);
  }
  void list_append_bool(List& c, bool val) const {
    list_append_bool_fn(c, val);
  }
  void list_append_i32(List& c, int32_t val) const {
    list_append_i32_fn(c,val);
  }
  void list_append_f64(List& c, double val) const {
    list_append_f64_fn(c, val);
  }
  void list_append_str(List& c, rust::Str val) const {
    list_append_str_fn(c, val);
  }
  List& list_append_list(List& c) const {
    return list_append_list_fn(c);
  }
  Dict& list_append_dict(List& c) const {
    return list_append_dict_fn(c);
  }

  void dict_set_none(Dict& c, rust::Str key) const {
    dict_set_none_fn(c, key);
  }
  void dict_set_bool(Dict& c, rust::Str key, bool val) const {
    dict_set_bool_fn(c, key, val);
  }
  void dict_set_i32(Dict& c, rust::Str key, int32_t val) const {
    dict_set_i32_fn(c, key,val);
  }
  void dict_set_f64(Dict& c, rust::Str key, double val) const {
    dict_set_f64_fn(c, key, val);
  }
  void dict_set_str(Dict& c, rust::Str key, rust::Str val) const {
    dict_set_str_fn(c, key, val);
  }
  List& dict_set_list(Dict& c, rust::Str key) const {
    return dict_set_list_fn(c, key);
  }
  Dict& dict_set_dict(Dict& c, rust::Str key) const {
    return dict_set_dict_fn(c, key);
  }
};

}  // namespace serde_json_lenient

#endif  // THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_
