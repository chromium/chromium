#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os
import tempfile
import unittest

from compiler import Compiler
from processor import FileCache, Processor


_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(_SCRIPT_DIR, os.pardir, os.pardir)
_RESOURCES_DIR = os.path.join(_SRC_DIR, "ui", "webui", "resources", "js")
_ASSERT_JS = os.path.join(_RESOURCES_DIR, "assert.js")
_CR_JS = os.path.join(_RESOURCES_DIR, "cr.js")
_CR_UI_JS = os.path.join(_RESOURCES_DIR, "cr", "ui.js")
_PROMISE_RESOLVER_JS = os.path.join(_RESOURCES_DIR, "promise_resolver.js")
_CHROME_EXTERNS = os.path.join(_SRC_DIR, "third_party", "closure_compiler",
                               "externs", "chrome.js")
_CHROME_SEND_EXTERNS = os.path.join(_SRC_DIR, "third_party", "closure_compiler",
                                    "externs", "chrome_send.js")
_CLOSURE_ARGS_GNI = os.path.join(_SCRIPT_DIR, "closure_args.gni")
_CLOSURE_ARGS = imp.load_source('closure_gni', _CLOSURE_ARGS_GNI)
_COMMON_CLOSURE_ARGS = _CLOSURE_ARGS.default_closure_args + \
                       _CLOSURE_ARGS.default_disabled_closure_args

class CompilerTest(unittest.TestCase):
  _ASSERT_DEFINITION = Processor(_ASSERT_JS).contents
  _PROMISE_RESOLVER_DEFINITION = (_ASSERT_DEFINITION +
                                  Processor(_PROMISE_RESOLVER_JS).contents)
  _CR_DEFINE_DEFINITION = (_PROMISE_RESOLVER_DEFINITION +
                           Processor(_CR_JS).contents)
  _CR_UI_DECORATE_DEFINITION = Processor(_CR_UI_JS).contents

  def setUp(self):
    self._compiler = Compiler()
    self._tmp_files = []

  def tearDown(self):
    for file in self._tmp_files:
      if os.path.exists(file):
        os.remove(file)

  def _runCompiler(self, source_code, needs_output, closure_args=None):
    file_path = "/script.js"
    FileCache._cache[file_path] = source_code
    out_file = self._createOutFiles()
    args = _COMMON_CLOSURE_ARGS + (closure_args or [])
    if needs_output and "checks_only" in args:
      args.remove("checks_only")

    sources = [file_path, _CHROME_EXTERNS, _CHROME_SEND_EXTERNS]
    found_errors, stderr = self._compiler.run(sources,
                                              out_file=out_file,
                                              closure_args=args)
    return found_errors, stderr, out_file

  def _runCompilerTestExpectError(self, source_code, expected_error,
                                 closure_args=None):
    _, stderr, out_file = self._runCompiler(
        source_code, needs_output=False, closure_args=closure_args)

    self.assertTrue(expected_error in stderr,
        msg="Expected chunk: \n%s\n\nOutput:\n%s\n" % (
            expected_error, stderr))
    self.assertFalse(os.path.exists(out_file))

  def _runCompilerTestExpectSuccess(self, source_code, expected_output=None,
                                    closure_args=None):
    found_errors, stderr, out_file = self._runCompiler(
        source_code, needs_output=True, closure_args=closure_args)

    self.assertFalse(found_errors,
        msg="Expected success, but got failure\n\nOutput:\n%s\n" % stderr)

    self.assertTrue(os.path.exists(out_file))
    if expected_output:
      with open(out_file, "r") as file:
        self.assertEquals(file.read(), expected_output)

  def _createOutFiles(self):
    out_file = tempfile.NamedTemporaryFile(delete=False)

    self._tmp_files.append(out_file.name)
    return out_file.name

  def testGetInstance(self):
    self._runCompilerTestExpectError("""
var cr = {
  /** @param {!Function} ctor */
  addSingletonGetter: function(ctor) {
    ctor.getInstance = function() {
      return ctor.instance_ || (ctor.instance_ = new ctor());
    };
  }
};

/** @constructor */
function Class() {
  /** @param {number} num */
  this.needsNumber = function(num) {};
}

cr.addSingletonGetter(Class);
Class.getInstance().needsNumber("wrong type");
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of Class.needsNumber does "
        "not match formal parameter")

  def testCrDefineFunctionDefinition(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  function internalName(num) {}

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of a.b.c.needsNumber does "
        "not match formal parameter")

  def testCrDefineFunctionAssignment(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  var internalName = function(num) {};

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of a.b.c.needsNumber does "
        "not match formal parameter")

  def testCrDefineConstructorDefinitionPrototypeMethod(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  function ClassInternalName() {}

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of "
        "a.b.c.ClassExternalName.prototype.method does not match formal "
        "parameter")

  def testCrDefineConstructorAssignmentPrototypeMethod(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  var ClassInternalName = function() {};

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of "
        "a.b.c.ClassExternalName.prototype.method does not match formal "
        "parameter")

  def testCrDefineEnum(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @enum {string} */
  var internalNameForEnum = {key: 'wrong_type'};

  return {
    exportedEnum: internalNameForEnum
  };
});

/** @param {number} num */
function needsNumber(num) {}

needsNumber(a.b.c.exportedEnum.key);
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of needsNumber does not "
        "match formal parameter")

  def testObjectDefineProperty(self):
    self._runCompilerTestExpectSuccess("""
/** @constructor */
function Class() {}

Object.defineProperty(Class.prototype, 'myProperty', {});

alert(new Class().myProperty);
""")

  def testCrDefineProperty(self):
    self._runCompilerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'myProperty', cr.PropertyKind.JS);

alert(new Class().myProperty);
""")

  def testCrDefinePropertyTypeChecking(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'booleanProp', cr.PropertyKind.BOOL_ATTR);

