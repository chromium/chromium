'use strict';

// These tests can be imported as a module or added as a script to tests lite
// bindings.

const kTestMessage = 'hello there';
const kTestNumbers = [0, 1, 1, 2, 3, 5, 8, 13, 21];

class TargetImpl {
  constructor() {
    this.numPokes = 0;
    this.target = new TestMessageTargetReceiver(this);
  }

  poke() { this.numPokes++; }
  ping() { return Promise.resolve(); }
  repeat(message, numbers) { return {message: message, numbers: numbers}; }
  echo(nested) { return Promise.resolve({nested: nested}); }
  deconstruct(test_struct) {}
  flatten(values) {}
  flattenUnions(unions) {}
  flattenMap(map) {}
  requestSubinterface(request, client) {}
}

promise_test(() => {
  let impl = new TargetImpl;
  let remote = impl.target.$.bindNewPipeAndPassRemote();
  remote.poke();
  return remote.ping().then(() => {
    assert_equals(impl.numPokes, 1);
  });
}, 'messages with replies return Promises that resolve on reply received');

promise_test(() => {
  let impl = new TargetImpl;
  let remote = impl.target.$.bindNewPipeAndPassRemote();
  return remote.repeat(kTestMessage, kTestNumbers)
               .then(reply => {
                 assert_equals(reply.message, kTestMessage);
                 assert_array_equals(reply.numbers, kTestNumbers);
               });
}, 'implementations can reply with multiple reply arguments');

promise_test(() => {
  let impl = new TargetImpl;
  let remote = impl.target.$.bindNewPipeAndPassRemote();
  const enumValue = TestMessageTarget_NestedEnum.kFoo;
  return remote.echo(enumValue)
               .then(({nested}) => assert_equals(nested, enumValue));
}, 'nested enums are usable as arguments and responses.');

promise_test(async (t) => {
  const impl = new TargetImpl;
  const remote = impl.target.$.bindNewPipeAndPassRemote();

  await remote.ping();
  remote.$.close();

  await promise_rejects_js(t, Error, remote.ping());
}, 'after the pipe is closed all future calls should fail');

promise_test(async (t) => {
  const impl = new TargetImpl;
  const remote = impl.target.$.bindNewPipeAndPassRemote();

  // None of these promises should successfully resolve because we are
  // immediately closing the pipe.
  const promises = []
  for (let i = 0; i < 10; i++) {
    promises.push(remote.ping());
  }

  remote.$.close();

  for (const promise of promises) {
    await promise_rejects_js(t, Error, promise);
  }
}, 'closing the pipe drops any pending messages');

promise_test(() => {
  let impl = new TargetImpl;

  // Intercept any browser-bound request for TestMessageTarget and bind it
  // instead to the local |impl| object.
  let interceptor = new MojoInterfaceInterceptor(
      TestMessageTarget.$interfaceName);
  interceptor.oninterfacerequest = e => {
    impl.target.$.bindHandle(e.handle);
  }
  interceptor.start();

  let remote = TestMessageTarget.getRemote();
  remote.poke();
  return remote.ping().then(() => {
    assert_equals(impl.numPokes, 1);
  });
}, 'getRemote() attempts to send requests to the frame host');

promise_test(() => {
  let router = new TestMessageTargetCallbackRouter;
  let remote = router.$.bindNewPipeAndPassRemote();
  return new Promise(resolve => {
    router.poke.addListener(resolve);
    remote.poke();
  });
}, 'basic generated CallbackRouter behavior works as intended');

promise_test(() => {
  let router = new TestMessageTargetCallbackRouter;
  let remote = router.$.bindNewPipeAndPassRemote();
  let numPokes = 0;
  router.poke.addListener(() => ++numPokes);
  router.ping.addListener(() => Promise.resolve());
  remote.poke();
  return remote.ping().then(() => assert_equals(numPokes, 1));
}, 'CallbackRouter listeners can reply to messages');

promise_test(() => {
  let router = new TestMessageTargetCallbackRouter;
  let remote = router.$.bindNewPipeAndPassRemote();
  router.repeat.addListener(
    (message, numbers) => ({message: message, numbers: numbers}));
  return remote.repeat(kTestMessage, kTestNumbers)
               .then(reply => {
                 assert_equals(reply.message, kTestMessage);
                 assert_array_equals(reply.numbers, kTestNumbers);
               });
}, 'CallbackRouter listeners can reply with multiple reply arguments');

promise_test(() => {
  let targetRouter = new TestMessageTargetCallbackRouter;
  let targetRemote = targetRouter.$.bindNewPipeAndPassRemote();
  let subinterfaceRouter = new SubinterfaceCallbackRouter;
  targetRouter.requestSubinterface.addListener((request, client) => {
    let values = [];
    subinterfaceRouter.$.bindHandle(request.handle);
    subinterfaceRouter.push.addListener(value => values.push(value));
    subinterfaceRouter.flush.addListener(() => {
      client.didFlush(values);
      values = [];
    });
  });

  let clientRouter = new SubinterfaceClientCallbackRouter;
  let subinterfaceRemote = new SubinterfaceRemote;
  targetRemote.requestSubinterface(
    subinterfaceRemote.$.bindNewPipeAndPassReceiver(),
    clientRouter.$.bindNewPipeAndPassRemote());
  return new Promise(resolve => {
    clientRouter.didFlush.addListener(values => {
      assert_array_equals(values, kTestNumbers);
      resolve();
    });

    kTestNumbers.forEach(n => subinterfaceRemote.push(n));
    subinterfaceRemote.flush();
  });
}, 'can send and receive interface requests and proxies');

promise_test(() => {
  let impl = new TargetImpl;
  let remote = impl.target.$.bindNewPipeAndPassRemote();

  // Poke a bunch of times. These should never race with the assertion below,
  // because the |flushForTesting| request/response is ordered against other
  // messages on |remote|.
  const kNumPokes = 100;
  for (let i = 0; i < kNumPokes; ++i)
    remote.poke();
  return remote.$.flushForTesting().then(() => {
    assert_equals(impl.numPokes, kNumPokes);
  });
}, 'can use generated flushForTesting API for synchronization in tests');

promise_test(async () => {
  let clientRouter = new SubinterfaceClientCallbackRouter;
  let clientRemote = clientRouter.$.bindNewPipeAndPassRemote();

  let actualDidFlushes = 0;
  clientRouter.didFlush.addListener(values => {
    actualDidFlushes++;
  });

  const kExpectedDidFlushes = 1000;
  for (let i = 0; i < kExpectedDidFlushes; i++) {
    clientRemote.didFlush([]);
  }

  await clientRouter.$.flush();
  assert_equals(actualDidFlushes, kExpectedDidFlushes);
}, 'can use generated flush API of callbackrouter/receiver for synchronization');

promise_test(async(t) => {
  const impl = new TargetImpl;
  const remote = impl.target.$.bindNewPipeAndPassRemote();
  const disconnectPromise = new Promise(resolve => impl.target.onConnectionError.addListener(resolve));
  remote.$.close();
  return disconnectPromise;
}, 'InterfaceTarget connection error handler runs when set on an Interface object');

promise_test(() => {
  const router = new TestMessageTargetCallbackRouter;
  const remote = router.$.bindNewPipeAndPassRemote();
  const disconnectPromise = new Promise(resolve => router.onConnectionError.addListener(resolve));
  remote.$.close();
  return disconnectPromise;
}, 'InterfaceTarget connection error handler runs when set on an InterfaceCallbackRouter object');
