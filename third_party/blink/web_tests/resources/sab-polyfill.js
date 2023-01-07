// See https://github.com/whatwg/html/issues/5380 for why the SharedArrayBuffer
// constructor doesn't always exist.
if (!self.SharedArrayBuffer) {
  const sabConstructor = new WebAssembly.Memory({
    shared:true, initial:0, maximum:0 }).buffer.constructor;
  if (sabConstructor.name !== "SharedArrayBuffer") {
    throw new Error("WebAssembly.Memory does not support shared:true");
  }
  self.SharedArrayBuffer = sabConstructor;
}
