'use strict';

// These tests can be imported as a module or added as a script to tests lite
// bindings.

promise_test(() => {
  const targetRouter = new TestMessageTargetCallbackRouter;
  const targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  targetRouter.deconstruct.addListener(({x, y, z}) => ({
    x: x,
    y: y,
    z: z
  }));

  return targetRemote.deconstruct({x: 1}).then(reply => {
    assert_equals(reply.x, 1);
    assert_equals(reply.y, 2);
    assert_equals(reply.z, 1);
  });
}, 'structs with default values from nested enums and constants are ' +
   'correctly serialized');

promise_test(() => {
  const targetRouter = new TestMessageTargetCallbackRouter;
  const targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  targetRouter.flatten.addListener(values => ({values: values.map(v => v.x)}));
  return targetRemote.flatten([{x: 1}, {x: 2}, {x: 3}]).then(reply => {
    assert_array_equals(reply.values, [1, 2, 3]);
  });
}, 'regression test for complex array serialization');

promise_test(() => {
  const targetRouter = new TestMessageTargetCallbackRouter;
  const targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  targetRouter.flattenUnions.addListener(unions => {
    return {x: unions.filter(u => u.x !== undefined).map(u => u.x),
            s: unions.filter(u => u.s !== undefined).map(u => u.s.x)};
  });

  return targetRemote.flattenUnions(
    [{x: 1}, {x: 2}, {s: {x: 3}}, {s: {x: 4}}, {x: 5}, {s: {x: 6}}])
                     .then(reply => {
                       assert_array_equals(reply.x, [1, 2, 5]);
                       assert_array_equals(reply.s, [3, 4, 6]);
                     });
}, 'can serialize and deserialize unions');

promise_test(() => {
  const targetRouter = new TestMessageTargetCallbackRouter;
  const targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  targetRouter.flattenMap.addListener(map => {
    return {keys: Object.keys(map), values: Object.values(map)};
  });

  return targetRemote.flattenMap({0: 1, 2: 3, 4: 5}).then(reply => {
    assert_array_equals(reply.keys, [0, 2, 4]);
    assert_array_equals(reply.values, [1, 3, 5]);
  });
}, 'can serialize and deserialize maps with odd number of items');

promise_test(() => {
  const targetRouter = new TestMessageTargetCallbackRouter;
  const targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  targetRouter.flattenMap.addListener(map => {
    return {keys: Object.keys(map), values: Object.values(map)};
  });

  return targetRemote.flattenMap({0: 1, 2: 3}).then(reply => {
    assert_array_equals(reply.keys, [0, 2]);
    assert_array_equals(reply.values, [1, 3]);
  });
}, 'can serialize and deserialize maps with even number of items');

function getMojoEchoRemote() {
  let remote = new MojoEchoRemote;
  remote.$.bindNewPipeAndPassReceiver().bindInBrowser('process');
  return remote;
}

promise_test(async () => {
  const remote = getMojoEchoRemote();
  {
    const {value} = await remote.echoBoolFromUnion({boolValue: true});
    assert_true(value);
  }
  {
    const {value} = await remote.echoInt32FromUnion({int32Value: 123});
    assert_equals(value, 123);
  }
  {
    const {value} = await remote.echoStringFromUnion({stringValue: "foo"});
    assert_equals(value, "foo");
  }
}, 'JS encoding and C++ decoding of unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const {testUnion: {boolValue}} = await remote.echoBoolAsUnion(true);
    assert_equals(boolValue, true);
  }
  {
    const {testUnion: {int32Value}} = await remote.echoInt32AsUnion(123);
    assert_equals(int32Value, 123);
  }
  {
    const {testUnion: {stringValue}} = await remote.echoStringAsUnion("foo");
    assert_equals(stringValue, "foo");
  }

}, 'JS decoding and C++ encoding of unions work as expected.');

promise_test(async () => {
  const remote = getMojoEchoRemote();
  {
    const response = await remote.echoNullFromOptionalUnion();
    assert_equals(Object.keys(response).length, 0);
  }
  {
    const {value} = await remote.echoBoolFromOptionalUnion({boolValue: true});
    assert_true(value);
  }
  {
    const {value} = await remote.echoInt32FromOptionalUnion({int32Value: 123});
    assert_equals(value, 123);
  }
  {
    const {value} = await remote.echoStringFromOptionalUnion({stringValue: "foo"});
    assert_equals(value, "foo");
  }
}, 'JS encoding and C++ decoding of optional unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const {testUnion} = await remote.echoNullAsOptionalUnion();
    assert_equals(testUnion, null);
  }
  {
    const {testUnion: {boolValue}} = await remote.echoBoolAsOptionalUnion(true);
    assert_equals(boolValue, true);
  }
  {
    const {testUnion: {int32Value}} = await remote.echoInt32AsOptionalUnion(123);
    assert_equals(int32Value, 123);
  }
  {
    const {testUnion: {stringValue}} =
      await remote.echoStringAsOptionalUnion("foo");
    assert_equals(stringValue, "foo");
  }

}, 'JS decoding and C++ encoding of optional unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const {value} = await remote.echoInt8FromNestedUnion({int8Value: -10});
    assert_equals(value, -10);
  }
  {
    const {value} = await remote.echoBoolFromNestedUnion({unionValue: {boolValue: true}});
    assert_true(value);
  }
  {
    const {value} = await remote.echoStringFromNestedUnion({unionValue: {stringValue: 'foo'}});
    assert_equals(value, 'foo');
  }
}, 'JS encoding and C++ decoding of nested unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const {testUnion: {int8Value}} = await remote.echoInt8AsNestedUnion(-10);
    assert_equals(int8Value, -10);
  }
  {
    const {testUnion: {unionValue: {boolValue}}} = await remote.echoBoolAsNestedUnion(true);
    assert_true(boolValue);
  }
  {
    const {testUnion: {unionValue: {stringValue}}} =
      await remote.echoStringAsNestedUnion('foo');
    assert_equals(stringValue, 'foo');
  }
}, 'JS decoding and C++ encoding of nested unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const response = await remote.echoNullFromOptionalNestedUnion();
    assert_equals(Object.keys(response).length, 0);
  }
  {
    const {value} = await remote.echoInt8FromOptionalNestedUnion({int8Value: -10});
    assert_equals(value, -10);
  }
  {
    const {value} = await remote.echoBoolFromOptionalNestedUnion({unionValue: {boolValue: true}});
    assert_true(value);
  }
  {
    const {value} = await remote.echoStringFromOptionalNestedUnion({unionValue: {stringValue: 'foo'}});
    assert_equals(value, 'foo');
  }
}, 'JS encoding and C++ decoding of optional nested unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  {
    const {testUnion} = await remote.echoNullAsOptionalNestedUnion();
    assert_equals(testUnion, null);
  }
  {
    const {testUnion: {int8Value}} = await remote.echoInt8AsOptionalNestedUnion(-10);
    assert_equals(int8Value, -10);
  }
  {
    const {testUnion: {unionValue: {boolValue}}} =
      await remote.echoBoolAsOptionalNestedUnion(true);
    assert_true(boolValue);
  }
  {
    const {testUnion: {unionValue: {stringValue}}} =
      await remote.echoStringAsOptionalNestedUnion('foo');
    assert_equals(stringValue, 'foo');
  }
}, 'JS decoding and C++ encoding of optional nested unions work as expected.');

promise_test(async() => {
  const remote = getMojoEchoRemote();
  const response = await remote.echoBoolArray([true, false, true]);
  assert_array_equals(response.values, [true, false, true]);
}, 'JS encoding and decoding array of bools as expected.');
