cr.define('cr.foo', function() {
  /* #export */ function foo() {}
  function bar() {}
  /* #export */ function baz() {}

  // #cr_define_end
  return {
    foo: foo,
    baz: baz,
  };
});
