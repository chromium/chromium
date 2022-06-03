importScripts('/resources/testharness.js');

async function importModuleDeps() {
  const {kTestReply, TestHelperImpl} = await import('./helpers.js');
  const {MojoWebTestHelper, MojoWebTestHelperRemote} = await import(
      '/gen/content/test/data/mojo_web_test_helper_test.mojom.m.js');
  Object.assign(
      self,
      {kTestReply, TestHelperImpl, MojoWebTestHelper, MojoWebTestHelperRemote});
}

const imports = importModuleDeps();

promise_test(async () => {
  await imports;

  let helperImpl = new TestHelperImpl();
  let interceptor =
      new MojoInterfaceInterceptor(MojoWebTestHelper.$interfaceName);
  interceptor.oninterfacerequest = e => {
    helperImpl.bindRequest(e.handle);
  };
  interceptor.start();

  let helper = new MojoWebTestHelperRemote();
  helper.$.bindNewPipeAndPassReceiver().bindInBrowser();

  const {reversed} = await helper.reverse('the string');
  assert_equals(reversed, kTestReply);
  assert_equals(helperImpl.getLastString(), 'the string');
}, 'Can implement a Mojo service and intercept it from a worker');

promise_test(async () => {
  await imports;

  assert_throws_dom('NotSupportedError', () => {
    new MojoInterfaceInterceptor(
        MojoWebTestHelper.$interfaceName, 'process');
  });
}, 'Cannot create a MojoInterfaceInterceptor with process scope');

// done() is needed because the testharness is running as if explicit_done
// was specified.
done();
