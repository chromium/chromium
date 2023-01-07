// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following macros are used to declare a map from color id enum to the
// stringized name of the color id enum for use in extracting the color id enum
// in string format. This file is intended to be included just before and just
// after the declaration of the map.

#if !defined(COLOR_ID_FLAT_MAP_MACROS_DEFINED)
#define COLOR_ID_FLAT_MAP_MACROS_DEFINED

#define E1(enum_name) \
  { enum_name, #enum_name }
#define E2(enum_name, old_enum_name) \
  { enum_name, #enum_name }
#define E3(enum_name, old_enum_name, enum_value) \
  { enum_name, #enum_name }
#define E_CPONLY(...) E(__VA_ARGS__)
#define GET_E(_1, _2, _3, macro_name, ...) macro_name
#define E(...) GET_E(__VA_ARGS__, E3, E2, E1)(__VA_ARGS__),

#else
#undef E1
#undef E2
#undef E3
#undef E_CPONLY
#undef GET_E
#undef E
#endif
