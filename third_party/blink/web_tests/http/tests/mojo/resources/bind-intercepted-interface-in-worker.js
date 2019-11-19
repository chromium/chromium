importScripts('/resources/testharness.js');
importScripts('/gen/layout_test_data/mojo/public/js/mojo_bindings.js');
importScripts('/gen/content/test/data/mojo_web_test_helper_test.mojom.js');
importScripts('helpers.js');

promise_test(async () => {
  let helperImpl = new TestHelperImpl;
  let interceptor =
      new MojoInterfaceInterceptor(content.mojom.MojoWebTestHelper.name, "context", true);
  interceptor.oninterfacerequest = e => {
    helperImpl.bindRequest(e.handle);
  };
  interceptor.start();

  let helper = new content.mojom.MojoWebTestHelperPtr;
  Mojo.bindInterface(content.mojom.MojoWebTestHelper.name,
                     mojo.makeRequest(helper).handle, "context", true);

  let response = await helper.reverse('the string');
  assert_equals(response.reversed, kTestReply);
  assert_equals(helperImpl.getLastString(), 'the string');
}, 'Can implement a Mojo service and intercept it from a worker');

test(t => {
  assert_throws(
      'NotSupportedError',
      () => {
        new MojoInterfaceInterceptor(content.mojom.MojoWebTestHelper.name,
                                     "process");
      });
}, 'Cannot create a MojoInterfaceInterceptor with process scope');

// done() is needed because the testharness is running as if explicit_done
// was specified.
done();
