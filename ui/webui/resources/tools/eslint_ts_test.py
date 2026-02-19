#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import eslint_ts
import os
import tempfile
import shutil
import unittest

_HERE_DIR = os.path.dirname(__file__)


class EslintTsTest(unittest.TestCase):

  _in_folder = os.path.join(_HERE_DIR, "tests", "eslint_ts")

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=self._in_folder)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, "r", encoding="utf-8") as file:
      return file.read()

  def _run_test(self, in_files, enable_web_component_missing_deps=False):
    config_base = os.path.join(_HERE_DIR, "eslint_ts.config_base.mjs")
    tsconfig = os.path.join(self._in_folder, "tsconfig.json")

    args = [
        "--in_folder",
        self._in_folder,
        "--out_folder",
        self._out_dir,
        "--config_base",
        os.path.relpath(config_base, self._out_dir).replace(os.sep, '/'),
        "--tsconfig",
        os.path.relpath(tsconfig, self._out_dir).replace(os.sep, '/'),
        "--in_files",
        *in_files,
    ]

    if enable_web_component_missing_deps:
      args += ['--enable_web_component_missing_deps']

    eslint_ts.main(args)

  def testSuccess(self):
    self._run_test(["no_violations.ts"])
    actual_contents = self._read_file(
        os.path.join(self._out_dir, "eslint.config.mjs"))
    expected_contents = self._read_file(
        os.path.join(self._in_folder, "eslint_expected.config.mjs"))
    self.assertMultiLineEqual(expected_contents, actual_contents)

  def testError(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_violations.ts"])

    # Expected ESLint rule violation that should be part of the error output.
    _EXPECTED_STRING = "@typescript-eslint/require-await"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))

  def testWebUiEslintPlugin_LitPropertyAccessor(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_webui_plugin_lit_violations.ts"])

    _EXPECTED_STRING = "@webui-eslint/lit-property-accessor"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))
    errors = [
        "Missing 'accessor' keyword when declaring Lit reactive property 'prop2' in class 'SomeElement'",
        "Unnecessary 'accessor' keyword when declaring regular (non Lit reactive) property 'prop3' in class 'SomeElement'",
        "Missing 'accessor' keyword when declaring Lit reactive property 'prop1' in class 'SomeOtherElement'",
        "Unnecessary 'accessor' keyword when declaring regular (non Lit reactive) property 'prop4' in class 'SomeOtherElement'",
    ]
    for e in errors:
      self.assertTrue(e in str(context.exception))

  def testWebUiEslintPlugin_PolymerPropertyDeclare(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_webui_plugin_polymer_violations.ts"])

    _EXPECTED_STRING = "@webui-eslint/polymer-property-declare"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))
    errors = [
        "Missing 'declare' keyword when declaring Polymer property 'prop2' in class 'SomeElement'",
        "Unnecessary 'declare' keyword when declaring regular (non Polymer) property 'prop3' in class 'SomeElement'",
        "Missing 'declare' keyword when declaring Polymer property 'prop1' in class 'SomeOtherElement'",
        "Unnecessary 'declare' keyword when declaring regular (non Polymer) property 'prop4' in class 'SomeOtherElement'",
    ]
    for e in errors:
      self.assertTrue(e in str(context.exception))

  def testWebUiEslintPlugin_PolymerPropertyClassMember(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(
          ["with_webui_plugin_polymer_property_class_member_violations.ts"])

    _EXPECTED_STRING = "@webui-eslint/polymer-property-class-member"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))
    errors = [
        "Polymer property 'prop3' in class 'SomeElement' must also be declared as a class member",
        "Polymer property 'prop1' in class 'SomeOtherElement' must also be declared as a class member",
    ]
    for e in errors:
      self.assertTrue(e in str(context.exception))

  def testWebUiEslintPlugin_WebComponentMissingDeps(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test([
          "with_webui_plugin_web_component_missing_deps_violations.html.ts",
          "with_webui_plugin_web_component_missing_deps_violations_bar.html.ts",
          "with_webui_plugin_web_component_missing_deps_violations_foo.html.ts",
      ],
                     enable_web_component_missing_deps=True)

    _EXPECTED_STRING = "@webui-eslint/web-component-missing-deps"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))

    _EXPECTED_ERROR = "Missing explicit import statement for '%(tagName)s' in the class definition file 'with_webui_plugin_web_component_missing_deps_violations.ts' or in 'lazy_load.ts'"

    # The following strings *should* appear in the error output since the
    # referenced dependencies are imported.
    errors = [
        _EXPECTED_ERROR % {
            'tagName': 'cr-icon-button'
        },
        _EXPECTED_ERROR % {
            'tagName': 'other-button1'
        },
        _EXPECTED_ERROR % {
            'tagName': 'other-button2'
        },
        # Testing missing dependencies check correctly identifies missing imports
        # in helper html.ts files.
        _EXPECTED_ERROR % {
            'tagName': 'cr-input'
        },
        _EXPECTED_ERROR % {
            'tagName': 'my-bar'
        },
    ]
    for e in errors:
      self.assertTrue(
          e in str(context.exception), f'Didn\'t find expected error: {e}')

    # The following strings *should not* appear in the error output since the
    # referenced dependencies are imported.
    non_errors = [
        # Imported via cr_expand_button.js (testing exact matching between tag
        # name and corresponding import).
        _EXPECTED_ERROR % {
            'tagName': 'cr-expand-button'
        },
        # Imported via other_button.js (testing fuzzy matching between tag name
        # and corresponding import).
        _EXPECTED_ERROR % {
            'tagName': 'some-other-button'
        },
        # Imported via iron-list.js (testing special case for
        # third_party/polymer elements/ which use "-" instead of "_".
        _EXPECTED_ERROR % {
            'tagName': 'iron-list'
        },
        # Imported via lazy_load.js (testing lazy loading detection).
        _EXPECTED_ERROR % {
            'tagName': 'foo-bar'
        },
        # Testing dependencies correctly imported for helper template files are
        # not reported as missing.
        _EXPECTED_ERROR % {
            'tagName': 'cr-textarea'
        },
        _EXPECTED_ERROR % {
            'tagName': 'my-foo'
        },
    ]
    for e in non_errors:
      self.assertFalse(
          e in str(context.exception), f'Found unexpected error: {e}')

  def testWebUiEslintPlugin_InlineEventHandler(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(
          ["with_webui_plugin_inline_event_handler_violations.html.ts"])

    _EXPECTED_STRING = "@webui-eslint/inline-event-handler"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))

    _EXPECTED_ERROR = "Inline event handler for event '%(eventName)s' found on element '%(tagName)s'. Do not use inline arrow functions in templates"

    # The following strings *should* appear in the error output since the events
    # have inline lambda event handlers.
    errors = [
        _EXPECTED_ERROR % {
            'eventName': 'click',
            'tagName': 'cr-icon-button',
        },
        _EXPECTED_ERROR % {
            'eventName': 'input',
            'tagName': 'cr-input',
        },
        _EXPECTED_ERROR % {
            'eventName': 'focus',
            'tagName': 'cr-button',
        },
        _EXPECTED_ERROR % {
            'eventName': 'animationend',
            'tagName': 'div',
        },
    ]
    for e in errors:
      self.assertTrue(
          e in str(context.exception), f'Didn\'t find expected error: {e}')

    # The following strings *should not* appear in the error output since the
    # event handlers are correctly bound to protected methods.
    non_errors = [
        _EXPECTED_ERROR % {
            'eventName': 'change',
            'tagName': 'select',
        },
        _EXPECTED_ERROR % {
            'eventName': 'blur',
            'tagName': 'cr-button',
        },
    ]
    for e in non_errors:
      self.assertFalse(
          e in str(context.exception), f'Found unexpected error: {e}')

  def testWebUiEslintPlugin_LitElementStructure(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_webui_plugin_lit_element_structure_violations.ts"])

    # Expected ESLint rule violation that should be part of the error output.
    _EXPECTED_STRING = "@webui-eslint/lit-element-structure"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))

    _EXPECTED_INCONSISTENT_METHOD_DEFINITION_ORDER_ERROR = "Inconsistent method definition order in class %(className)s. Expected %(expectedOrder)s, found %(actualOrder)s"
    _EXPECTED_INCORRECT_CLASS_NAME_ERROR = 'CrLitElement subclass %(className)s should end with the \'Element\' suffix'
    _EXPECTED_INCORRECT_DOLLAR_SIGN_NOTATION_ERROR = 'Use camelCase instead of dash-case for DOM ids, change this.$[\'%(dashCaseName)s\'] to this.$.%(camelCaseName)s'
    _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR = "Missing customElements.define(%(className)s.is, %(className)s) call"
    _EXPECTED_MISSING_STATIC_GET_IS_ERROR = "Missing 'static get is() {...}' for web component class %(className)s"
    _EXPECTED_MISSING_SUPER_CALLS_ERROR = "Missing superclass calls for lifecycle method(s) %(lifecycleMethods)s in class %(className)s"
    _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR = "Tag/class name pair registration to HTMLElementTagNameMap interface missing for %(domName)s ↔ %(className)s"
    _EXPECTED_USE_FIRE_HELPER_ERROR = "Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...))."
    _EXPECTED_USE_FIRE_HELPER_WITH_EVENT_NAME_ERROR = "Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...)), for event \'%(eventName)s\'"

    super_call_required_methods = [
        'connectedCallback', 'disconnectedCallback', 'willUpdate', 'updated'
    ]

    # The following strings *should* appear in the error output.
    errors = [
        # Case 1.1
        _EXPECTED_MISSING_STATIC_GET_IS_ERROR % {
            'className': 'TestError1Element',
        },
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestError1Element',
        },
        # Case 1.2
        _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR % {
            'className': 'TestError2Element',
            'domName': 'test-error2'
        },
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestError2Element',
        },
        # Case 1.3
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestError3Element',
        },
        # Case 1.4
        _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR % {
            'className': 'TestError4Element',
            'domName': 'test-error4',
        },
        # Case 1.5
        _EXPECTED_MISSING_SUPER_CALLS_ERROR % {
            'className': 'TestError5Element',
            'lifecycleMethods': ', '.join(super_call_required_methods),
        },
        # Case 1.6
        _EXPECTED_INCONSISTENT_METHOD_DEFINITION_ORDER_ERROR % {
            'className':
                'TestError6Element',
            'expectedOrder':
                '[is, styles, render, properties, constructor, connectedCallback, disconnectedCallback, willUpdate, firstUpdated, updated]',
            'actualOrder':
                '[render, styles, is, properties, disconnectedCallback, connectedCallback, constructor, willUpdate, updated, firstUpdated]',
        },
        _EXPECTED_USE_FIRE_HELPER_WITH_EVENT_NAME_ERROR % {
            'eventName': 'foo1-updated',
        },
        _EXPECTED_USE_FIRE_HELPER_WITH_EVENT_NAME_ERROR % {
            'eventName': 'foo2-updated',
        },
        _EXPECTED_USE_FIRE_HELPER_ERROR,
        _EXPECTED_INCORRECT_DOLLAR_SIGN_NOTATION_ERROR % {
            'dashCaseName': 'hello-button',
            'camelCaseName': 'helloButton',
        },
        # Case 1.7
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestError7ElementFoo',
        },
        # Case 1.8
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestError8ElementFoo',
        },
        # Case 1.9
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestError9ElementFoo',
        },
        # Case 1.10
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestError10ElementFoo',
        },
    ]
    for e in errors:
      self.assertTrue(
          e in str(context.exception), f'Didn\'t find expected error: {e}')

    # The following strings *should not* appear in the error output.
    non_errors = [
        # Case 2.1
        _EXPECTED_MISSING_STATIC_GET_IS_ERROR % {
            'className': 'TestNoError1Element'
        },
        _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR % {
            'className': 'TestNoError1Element',
            'domName': 'test-no-error1',
        },
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestNoError1Element'
        },
        _EXPECTED_MISSING_SUPER_CALLS_ERROR % {
            'className': 'TestNoError1Element',
            'lifecycleMethods': ', '.join(super_call_required_methods)
        },
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestNoError1Element',
        },
        # Case 2.2
        _EXPECTED_MISSING_STATIC_GET_IS_ERROR % {
            'className': 'TestNoError2Element'
        },
        _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR % {
            'className': 'TestNoError2Element',
            'domName': 'test-no-error2',
        },
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestNoError2Element'
        },
        _EXPECTED_MISSING_SUPER_CALLS_ERROR % {
            'className': 'TestNoError2Element',
            'lifecycleMethods': ', '.join(super_call_required_methods),
        },
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestNoError2Element',
        },
        # Case 2.3
        _EXPECTED_MISSING_STATIC_GET_IS_ERROR % {
            'className': 'TestNoError3Element',
        },
        _EXPECTED_MISSING_TAG_NAME_REGISTRATION_ERROR % {
            'className': 'TestNoError3Element',
            'domName': 'test-no-error3',
        },
        _EXPECTED_MISSING_CUSTOM_ELEMENTS_DEFINE_ERROR % {
            'className': 'TestNoError3Element',
        },
        _EXPECTED_MISSING_SUPER_CALLS_ERROR % {
            'className': 'TestNoError3Element',
            'lifecycleMethods': ', '.join(super_call_required_methods)
        },
        _EXPECTED_INCONSISTENT_METHOD_DEFINITION_ORDER_ERROR % {
            'className':
                'TestNoError3Element',
            'expectedOrder':
                '[is, styles, render, properties, constructor, connectedCallback, disconnectedCallback, willUpdate, firstUpdated, updated]',
            'actualOrder':
                '',
        },
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestNoError3Element',
        },
        _EXPECTED_USE_FIRE_HELPER_WITH_EVENT_NAME_ERROR % {
            'eventName': 'bar-updated',
        },
        _EXPECTED_INCORRECT_DOLLAR_SIGN_NOTATION_ERROR % {
            'dashCaseName': 'hello-other-button',
            'camelCaseName': 'helloOtherButton',
        },
        # Case 2.4
        _EXPECTED_INCORRECT_CLASS_NAME_ERROR % {
            'className': 'TestNoError4Element',
        },
    ]
    for e in non_errors:
      self.assertFalse(
          e in str(context.exception), f'Found unexpected error: {e}')

  def testWebUiEslintPlugin_LitElementTemplateStructure(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test([
          "with_webui_plugin_lit_element_template_structure_violations.html.ts"
      ])

    _EXPECTED_STRING = "@webui-eslint/lit-element-template-structure"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))

    _FOR_STATEMENT_ERROR = "For loop found in getHtml() method. Use Array#map() to render the same HTML for an array of items, and delegate more complex logic to the class definition file"

    _IF_STATEMENT_ERROR = "If statement found in getHtml() method. Use ternary statements for conditional rendering, and delegate more complex logic to the class definition file"

    _FUNCTION_DEFINITION_ERROR = "Extra function definition '%(functionName)s' found in the HTML template file. Complex logic should be delegated to the class definition file. Standalone/separate chunks of templates may need a dedicated custom element"

    _VARIABLE_DECLARATION_ERROR = "Local (const/let) variable '%(variableName)s' found in the HTML template file. Logic should be delegated to the class definition file"

    # The following strings *should* appear in the error output.
    errors = [
        _FOR_STATEMENT_ERROR,
        _IF_STATEMENT_ERROR,
        _FUNCTION_DEFINITION_ERROR % {
            'functionName': 'computeProgress'
        },
        _FUNCTION_DEFINITION_ERROR % {
            'functionName': 'getButtonHtml'
        },
        _FUNCTION_DEFINITION_ERROR % {
            'functionName': 'getSpinnerDiv'
        },
        _VARIABLE_DECLARATION_ERROR % {
            'variableName': 'INPUT_MAX_LENGTH'
        },
        _VARIABLE_DECLARATION_ERROR % {
            'variableName': 'input'
        },
        _VARIABLE_DECLARATION_ERROR % {
            'variableName': 'titleClass'
        },
        _VARIABLE_DECLARATION_ERROR % {
            'variableName': 'messagesToRender'
        },
    ]
    for e in errors:
      self.assertTrue(
          e in str(context.exception), f'Didn\'t find expected error: {e}')

    # The following strings *should not* appear in the error output.
    non_errors = [
        # getHtml() declaration is allowed.
        _FUNCTION_DEFINITION_ERROR % {
            'functionName': 'getHtml'
        },
    ]
    for e in non_errors:
      self.assertFalse(
          e in str(context.exception), f'Found unexpected error: {e}')


if __name__ == "__main__":
  unittest.main()
