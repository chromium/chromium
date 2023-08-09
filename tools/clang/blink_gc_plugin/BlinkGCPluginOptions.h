// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_OPTIONS_H_
#define TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_OPTIONS_H_

#include <set>
#include <string>
#include <vector>

struct BlinkGCPluginOptions {
  bool dump_graph = false;

  // Member<T> fields are only permitted in managed classes,
  // something CheckFieldsVisitor verifies, issuing errors if
  // found in unmanaged classes. WeakMember<T> should be treated
  // the exact same, but CheckFieldsVisitor was missing the case
  // for handling the weak member variant until crbug.com/724418.
  //
  // We've default-enabled the checking for those also now, but do
  // offer an opt-out option should enabling the check lead to
  // unexpected (but wanted, really) compilation errors while
  // rolling out an updated GC plugin version.
  //
  // TODO(sof): remove this option once safely rolled out.
  bool enable_weak_members_in_unmanaged_classes = false;

  // Persistent<T> fields are not allowed in garbage collected classes to avoid
  // memory leaks. Enabling this flag allows the plugin to check also for
  // Persistent<T> in types held by unique_ptr in garbage collected classes. The
  // guideline for this check is that a Persistent<T> should never be kept alive
  // by a garbage collected class, which unique_ptr clearly conveys.
  //
  // This check is disabled by default since there are currently non-ignored
  // violations of this rule in the code base, leading to compilation failures.
  // TODO(chromium:1283867): Enable this checks once all violations are handled.
  bool enable_persistent_in_unique_ptr_check = false;

  // On stack references to garbage collected objects should use raw pointers.
  // Although using Members/WeakMembers on stack is not strictly incorrect, it
  // is redundant and incurs additional costs that can mount up and become
  // significant. Enabling this flag lets the plugin to check for instances of
  // using Member/WeakMember on stack. These would include variable
  // declarations, method arguments and return types.
  //
  // This check is disabled by default since there currently are violations
  // of this rule in the code base, leading to compilation failures.
  // TODO(chromium:1283720): Enable this checks once all violations are handled.
  bool enable_members_on_stack_check = false;

  // Checks that any inlined classes (ones that could be a value-type of heap
  // containers) don't have extra padding potentially introduced by Member (e.g
  // due to pointer compression).
  bool enable_extra_padding_check = false;

  // Enables checking for `mojo::Associated{Remote,Receiver}` in the forbidden
  // fields checker.
  bool forbid_associated_remote_receiver = false;

  // Enables checks for GCed objects, Members, and pointers or references to
  // GCed objects and in stl and WTF collections.
  bool enable_off_heap_collections_of_gced_check = false;
  bool enable_off_heap_collections_of_gced_check_pdfium = false;

  std::set<std::string> ignored_classes;
  std::set<std::string> checked_namespaces;
  std::vector<std::string> checked_directories;
  std::vector<std::string> ignored_directories;
};

#endif  // TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_OPTIONS_H_
