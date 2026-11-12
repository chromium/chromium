// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that Runtime.evaluate with generatePreview does not crash when assigning a cyclic object to undefined and evaluating an undeclared variable. Bug 64155.');

  // Enable Debugger so V8 attaches source/stack MessageObjects to rejected
  // replMode evaluation promises (matching Console evaluation).
  await dp.Debugger.enable();

  const evalParams = {
    objectGroup: 'console',
    includeCommandLineAPI: true,
    generatePreview: true,
    replMode: true,
  };

  const expr1 = 'var x = {a:1}; x.self = x; undefined = x;';
  testRunner.log(expr1);
  const {result: res1} =
      await dp.Runtime.evaluate({...evalParams, expression: expr1});
  testRunner.log(res1.result.preview);

  const expr2 = 'unknownVar';
  testRunner.log(expr2);
  const {result: {exceptionDetails}} =
      await dp.Runtime.evaluate({...evalParams, expression: expr2});
  testRunner.log({
    text: exceptionDetails.text,
    className: exceptionDetails.exception.className,
    description: exceptionDetails.exception.description,
  });

  testRunner.completeTest();
});