/** @param {number} num */
function needsNumber(num) {}

needsNumber(new Class().booleanProp);
""", "ERROR - [JSC_TYPE_MISMATCH] actual parameter 1 of needsNumber does not "
        "match formal parameter")

  def testCrDefineOnCrWorks(self):
    self._runCompilerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
cr.define('cr', function() {
  return {};
});
""")

  def testAssertWorks(self):
    self._runCompilerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @return {?string} */
function f() {
  return "string";
}

/** @type {!string} */
var a = assert(f());
""")

  def testAssertInstanceofWorks(self):
    self._runCompilerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  return assertInstanceof(a, Class);
}
""")

  def testCrUiDecorateWorks(self):
    self._runCompilerTestExpectSuccess(self._CR_DEFINE_DEFINITION +
        self._CR_UI_DECORATE_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  cr.ui.decorate(a, Class);
  return a;
}
""")

  def testValidScriptCompilation(self):
    self._runCompilerTestExpectSuccess("""
var testScript = function() {
  console.log("hello world")
};
""",
"""'use strict';var testScript=function(){console.log("hello world")};\n""")

  def testOutputWrapper(self):
    source_code = """
var testScript = function() {
  console.log("hello world");
};
"""
    expected_output = ("""(function(){'use strict';var testScript=function()"""
                       """{console.log("hello world")};})();\n""")
    closure_args=["output_wrapper='(function(){%output%})();'"]
    self._runCompilerTestExpectSuccess(source_code, expected_output,
                                       closure_args)

  def testCustomSources(self):
    source_file1 = tempfile.NamedTemporaryFile(delete=False)
    with open(source_file1.name, "w") as f:
      f.write("""
goog.provide('testScript');

var testScript = function() {};
""")
    self._tmp_files.append(source_file1.name)

    source_file2 = tempfile.NamedTemporaryFile(delete=False)
    with open(source_file2.name, "w") as f:
      f.write("""
goog.require('testScript');

testScript();
""")
    self._tmp_files.append(source_file2.name)

    out_file = self._createOutFiles()
    sources = [source_file1.name, source_file2.name]
    closure_args = [a for a in _COMMON_CLOSURE_ARGS if a != "checks_only"]
    found_errors, stderr = self._compiler.run(sources, out_file=out_file,
                                              closure_args=closure_args,
                                              custom_sources=True)
    self.assertFalse(found_errors,
        msg="Expected success, but got failure\n\nOutput:\n%s\n" % stderr)

    expected_output = "'use strict';var testScript=function(){};testScript();\n"
    self.assertTrue(os.path.exists(out_file))
    with open(out_file, "r") as file:
      self.assertEquals(file.read(), expected_output)

  def testExportPath(self):
    self._runCompilerTestExpectSuccess(self._CR_DEFINE_DEFINITION +
        "cr.exportPath('a.b.c');");

  def testExportPathWithTargets(self):
    self._runCompilerTestExpectSuccess(self._CR_DEFINE_DEFINITION +
        "var path = 'a.b.c'; cr.exportPath(path, {}, {});")

  def testExportPathNoPath(self):
    self._runCompilerTestExpectError(self._CR_DEFINE_DEFINITION +
        "cr.exportPath();",
        "ERROR - [JSC_CR_EXPORT_PATH_TOO_FEW_ARGUMENTS] cr.exportPath() should"
            " have at least 1 argument: path name")

  def testMissingReturnAssertNotReached(self):
    template = self._ASSERT_DEFINITION + """
/** @enum {number} */
var Enum = {FOO: 1, BAR: 2};

/**
 * @param {Enum} e
 * @return {number}
 */
function enumToVal(e) {
  switch (e) {
    case Enum.FOO:
      return 1;
    case Enum.BAR:
      return 2;
  }
  %s
}
"""
    args = ['warning_level=VERBOSE']
    self._runCompilerTestExpectError(template % '', 'Missing return',
                                     closure_args=args)
    self._runCompilerTestExpectSuccess(template % 'assertNotReached();',
                                       closure_args=args)


if __name__ == "__main__":
  unittest.main()
