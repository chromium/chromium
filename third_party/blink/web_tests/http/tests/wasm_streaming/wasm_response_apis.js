// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const incrementer_url = '../wasm/resources/load-wasm.php';
const not_available_url = '../wasm/resources/not-available.php';
const invalid_wasm_url = '../wasm/resources/load-wasm.php?name=invalid-wasm.wasm';

function AssertType(obj, type) {
  assert_equals(obj.constructor, type);
}

function AssertTypeError(obj) {
  return AssertType(obj, TypeError);
}

function AssertCompileError(obj) {
  return AssertType(obj, WebAssembly.CompileError);
}

function TestStreamedCompile() {
  return fetch(incrementer_url)
    .then(WebAssembly.compileStreaming)
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(5, i.exports.increment(4)));
}

function TestCompileOkStatusIsChecked() {
  return fetch(not_available_url)
    .then(WebAssembly.compileStreaming)
    .then(assert_unreached,
          AssertTypeError);
}

function TestInstantiateOkStatusIsChecked() {
  return fetch(not_available_url)
    .then(WebAssembly.instantiateStreaming)
    .then(assert_unreached,
          AssertTypeError);
}

function TestCompileMimeTypeIsChecked() {
  return fetch('../wasm/resources/incrementer.wasm')
    .then(WebAssembly.compileStreaming)
    .then(assert_unreached,
          AssertTypeError);
}

function TestInstantiateMimeTypeIsChecked() {
  return fetch('../wasm/resources/incrementer.wasm')
    .then(WebAssembly.instantiateStreaming)
    .then(assert_unreached,
          AssertTypeError);
}

function TestShortFormStreamedCompile() {
  return WebAssembly.compileStreaming(fetch(incrementer_url))
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(5, i.exports.increment(4)));
}

function NegativeTestStreamedCompilePromise() {
  return WebAssembly.compileStreaming(Promise.resolve(5))
    .then(assert_unreached,
          AssertTypeError);
}

function CompileBlankResponse() {
  return WebAssembly.compileStreaming(new Response())
    .then(assert_unreached,
          AssertTypeError);
}

function InstantiateBlankResponse() {
  return WebAssembly.instantiateStreaming(new Response())
    .then(assert_unreached,
          AssertTypeError);
}

function CompileEmpty() {
  return WebAssembly.compileStreaming()
    .then(assert_unreached,
          AssertTypeError);
}

function InstantiateEmpty() {
  return WebAssembly.instantiateStreaming()
    .then(assert_unreached,
          AssertTypeError);
}

function CompileFromArrayBuffer() {
  return fetch(incrementer_url)
    .then(r => r.arrayBuffer())
    .then(arr => new Response(arr, {headers:{"Content-Type":"application/wasm"}}))
    .then(WebAssembly.compileStreaming)
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(6, i.exports.increment(5)));
}

function CompileFromInvalidArrayBuffer() {
  var arr = new ArrayBuffer(10);
  var view = new Uint8Array(arr);
  for (var i = 0; i < view.length; ++i) view[i] = i;

  return WebAssembly.compileStreaming(new Response(arr))
    .then(assert_unreached,
          AssertTypeError);
}

function InstantiateFromInvalidArrayBuffer() {
  var arr = new ArrayBuffer(10);
  var view = new Uint8Array(arr);
  for (var i = 0; i < view.length; ++i) view[i] = i;

  return WebAssembly.instantiateStreaming(new Response(arr))
    .then(assert_unreached,
          AssertTypeError);
}

function TestStreamedInstantiate() {
  return fetch(incrementer_url)
    .then(WebAssembly.instantiateStreaming)
    .then(pair => assert_equals(5, pair.instance.exports.increment(4)));
}

function InstantiateFromArrayBuffer() {
  return fetch(incrementer_url)
    .then(response => response.arrayBuffer())
    .then(WebAssembly.instantiateStreaming)
    .then(assert_unreached, AssertTypeError);
}

