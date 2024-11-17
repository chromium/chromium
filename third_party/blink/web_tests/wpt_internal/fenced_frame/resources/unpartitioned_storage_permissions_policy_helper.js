// Runs a single test case that checks if the fenced-unpartitioned-storage-read
// Permissions Policy properly gates access to sharedStorage.get() in a fenced
// frame.
//
// The test case is defined by the following parameters:
// - frame_origin: The origin of the fenced frame to test. Use this for testing
//   same-origin vs cross-origin feature availability.
// - type: `header`, `allow`, or `null` (default). If `header`, the fenced
//   frame's policy is set via a HTTP header. If `allow`, the fenced frame's
//   policy is set via the frame's `allow` attribute. If `null`, the fenced
//   frame's policy is not set at all, and it will only inherit the the policy
//   from the top-level page.
// - allowlist: A string containing a *properly-formatted* allowlist of origins
//   as required by `type`. By default, this is `*`, which will allow all
//   origins in all contexts.
async function runUnpartitionedStoragePermissionsPolicyTestCase(
    frame_origin = get_host_info().HTTPS_ORIGIN, type = null, allowlist = '*') {
  response_headers = [];
  if (type === 'header') {
    response_headers.push(
        ['Permissions-Policy', 'fenced-unpartitioned-storage-read=' + allowlist]);
  }

  frame_attributes = [];
  if (type === 'allow') {
    frame_attributes.push(
        ['allow', 'fenced-unpartitioned-storage-read ' + allowlist]);
  }

  const fencedframe = await attachFencedFrameContext({
    origin: frame_origin,
    headers: response_headers,
    attributes: frame_attributes,
  });

  let test_result = await fencedframe.execute(async () => {
    await window.fence.disableUntrustedNetwork();
    try {
      let get_result = await sharedStorage.get('test');
      return get_result;
    } catch (e) {
      if (e.name !== 'OperationError') {
        assert_unreached('Unexpected error: ' + e.name + ' ' + e.message);
      }
      return 'Permission denied';
    }
  });

  return test_result;
};

async function configureSharedStorageDataForTesting() {
  // Set sharedStorage value for HTTPS_ORIGIN
  await sharedStorage.set('test', 'apple');

  // Set sharedStorage value for HTTPS_REMOTE_ORIGIN.
  let init_iframe = await attachIFrameContext(
                  {origin: get_host_info().HTTPS_REMOTE_ORIGIN});
  await init_iframe.execute(async () => {
      await sharedStorage.set('test', 'banana');
  });
};
