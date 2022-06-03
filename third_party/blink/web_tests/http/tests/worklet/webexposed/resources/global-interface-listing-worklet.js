/* Adopted from LayoutTests/resources/global-interface-listing.js */

// Run all the code in a local scope.
(function() {

// Generally, Worklet should not have a reference to the global object.
// https://drafts.css-houdini.org/worklets/#code-idempotency
if (this) {
  console.error('"this" should not refer to the global object');
  return;
}
// Instead, retrieve the global object in a tricky way.
var global_object = Function('return this')();

var globals = [];

// List of builtin JS constructors; Blink is not controlling what properties these
// objects have, so exercising them in a Blink test doesn't make sense.
//
// This list should be kept in sync with the one at web_tests/resources/global-interface-listing.js
var js_builtins = new Set([
    'AggregateError',
    'Array',
    'ArrayBuffer',
    'Atomics',
    'BigInt',
    'BigInt64Array',
    'BigUint64Array',
    'Boolean',
    'DataView',
    'Date',
    'Error',
    'EvalError',
    'FinalizationRegistry',
    'Float32Array',
    'Float64Array',
    'Function',
    'Infinity',
    'Int16Array',
    'Int32Array',
    'Int8Array',
    'Intl',
    'JSON',
    'Map',
    'Math',
    'NaN',
    'Number',
    'Object',
    'Promise',
    'Proxy',
    'RangeError',
    'ReferenceError',
    'Reflect',
    'RegExp',
    'Set',
    'SharedArrayBuffer',
    'String',
    'Symbol',
    'SyntaxError',
    'TypeError',
    'URIError',
    'Uint16Array',
    'Uint32Array',
    'Uint8Array',
    'Uint8ClampedArray',
    'WeakMap',
    'WeakRef',
    'WeakSet',
    'WebAssembly',
    'decodeURI',
    'decodeURIComponent',
    'encodeURI',
    'encodeURIComponent',
    'escape',
    'eval',
    'isFinite',
    'isNaN',
    'parseFloat',
    'parseInt',
    'undefined',
    'unescape',
]);

function is_web_idl_constructor(property_name) {
  if (js_builtins.has(property_name))
    return false;
  var descriptor = Object.getOwnPropertyDescriptor(global_object, property_name);
  if (descriptor.value === undefined ||
      descriptor.value.prototype === undefined) {
    return false;
  }
  return descriptor.writable && !descriptor.enumerable &&
         descriptor.configurable;
}

function collect_property_info(object, property_name, output) {
  var keywords = ('prototype' in object) ? 'static ' : '';
  var descriptor = Object.getOwnPropertyDescriptor(object, property_name);
  if ('value' in descriptor) {
    var type;
    if (typeof descriptor.value === 'function') {
      type = 'method';
    } else {
      type = 'attribute';
    }
    output.push('    ' + keywords + type + ' ' + property_name);
  } else {
    if (descriptor.get)
      output.push('    ' + keywords + 'getter ' + property_name);
    if (descriptor.set)
      output.push('    ' + keywords + 'setter ' + property_name);
  }
}

var interface_names = Object.getOwnPropertyNames(global_object).filter(is_web_idl_constructor);
interface_names.sort();
interface_names.forEach(function(interface_name) {
  var inherits_from = global_object[interface_name].__proto__.name;
  if (inherits_from)
    globals.push('interface ' + interface_name + ' : ' + inherits_from);
  else
    globals.push('interface ' + interface_name);
  // List static properties then prototype properties.
  [global_object[interface_name], global_object[interface_name].prototype].forEach(function(object) {
    if ('prototype' in object) {
      // Skip properties that aren't static (e.g. consts), or are inherited.
      var proto_properties = new Set(Object.keys(object.prototype).concat(
                                     Object.keys(object.__proto__)));
      var property_names = Object.keys(object).filter(function(name) {
        return !proto_properties.has(name);
      });
    } else {
      var property_names = Object.getOwnPropertyNames(object);
    }
    var property_strings = [];
    property_names.forEach(function(property_name) {
      collect_property_info(object, property_name, property_strings);
    });
    globals.push.apply(globals, property_strings.sort());
  });
});

globals.push('global object');
var property_strings = [];
var member_names = Object.getOwnPropertyNames(global_object).filter(function(property_name) {
  return !js_builtins.has(property_name) && !is_web_idl_constructor(property_name);
});
member_names.forEach(function(property_name) {
  collect_property_info(global_object, property_name, property_strings);
});
globals.push.apply(globals, property_strings.sort());

// Worklets don't have a mechanism to communicate back to the main page, dump
// results into the console.
globals.forEach(function(global) {
    console.log(global);
});

})(); // Run all the code in a local scope.
