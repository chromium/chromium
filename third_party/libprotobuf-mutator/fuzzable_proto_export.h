// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBPROTOBUF_MUTATOR_FUZZABLE_PROTO_EXPORT_H_
#define THIRD_PARTY_LIBPROTOBUF_MUTATOR_FUZZABLE_PROTO_EXPORT_H_

// Used to annotate symbols which are exported by the proto component.
// See base/component_export.h for more details.
#define FUZZABLE_PROTO_EXPORT(component)                    \
  FUZZABLE_PROTO_MACRO_CONDITIONAL_(IS_##component##_IMPL, \
                               FUZZABLE_PROTO_EXPORT_ANNOTATION, \
                               FUZZABLE_PROTO_IMPORT_ANNOTATION)

#if defined(COMPONENT_BUILD)
#if defined(WIN32)
#define FUZZABLE_PROTO_EXPORT_ANNOTATION __declspec(dllexport)
#define FUZZABLE_PROTO_IMPORT_ANNOTATION __declspec(dllimport)
#else  // defined(WIN32)
#define FUZZABLE_PROTO_EXPORT_ANNOTATION __attribute__((visibility("default")))
#define FUZZABLE_PROTO_IMPORT_ANNOTATION __attribute__((visibility("default")))
#endif  // defined(WIN32)
#else   // defined(COMPONENT_BUILD)
#define FUZZABLE_PROTO_EXPORT_ANNOTATION
#define FUZZABLE_PROTO_IMPORT_ANNOTATION
#endif  // defined(COMPONENT_BUILD)

// Helper macros for conditional expansion.
#define FUZZABLE_PROTO_MACRO_CONDITIONAL_(condition, consequent, alternate) \
  FUZZABLE_PROTO_MACRO_SELECT_THIRD_ARGUMENT_(                              \
      FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_(condition), consequent, alternate)

#define FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_(...) \
  FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_IMPL_(__VA_ARGS__, )
#define FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_IMPL_(x, ...) \
  FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_##x##_
#define FUZZABLE_PROTO_MACRO_CONDITIONAL_COMMA_1_ ,

#define FUZZABLE_PROTO_MACRO_SELECT_THIRD_ARGUMENT_(...) \
  FUZZABLE_PROTO_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(__VA_ARGS__)
#define FUZZABLE_PROTO_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(a, b, c, ...) c

#endif  // THIRD_PARTY_LIBPROTOBUF_MUTATOR_FUZZABLE_PROTO_EXPORT_H_
