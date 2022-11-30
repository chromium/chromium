// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Build a WebAssembly module which uses exception-features.
function createBuilderWithExceptions() {
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("simple_throw_catch_to_0_1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmStmt,
            kExprThrow, except,
          kExprEnd,
          kExprI32Const, 42,
        kExprCatch, except,
          kExprI32Const, 23,
        kExprEnd
      ]).exportFunc();
  return builder;
}

// Starts the function provided by blobURL on a worker and checks that the data
// the worker sends with postMessage is 'true'.
function testModuleCompilationOnWorker(blobURL) {
  let resolve;
  // Create a promise which fulfills when the worker finishes.
  let promise = new Promise(function(res, rej) {
    resolve = res;
  });

  let worker = new Worker(blobURL);
  worker.addEventListener('message', e => resolve(e.data));

  // Send the module bytes to the worker, because the worker does not have
  // access to the WasmModuleBuilder.
  worker.postMessage(createBuilderWithExceptions().toBuffer());

  return promise.then(result => assert_true(result));
}

// Test that WebAssembly exceptions are disabled and a WebAssembly module which
// uses exceptions cannot be compiled.
function testWasmExceptionsDisabled() {
  // If WebAssembly exceptions are enabled even without origin trial token, then
  // we do not run this test.
  if (window.internals.runtimeFlags.webAssemblyExceptionsEnabled)
    return;

  try {
    // Compiling the WebAssembly bytes should throw a CompileError.
    createBuilderWithExceptions().toModule();
    assert_unreachable();
  } catch (e) {
    assert_true(e instanceof WebAssembly.CompileError);
  }
}

function testWasmExceptionsDisabledOnWorker() {
  // If WebAssembly exceptions are enabled even without origin trial token, then
  // we do not run this test.
  if (window.internals.runtimeFlags.webAssemblyExceptionsEnabled) {
    return Promise.resolve();
  }

  const blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            try {
              // Compiling the WebAssembly bytes should throw a CompileError.
              new WebAssembly.Module(data.data);
              self.postMessage(false);
            } catch (e) {
              self.postMessage(e instanceof WebAssembly.CompileError);
            }
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}

function testWasmExceptionsEnabled() {
  const module = createBuilderWithExceptions().toModule();
  assert_not_equals(module, undefined);
  assert_true(module instanceof WebAssembly.Module);
}

function testWasmExceptionsEnabledOnWorker() {
  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            try {
              const module = new WebAssembly.Module(data.data);
              self.postMessage(module !== undefined);
            } catch (e) {
              console.log(e);
              self.postMessage(false);
            }
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}

function testWasmExceptionConstructorEnabled() {
  assert_not_equals(WebAssembly.Exception, undefined);
}

function testWasmExceptionConstructorDisabled() {
  // If WebAssembly exceptions are enabled even without origin trial token, then
  // we do not run this test.
  if (window.internals.runtimeFlags.webAssemblyExceptionsEnabled)
    return;

  assert_equals(WebAssembly.Exception, undefined);
}

function testWasmExceptionConstructorEnabledOnWorker() {
  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            self.postMessage(WebAssembly.Exception !== undefined);
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}

function testWasmExceptionConstructorDisabledOnWorker() {
  // If WebAssembly exceptions are enabled even without origin trial token, then
  // we do not run this test.
  if (window.internals.runtimeFlags.webAssemblyExceptionsEnabled) {
    return Promise.resolve();
  }

  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            self.postMessage(WebAssembly.Exception === undefined);
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}
