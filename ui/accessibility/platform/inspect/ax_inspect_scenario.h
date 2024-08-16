// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace base {
class FilePath;
}

namespace ui {

class AXScriptInstruction;

// Describes the test execution flow, which is parsed from a sequence
// of testing directives (instructions). The testing directives are typically
// found in a testing file in the comment section. For example, such section
// in a dump_tree HTML test file will instruct to wait for 'bananas' text in
// a document and then dump an accessible tree which includes aria-live property
// on all platforms:
// <!--
// @WAIT-FOR:bananas
// @MAC-ALLOW:AXARIALive
// @WIN-ALLOW:live*
// @UIA-WIN-ALLOW:LiveSetting*
// @BLINK-ALLOW:live*
// @BLINK-ALLOW:container*
// @AURALINUX-ALLOW:live*
// -->
class COMPONENT_EXPORT(AX_PLATFORM) AXInspectScenario {
 public:
  explicit AXInspectScenario(
      const std::vector<AXPropertyFilter>& default_filters = {});
  AXInspectScenario(AXInspectScenario&&);
  ~AXInspectScenario();

  AXInspectScenario& operator=(AXInspectScenario&&);

  // Parses a given testing scenario.
  // @directive_prefix  platform dependent directive prefix, for example,
  //                    @MAC- is used for filter directives on Mac
  // @lines             lines containing directives as a text
  // @default_filters   set of default filters, a special type of directives,
  //                    defining which property gets (or not) into the output,
  //                    useful to not make each test to specify common filters
  //                    all over
  static AXInspectScenario From(
      const std::string& directive_prefix,
      const std::vector<std::string>& lines,
      const std::vector<AXPropertyFilter>& default_filters = {});

  // Parses a given testing scenario.
  // @directive_prefix  platform dependent directive prefix, for example,
  //                    @MAC- is used for filter directives on Mac
  // @scenario_path     Path can be a plain file or HTML file containing a
  //                    scenario in a <!-- --> comment section.
  // @default_filters   set of default filters, a special type of directives,
  //                    defining which property gets (or not) into the output,
  //                    useful to not make each test to specify common filters
  //                    all over
  static std::optional<AXInspectScenario> From(
      const std::string& directive_prefix,
      const base::FilePath& scenario_path,
      const std::vector<AXPropertyFilter>& default_filters = {});

  // A list of URLs of resources that are never expected to load. For example,
  // a broken image url, which otherwise would make a test failing.
  std::vector<std::string> no_load_expected;

  // A list of strings must be present in the formatted tree before the test
  // starts
  std::vector<std::string> wait_for;

  // A list of string indicating an element the default accessible action
  // should be performed at before the test starts.
  std::vector<std::string> default_action_on;

  // A list of JavaScripts functions to be executed consequently. Function
  // may return a value, which has to be present in a formatter tree before
  // the next function evaluated.
  std::vector<std::string> execute;

  // A list of property filters which defines generated output of a formatted
  // tree.
  std::vector<AXPropertyFilter> property_filters;

  // The node filters indicating subtrees that should be not included into
  // a formatted tree.
  std::vector<AXNodeFilter> node_filters;

  // Scripting instructions.
  std::vector<AXScriptInstruction> script_instructions;

 private:
  enum Directive {
    // No directive.
    kNone,

    // Instructs to not wait for document load for url defined by the
    // directive.
    kNoLoadExpected,

    // Delays a test unitl a string defined by the directive is present
    // in the dump.
    kWaitFor,

    // Delays a test until a string returned by a script defined by the
    // directive is present in the dump.
    kExecuteAndWaitFor,

    // Invokes default action on an accessible object defined by the
    // directive.
    kDefaultActionOn,

    // Property filter directives, see AXPropertyFilter.
    kPropertyFilterAllow,
    kPropertyFilterAllowEmpty,
    kPropertyFilterDeny,

    // Scripting instruction.
    kScript,

    // Node filter directives, see AXNodeFilter.
    kNodeFilter,
  };

  // Parses directives from the given line.
  static Directive ParseDirective(const std::string& directive_prefix,
                                  std::string_view directive);

  // Adds a given directive into a scenario.
  void ProcessDirective(Directive directive, std::string_view value);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_
