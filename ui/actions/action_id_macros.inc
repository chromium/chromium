// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following macros are used to declare both the action id enumerations and
// the stringized names of the enumeration elements for use in persisting the
// actions. To stringize the element names, define STRINGIZE_ACTION_IDS prior to
// including this file. This file is intended to be included just before and
// just after the enumeration or string array declarations.

#if !defined(ACTION_ID_MACROS_DEFINED)
#define ACTION_ID_MACROS_DEFINED
#if defined(STRINGIZE_ACTION_IDS)
// Convert first token to string, throw away the rest.
#define D1(enum_name) #enum_name
#define D2(enum_name, enum_scope) #enum_name
#define D3(enum_name, enum_value, enum_scope) #enum_name
#elif defined(MAP_ACTION_IDS_TO_STRINGS)
// Map first token from enum to string, throw away the rest.
#define D1(enum_name) \
  { enum_name, #enum_name }
#define D2(enum_name, enum_scope) \
  { enum_scope::enum_name, #enum_name }
#define D3(enum_name, enum_value, enum_scope) \
  { enum_scope::enum_name, #enum_name }
#elif defined(MAP_STRING_TO_ACTION_IDS)
// Map first token from string to enum, throw away the rest.
#define D1(enum_name) \
  { #enum_name, enum_name }
#define D2(enum_name, enum_scope) \
  { #enum_name, enum_scope::enum_name }
#define D3(enum_name, enum_value, enum_scope) \
  { #enum_name, enum_scope::enum_name }
#else  // defined(STRINGIZE_ACTION_IDS)
// Declare enum with optional assigned value.
#define D1(enum_name) enum_name
#define D2(enum_name, enum_scope) enum_name
#define D3(enum_name, enum_value, enum_scope) enum_name = enum_value
#endif  // defined(STRINGIZE_ACTION_IDS)
// Select which token in the declaration is the assigned value.
// Use first and optional third token, ignore optional second.
#define E1(enum_name) D1(enum_name)
#define E2(enum_name, old_enum_name) D1(enum_name)
#define E3(enum_name, old_enum_name, enum_scope) D2(enum_name, enum_scope)
#define E4(enum_name, old_enum_name, enum_value, enum_scope) \
  D3(enum_name, enum_value, enum_scope)
#define GET_E(_1, _2, _3, _4, macro_name, ...) macro_name
#define E(...) GET_E(__VA_ARGS__, E4, E3, E2, E1)(__VA_ARGS__),
#else  // !defined(ACTION_ID_MACROS_DEFINED)
#undef D1
#undef D2
#undef D3
#undef E1
#undef E2
#undef E3
#undef E4
#undef GET_E
#undef E
#undef ACTION_ID_MACROS_DEFINED
#endif  // !defined(ACTION_ID_MACROS_DEFINED)
