// META: global=window,dedicatedworker,jsshell

// (module
//   (rec
//     (type $struct (descriptor $desc) (struct (field (mut i32))))
//     (type $desc (describes $struct) (struct (field externref)))
//   )
//   (global $proto (import "env" "prototype") externref)
//   (global $desc (ref (exact $desc))
//     (struct.new $desc (global.get $proto))
//   )
//   (func $make (export "make") (result (ref $struct))
//     (struct.new_default_desc $struct (global.get $desc))
//   )
// )
const moduleBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x14,                    // section length 20
  0x02,                    // types count 2
  0x4e,                    // rec. group definition
  0x02,                    // recursive group size 2
  0x4d, 0x01,              //  descriptor 1
  0x5f, 0x01, 0x7f, 0x01,  //  kind: struct, field count 1:  i32 mutable
  0x4c, 0x00,              //  describes 0
  0x5f, 0x01, 0x6f, 0x00,  //  kind: struct, field count 1:  externref immutable
                           // end of rec. group
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x64, 0x00,        // return count 1:  (ref $type0)

  0x02,                    // section kind: Import
  0x12,                    // section length 18
  0x01, 0x03,              // imports count 1: module name length:  3
  0x65, 0x6e, 0x76,        // module name: env
  0x09,                    // field name length:  9
  0x70, 0x72, 0x6f, 0x74, 0x6f, 0x74, 0x79, 0x70,
  0x65,                    // field name: prototype
  0x03, 0x6f, 0x00,        // kind or compact flag: global externref immutable
                           // import #0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x02,              // functions count 1: 0 $make (result (ref $type0))

  0x06,                    // section kind: Global
  0x0b,                    // section length 11
  0x01, 0x64, 0x62, 0x01, 0x00,  // globals count 1: global #1: (ref exact $type1) immutable
  0x23, 0x00,              // global.get $env.prototype
  0xfb, 0x00, 0x01, 0x0b,  // struct.new $type1

  0x07,                    // section kind: Export
  0x08,                    // section length 8
  0x01,                    // exports count 1: export # 0
  0x04,                    // field name length:  4
  0x6d, 0x61, 0x6b, 0x65,  // field name: make
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x09,                    // section length 9
  0x01,                    // functions count 1
                           // function #0 $make
  0x07,                    // body size 7
  0x00,                    // 0 entries in locals list
  0x23, 0x01,              // global.get $global1
  0xfb, 0x21, 0x00,        // struct.new_default_desc $type0
  0x0b,                    // end
]);

test(() => {
  // The experimental custom descriptors feature should be enabled.
  assert_true(WebAssembly.validate(moduleBytes));
});

test(() => {
  const proto = {};
  const imports = {env: {proto}};
  const module = new WebAssembly.Module(moduleBytes);
  const instance = new WebAssembly.Instance(module, imports);
  const o = instance.exports.make();

  // The export should work.
  assert_equals(typeof o, 'object');

  // The JS interop features should not be enabled.
  assert_not_equals(Object.getPrototypeOf(o), proto);
})
