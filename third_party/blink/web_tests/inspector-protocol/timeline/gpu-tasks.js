// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session} = await testRunner.startHTML(`<head></head><body><canvas></canvas></body>`, 'Tests generation of GPU tasks');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("disabled-by-default-devtools.timeline");
  await session.evaluate(`function run() {
        const canvas = document.querySelector("canvas");
        const gl = canvas.getContext("webgl");
        gl.clearColor(0.0, 0.0, 0.0, 1.0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        return gl.getParameter(gl.MAX_VIEWPORT_DIMS)
      }
      run();
  `);
  const events = await tracingHelper.stopTracing(/disabled-by-default-devtools.timeline/);
  const gpuEvents = events.filter(e => e.name === 'GPUTask')
  // We expect a bunch of these, but to avoid flakes let's just assert that we see at least a few.
  testRunner.log(`Got the expected amount of GPUTasks (at least 5): ${gpuEvents.length > 5}`)
  testRunner.completeTest()
})
