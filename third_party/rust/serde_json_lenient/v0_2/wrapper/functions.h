// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_
#define THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_

#include <stdint.h>

#include "third_party/rust/cxx/v1/cxx.h"

namespace serde_json_lenient {

// An opaque pointer provided by the caller to decode_json() which is passed
// through to the visitor functions on `Functions`.
struct ContextPointer;

// C++ functions that provide the functionality for each stop by the JSON
// deserializer in order to construct the output in C++ from the JSON
// deserialization.
//
// TODO(danakj): CXX does not support function pointers, so we need to use a
// struct of methods (this struct) for it to call instead of a structure of
// function pointers which could be defined directly in the Rust code.
struct Functions {
  void (*list_append_none_fn)(ContextPointer&);
  void (*list_append_bool_fn)(ContextPointer&, bool);
  void (*list_append_i32_fn)(ContextPointer&, int32_t);
  void (*list_append_f64_fn)(ContextPointer&, double);
  void (*list_append_str_fn)(ContextPointer&, rust::Str);
  // The returned ContextPointer reference is given to the visitor functions as
  // an argument for all nodes visited in this list.
  ContextPointer& (*list_append_list_fn)(ContextPointer&, size_t);
  // The returned ContextPointer reference is given to the visitor functions as
  // an argument for all nodes visited in this dict.
  ContextPointer& (*list_append_dict_fn)(ContextPointer&);

  void (*dict_set_none_fn)(ContextPointer&, rust::Str);
  void (*dict_set_bool_fn)(ContextPointer&, rust::Str, bool);
  void (*dict_set_i32_fn)(ContextPointer&, rust::Str, int32_t);
  void (*dict_set_f64_fn)(ContextPointer&, rust::Str, double);
  void (*dict_set_str_fn)(ContextPointer&, rust::Str, rust::Str);
  // The returned ContextPointer reference is given to the visitor functions as
  // an argument for all nodes visited in this list.
  ContextPointer& (*dict_set_list_fn)(ContextPointer&, rust::Str, size_t);
  // The returned ContextPointer reference is given to the visitor functions as
  // an argument for all nodes visited in this dict.
  ContextPointer& (*dict_set_dict_fn)(ContextPointer&, rust::Str);

  void list_append_none(ContextPointer& c) const {
    list_append_none_fn(c);
  }
  void list_append_bool(ContextPointer& c, bool val) const {
    list_append_bool_fn(c, val);
  }
  void list_append_i32(ContextPointer& c, int32_t val) const {
    list_append_i32_fn(c,val);
  }
  void list_append_f64(ContextPointer& c, double val) const {
    list_append_f64_fn(c, val);
  }
  void list_append_str(ContextPointer& c, rust::Str val) const {
    list_append_str_fn(c, val);
  }
  ContextPointer& list_append_list(ContextPointer& c, size_t reserve) const {
    return list_append_list_fn(c, reserve);
  }
  ContextPointer& list_append_dict(ContextPointer& c) const {
    return list_append_dict_fn(c);
  }

  void dict_set_none(ContextPointer& c, rust::Str key) const {
    dict_set_none_fn(c, key);
  }
  void dict_set_bool(ContextPointer& c, rust::Str key, bool val) const {
    dict_set_bool_fn(c, key, val);
  }
  void dict_set_i32(ContextPointer& c, rust::Str key, int32_t val) const {
    dict_set_i32_fn(c, key,val);
  }
  void dict_set_f64(ContextPointer& c, rust::Str key, double val) const {
    dict_set_f64_fn(c, key, val);
  }
  void dict_set_str(ContextPointer& c, rust::Str key, rust::Str val) const {
    dict_set_str_fn(c, key, val);
  }
  ContextPointer& dict_set_list(ContextPointer& c, rust::Str key,
                                size_t reserve) const {
    return dict_set_list_fn(c, key, reserve);
  }
  ContextPointer& dict_set_dict(ContextPointer& c, rust::Str key) const {
    return dict_set_dict_fn(c, key);
  }
};

}  // namespace serde_json_lenient

#endif  // THIRD_PARTY_RUST_SERDE_JSON_LENIENT_V0_2_WRAPPER_SERDE_JSON_LENIENT_H_
