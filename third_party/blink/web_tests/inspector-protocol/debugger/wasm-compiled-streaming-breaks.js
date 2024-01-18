// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function compileStreamingAndBreak() {
  function call_debugger() {
    debugger;
  }

  // Call from wasm to js.
  // TODO(kimanh): Find a way to use the WasmModuleBuilder.
  const moduleHeader =   [0x0,0x61,0x73,0x6d];
  const moduleVersion =  [0x1,0x0,0x0,0x0];
  const typeSection =    [0x1,0xd,0x4,                // Type section.
                          0x60,0x0,0x0,               // Function type.
                          0x60,0x0,0x0,               // Function type.
                          0x60,0x0,0x0,               // Function type.
                          0x60,0x0,0x0];              // Function type.
  const importSection =  [0x2,0xc,0x1,                // Import section.
                          0x3,0x78,0x78,0x78,         // Module name.
                          0x4,0x66,0x75,0x6e,0x63,    // Field name.
                          0x0,                        // Import kind.
                          0x0];                       // Import index.
  const funcDelaration = [0x3,0x3,0x2,                // Function section.
                          0x1,                        // Function type.
                          0x3];                       // Function type.
  const tableSection =   [0x4,0x4,0x1,                // Table section.
                          0x70,0x0,0x1];              // Funcref, no max, min 1.
  const exportSection =  [0x7,0x8,0x1,                // Export section.
                          0x4,0x6d,0x61,0x69,0x6e,    // Export name.
                          0x0,                        // Export kind.
                          0x2];                       // Export index.
  const elementsSection = [0x9,0x7,0x1,               // Element section.
                           0x0,                       // Active, no index.
                           0x41,0x0,0xb,0x1,0x1];     // i32.const 0, index 1
  const functionBodies =  [0xa,0x11,0x2,              // Code section.
                           0x4,0x0,0x10,0x0,0xb,      // Function body 1.
                           0xa,0x0,0x2,0x40,0x41,     // Function body 2.
                           0x0,0x11,0x2,0x0,0xb,0xb]; // Function body 2.
  const names =           [0x0,0x19,                     // Name section.
                           0x4,0x6e,0x61,0x6d,0x65,      // Name section name.
                           0x1,0x12,0x2,                 // 2 function names.
                           0x1,0x9,0x63,0x61,0x6c,0x6c,  // Name at index 1.
                           0x5f,0x66,0x75,0x6e,0x63,     // Name at index 1.
                           0x2,0x4,0x6d,0x61,0x69,0x6e]; // Name at index 2.

  const wasm = Uint8Array.from([
      ...moduleHeader,
      ...moduleVersion,
      ...typeSection,
      ...importSection,
      ...funcDelaration,
      ...tableSection,
      ...exportSection,
      ...elementsSection,
      ...functionBodies,
      ...names]);

  let b = new Blob([wasm.buffer], {type: 'application/wasm'});
  let bURL =  URL.createObjectURL(b);
  fetch(bURL)
    .then(WebAssembly.compileStreaming)
    .then(module => new WebAssembly.Instance(module, {xxx: {func: call_debugger}}))
    .then(instance => instance.exports.main())
}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test pausing in wasm script.');
  let debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  let debuggers = new Map([[debuggerId, dp.Debugger]]);
  testRunner.log('Did enable debugger.');

  await session.evaluate('(' + compileStreamingAndBreak + ')()');

  let {params: {callFrames, stackTrace, stackTraceId}} = await dp.Debugger.oncePaused();
  testRunner.log('Paused on debugger.');
  await testRunner.logStackTrace(debuggers,
                                {callFrames, stackTrace, stackTraceId},
                                debuggerId);

  await dp.Debugger.resume();
  testRunner.log('Resumed execution.');

  testRunner.completeTest();
})