function TestShortFormStreamedInstantiate() {
  return WebAssembly.instantiateStreaming(fetch(incrementer_url))
    .then(pair => assert_equals(5, pair.instance.exports.increment(4)));
}

function InstantiateFromInvalidArrayBuffer() {
  var arr = new ArrayBuffer(10);
  var view = new Uint8Array(arr);
  for (var i = 0; i < view.length; ++i) view[i] = i;

  return WebAssembly.compileStreaming(new Response(arr))
    .then(assert_unreached,
          AssertTypeError);
}

function buildImportingModuleBytes() {
  var builder = new WasmModuleBuilder();
  builder.addImportedMemory("", "memory", 1);
  var kSig_v_i = makeSig([kWasmI32], []);
  var signature = builder.addType(kSig_v_i);
  builder.addImport("", "some_value", kSig_i_v);
  builder.addImport("", "writer", signature);

  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprI32LoadMem, 0, 0,
      kExprI32Const, 1,
      kExprCallIndirect, signature, kTableZero,
      kExprLocalGet,0,
      kExprI32LoadMem,0, 0,
      kExprCallFunction, 0,
      kExprI32Add
    ]).exportFunc();

  // writer(mem[i]);
  // return mem[i] + some_value();
  builder.addFunction("_wrap_writer", signature)
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, 1]);
  builder.appendToTable([2, 3]);

  var wire_bytes = builder.toBuffer();
  return new Response(wire_bytes, {headers:{"Content-Type":"application/wasm"}});
}

function TestInstantiateComplexModule() {
  var mem_1 = new WebAssembly.Memory({initial: 1});
  var view_1 = new Int32Array(mem_1.buffer);
  view_1[0] = 42;
  var outval_1;

  var ffi = {"":
             {some_value: () => 1,
              writer: (x) => outval_1 = x ,
              memory: mem_1}
            };
  return Promise.resolve(buildImportingModuleBytes())
    .then(b => WebAssembly.instantiateStreaming(b, ffi))
    .then(pair => AssertType(pair.instance, WebAssembly.Instance));
}

function CompileFromInvalidDownload() {
  return WebAssembly.compileStreaming(fetch(invalid_wasm_url))
    .then(assert_unreached,
          AssertCompileError);
}

function InstantiateFromInvalidDownload() {
  return WebAssembly.instantiateStreaming(fetch(invalid_wasm_url))
    .then(assert_unreached,
          AssertCompileError);
}

function TestStreamingCompileExistsInWorker() {
  let resolve;
  // Create a promise which fulfills when the worker finishes.
  let promise = new Promise(function(res, rej) {
    resolve = res;
  });

  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          // Return true if the WebAssembly.compileStreaming exists.
          // WebAssembly.compileStreaming is not executed on the worker because
          // fetch() does not work on a worker. It just causes a timeout.
          self.postMessage(typeof WebAssembly.compileStreaming !== "undefined");
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  let worker = new Worker(blobURL);
  worker.addEventListener('message', e => resolve(e.data));
  return promise.then(exists => assert_true(exists));
}

function TestStreamingInstantiateExistsInWorker() {
  let resolve;
  // Create a promise which fulfills when the worker finishes.
  let promise = new Promise(function(res, rej) {
    resolve = res;
  });

  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          // Return true if the WebAssembly.instantiateStreaming exists.
          // WebAssembly.instantiateStreaming is not executed on the worker because
          // fetch() does not work on a worker. It just causes a timeout.
          self.postMessage(typeof WebAssembly.instantiateStreaming !== "undefined");
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  let worker = new Worker(blobURL);
  worker.addEventListener('message', e => resolve(e.data));
  return promise.then(exists => assert_true(exists));
}

function TestRegression837417() {
  let old_then = WebAssembly.Module.prototype.then;
  WebAssembly.Module.prototype.then = resolve => resolve(String.fromCharCode(
      null, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41));

  return WebAssembly.instantiateStreaming(fetch(incrementer_url))
    .then(pair => assert_equals(5, pair.instance.exports.increment(4)));
}
