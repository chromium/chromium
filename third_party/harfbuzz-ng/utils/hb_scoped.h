// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_HARFBUZZ_NG_UTILS_HB_SCOPED_H_
#define THIRD_PARTY_HARFBUZZ_NG_UTILS_HB_SCOPED_H_

// clang-format off
#include <hb.h>
#include <hb-subset.h>
// clang-format on

#include <memory>
#include <type_traits>

template <typename T>
struct always_false : std::false_type {};

template <class T>
struct HbSpecializedDeleter {
  inline void operator()(T* obj) {
    static_assert(always_false<T>::value,
                  "HbScoped is only allowed for HarfBuzz types that have a "
                  "deleter specialization.");
  }
};

// Defines a scoped pointer type HbScoped based on std::unique_ptr, using the
// corresponsing HarfBuzz destructors to commonly used public HarfBuzz types.
// The interface of HbScoped is the same as that of std::unique_ptr.
//
//  void MyFunction() {
//    HbScoped<hb_blob_t> scoped_harfbuzz_blob(
//        hb_blob_create(mydata, mylength));
//
//    DoSomethingWithBlob(scoped_harfbuzz_blob.get());
//  }
//
// When |scoped_harfbuzz_buffer| goes out of scope, hb_blob_destroy() is called
// for the hb_blob_t* created from hb_blob_create().
template <class T>
using HbScoped = std::unique_ptr<T, HbSpecializedDeleter<T>>;

#define SPECIALIZED_DELETER_FOR_HARFBUZZ_TYPE(TYPE, DESTRUCTOR) \
  template <>                                                   \
  struct HbSpecializedDeleter<TYPE> {                           \
    inline void operator()(TYPE* obj) { DESTRUCTOR(obj); }      \
  };

#define HB_TYPE_DESTRUCTOR_PAIRS_REPEAT(F) \
  F(hb_blob_t, hb_blob_destroy)            \
  F(hb_buffer_t, hb_buffer_destroy)        \
  F(hb_face_t, hb_face_destroy)            \
  F(hb_font_t, hb_font_destroy)            \
  F(hb_set_t, hb_set_destroy)              \
  F(hb_subset_input_t, hb_subset_input_destroy)

HB_TYPE_DESTRUCTOR_PAIRS_REPEAT(SPECIALIZED_DELETER_FOR_HARFBUZZ_TYPE)

#endif  // THIRD_PARTY_HARFBUZZ_NG_UTILS_HB_SCOPED_H_
