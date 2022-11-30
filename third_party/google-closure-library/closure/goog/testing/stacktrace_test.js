/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.stacktraceTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Frame = goog.require('goog.testing.stacktrace.Frame');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const StrictMock = goog.require('goog.testing.StrictMock');
const asserts = goog.require('goog.testing.asserts');
const functions = goog.require('goog.functions');
const googString = goog.require('goog.string');
const stacktrace = goog.require('goog.testing.stacktrace');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

const stubs = new PropertyReplacer();
let expectedFailures;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    stubs.set(stacktrace, 'isClosureInspectorActive_', () => false);
  },

  tearDown() {
    stubs.reset();
    expectedFailures.handleTearDown();
  },

  testParseStackFrameInV8() {
    let frameString = '    at Error (unknown source)';
    /** @suppress {visibility} suppression added to enable type checking */
    let frame = stacktrace.parseStackFrame_(frameString);
    let expected = new Frame('', 'Error', '', '');
    assertObjectEquals('exception name only', expected, frame);

    frameString = '    at Object.assert (file:///.../asserts.js:29:10)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected =
        new Frame('Object', 'assert', '', 'file:///.../asserts.js:29:10');
    assertObjectEquals('context object + function name + url', expected, frame);

    frameString = '    at Object.x.y.z (/Users/bob/file.js:564:9)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('Object.x.y', 'z', '', '/Users/bob/file.js:564:9');
    assertObjectEquals(
        'nested context object + function name + url', expected, frame);

    frameString = '    at http://www.example.com/jsunit.js:117:13';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'http://www.example.com/jsunit.js:117:13');
    assertObjectEquals('url only', expected, frame);

    frameString = '    at [object Object].exec [as execute] (file:///foo)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('[object Object]', 'exec', 'execute', 'file:///foo');
    assertObjectEquals('function alias', expected, frame);

    frameString = '    at new Class (file:///foo)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'new Class', '', 'file:///foo');
    assertObjectEquals('constructor call', expected, frame);

    frameString = '    at new <anonymous> (file:///foo)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'new <anonymous>', '', 'file:///foo');
    assertObjectEquals('anonymous constructor call', expected, frame);

    frameString = '    at Array.forEach (native)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('Array', 'forEach', '', '');
    assertObjectEquals('native function call', expected, frame);

    frameString = '    at foo (eval at file://bar)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'foo', '', 'eval at file://bar');
    assertObjectEquals('eval', expected, frame);

    frameString = '    at foo.bar (closure/goog/foo.js:11:99)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('foo', 'bar', '', 'closure/goog/foo.js:11:99');
    assertObjectEquals('Path without schema', expected, frame);

    // In the Chrome console, execute: console.log(eval('Error().stack')).
    frameString =
        '    at eval (eval at <anonymous> (unknown source), <anonymous>:1:1)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame(
        '', 'eval', '',
        'eval at <anonymous> (unknown source), <anonymous>:1:1');
    assertObjectEquals('nested eval', expected, frame);
  },

  testParseStackFrameInOpera() {
    let frameString = '@';
    /** @suppress {visibility} suppression added to enable type checking */
    let frame = stacktrace.parseStackFrame_(frameString);
    let expected = new Frame('', '', '', '');
    assertObjectEquals('empty frame', expected, frame);

    frameString = '@javascript:console.log(Error().stack):1';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'javascript:console.log(Error().stack):1');
    assertObjectEquals('javascript path only', expected, frame);

    frameString = '@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'file:///foo:42');
    assertObjectEquals('path only', expected, frame);

    // (function go() { throw new Error() })()
    // var c = go; c()
    frameString = 'go([arguments not available])@';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'go', '', '');
    assertObjectEquals('name and empty path', expected, frame);

    frameString = 'go([arguments not available])@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'go', '', 'file:///foo:42');
    assertObjectEquals('name and path', expected, frame);

    // (function() { throw new Error() })()
    frameString =
        '<anonymous function>([arguments not available])@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'file:///foo:42');
    assertObjectEquals('anonymous function', expected, frame);

    // var b = {foo: function() { throw new Error() }}
    frameString = '<anonymous function: foo>()@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'foo', '', 'file:///foo:42');
    assertObjectEquals('object literal function', expected, frame);

    // var c = {}; c.foo = function() { throw new Error() }
    frameString = '<anonymous function: c.foo>()@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('c', 'foo', '', 'file:///foo:42');
    assertObjectEquals('named object literal function', expected, frame);

    frameString = '<anonymous function: Foo.prototype.bar>()@';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('Foo.prototype', 'bar', '', '');
    assertObjectEquals('prototype function', expected, frame);

    frameString = '<anonymous function: goog.Foo.prototype.bar>()@';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('goog.Foo.prototype', 'bar', '', '');
    assertObjectEquals('namespaced prototype function', expected, frame);
  },

  // All test strings are parsed with the conventional and long
  // frame algorithms.
  testParseStackFrameInFirefox() {
    let frameString = 'Error("Assertion failed")@:0';
    /** @suppress {visibility} suppression added to enable type checking */
    let frame = stacktrace.parseStackFrame_(frameString);
    let expected = new Frame('', 'Error', '', '');
    assertObjectEquals('function name + arguments', expected, frame);

    frameString = '()@file:///foo:42';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'file:///foo:42');
    assertObjectEquals('anonymous function', expected, frame);

    frameString = '@javascript:alert(0)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', '', '', 'javascript:alert(0)');
    assertObjectEquals('anonymous function', expected, frame);
  },

  // All test strings are parsed with the conventional and long
  // frame algorithms.
  testParseStackFrameInFirefoxWithQualifiedName() {
    const frameString = 'ns.method@http://some.thing/a.js:1:2';
    /** @suppress {visibility} suppression added to enable type checking */
    const frame = stacktrace.parseStackFrame_(frameString);
    const expected =
        new Frame('', 'ns.method', '', 'http://some.thing/a.js:1:2');
    assertObjectEquals('anonymous function', expected, frame);
  },

  testCanonicalizeFrame() {
    const frame = new Frame('<window>', 'foo', 'bar', 'http://x?a=1&b=2:1');
    assertEquals(
        'canonical stack frame, everything is escaped',
        '&lt;window&gt;.foo ' +
            '[as bar] at http://x?a=1&amp;b=2:1',
        frame.toCanonicalString());
  },

  testDeobfuscateFunctionName() {
    stacktrace.setDeobfuscateFunctionName((name) => name.replace(/\$/g, '.'));

    const frame = new Frame('', 'a$b$c', 'd$e', '');
    assertEquals(
        'deobfuscated function name', 'a.b.c [as d.e]',
        frame.toCanonicalString());
  },

  testFramesToString() {
    const normalFrame = new Frame('', 'foo', '', '');
    const anonFrame = new Frame('', '', '', '');
    const frames = [normalFrame, anonFrame, null, anonFrame];
    /** @suppress {visibility} suppression added to enable type checking */
    const stack = stacktrace.framesToString_(frames);
    assertEquals('framesToString', '> foo\n> anonymous\n> (unknown)\n', stack);
  },

  // Create a stack trace string with one modest record and one long record,
  // Verify that all frames are parsed. The length of the long arg is set
  // to blow Firefox 3x's stack if put through a RegExp.
  testParsingLongStackTrace() {
    const longArg =
        googString.buildString('(', googString.repeat('x', 1000000), ')');
    const stackTrace = googString.buildString(
        'shortFrame()@:0\n', 'longFrame', longArg,
        '@http://google.com/somescript:0\n');
    /** @suppress {visibility} suppression added to enable type checking */
    const frames = stacktrace.parse_(stackTrace);
    assertEquals('number of returned frames', 2, frames.length);
    const expected = new Frame('', 'shortFrame', '', '');
    assertObjectEquals('short frame', expected, frames[0]);

    assertNull('exception no frame', frames[1]);
  },

  testParseStackFrameInIE10() {
    let frameString = '   at foo (http://bar:4000/bar.js:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    let frame = stacktrace.parseStackFrame_(frameString);
    let expected = new Frame('', 'foo', '', 'http://bar:4000/bar.js:150:3');
    assertObjectEquals('name and path', expected, frame);

    frameString = '   at Anonymous function (http://bar:4000/bar.js:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected =
        new Frame('', 'Anonymous function', '', 'http://bar:4000/bar.js:150:3');
    assertObjectEquals('Anonymous function', expected, frame);

    frameString = '   at Global code (http://bar:4000/bar.js:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'Global code', '', 'http://bar:4000/bar.js:150:3');
    assertObjectEquals('Global code', expected, frame);

    frameString = '   at foo (eval code:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'foo', '', 'eval code:150:3');
    assertObjectEquals('eval code', expected, frame);

    frameString = '   at eval code (eval code:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    frame = stacktrace.parseStackFrame_(frameString);
    expected = new Frame('', 'eval code', '', 'eval code:150:3');
    assertObjectEquals('nested eval', expected, frame);
  },

  testParseStackFrameInIE11() {
    const frameString = '   at a.b.c (Unknown script code:150:3)';
    /** @suppress {visibility} suppression added to enable type checking */
    const frame = stacktrace.parseStackFrame_(frameString);
    const expected = new Frame('', 'a.b.c', '', 'Unknown script code:150:3');
    assertObjectEquals('name and path', expected, frame);
  },

  testParseStackFrameInEdge() {
    const frameString = '   at a.b.c (http://host.com:80/some/file.js:101:2)';
    /** @suppress {visibility} suppression added to enable type checking */
    const frame = stacktrace.parseStackFrame_(frameString);
    const expected =
        new Frame('', 'a.b.c', '', 'http://host.com:80/some/file.js:101:2');
    assertObjectEquals(expected, frame);
  },

  // Verifies that retrieving the stack trace works when the 'stack' field of an
  // exception contains an array of CallSites instead of a string. This is the
  // case when running in a lightweight V8 runtime (for instance, in gjstests),
  // as opposed to a browser environment.
  testGetStackFrameWithV8CallSites() {
    // A function to create V8 CallSites. Note that CallSite is an extern and
    // thus cannot be mocked with closure mocks.
    function createCallSite(functionName, fileName, lineNumber, colNumber) {
      return {
        getFunctionName: functions.constant(functionName),
        getFileName: functions.constant(fileName),
        getLineNumber: functions.constant(lineNumber),
        getColumnNumber: functions.constant(colNumber),
      };
    }

    // Mock the goog.testing.stacktrace.getStack_ function, which normally
    // triggers an exception for the purpose of reading and returning its stack
    // trace. Here, pretend that V8 provided an array of CallSites instead of
    // the string that browsers provide.
    stubs.set(
        stacktrace, 'getNativeStack_',
        () =>
            [createCallSite('fn1', 'file1', 1, 2),
             createCallSite('fn2', 'file2', 3, 4),
             createCallSite('fn3', 'file3', 5, 6),
    ]);

    // Retrieve the stacktrace. This should translate the array of CallSites
    // into a single multi-line string.
    const stackTrace = stacktrace.get();

    // Make sure the stack trace was translated correctly.
    const frames = stackTrace.split('\n');
    assertEquals(frames[0], '> fn1 at file1:1:2');
    assertEquals(frames[1], '> fn2 at file2:3:4');
    assertEquals(frames[2], '> fn3 at file3:5:6');
  },
});
