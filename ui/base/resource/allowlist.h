// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_ALLOWLIST_H_
#define UI_BASE_RESOURCE_ALLOWLIST_H_

namespace ui {

// The purpose of this function template is to support resource allowlisting,
// which is a mechanism for filtering out unused resources from the .pak files
// that we ship. The grit program generates macros that look like this:
//
// #define IDS_FOO (::ui::AllowlistedResource<123>(), 123)
//
// A reference via the macro to the resource causes the function template to be
// instantiated with the given resource identifier and (because of the used
// attribute) emitted into the object file. After the program is linked, the
// used resource identifiers can be found by searching the debug info in the
// linker's output file for instantiations of the template. The implementation
// of this lives in tools/resources/generate_resource_allowlist.py.
//
// In ELF linkers, the function definitions themselves are dropped from the
// final output file because of --gc-sections (debug info is immune to
// --gc-sections). In COFF and Mach-O, the used attribute also causes the linker
// to preserve the function definitions in the output file, but that shouldn't
// affect binary size significantly because all of the function definitions are
// the same, which means that they can be ICF'd together (or, more likely, ICF'd
// with an existing empty function).
//
// Because the AllowlistedResource function is constexpr, the definitions of the
// resource macros are integral constant expressions, which means that they can
// appear in places like case statements in switches.
template <int ResourceId>
__attribute__((used)) constexpr void AllowlistedResource() {}

}  // namespace ui

#endif  // UI_BASE_RESOURCE_ALLOWLIST_H_
