// This test loads a variety of different iframe configurations. It then
// verifies that when the parent changes its baseURI, the child frames do not
// see the change.

// Helper to load the iframes once they have been created and configured.
async function load_iframe(the_iframe) {
  const the_iframe_load = new Promise(resolve => {
    the_iframe.onload = resolve;
  });
  document.body.appendChild(the_iframe);
  await the_iframe_load;
}

async function check_results(the_iframe, expected_base_uri) {
  // Send a postMessage to trigger sending results from `the_iframe`. The
  // following promise resolves with the value of the next message received via
  // postMessage from `the_iframe`.
  const child_base_uri = new Promise(r => onmessage = e => r(e.data));
  the_iframe.contentWindow.postMessage("base url", "*");
  const result = await child_base_uri;
  assert_true(result.links_unchanged);
  assert_equals(expected_base_uri, result.base_uri);
}

onload = () => {
  promise_test(async test => {
    const html_src =
        await fetch('./resources/baseurl-test.html').then(r => r.text());

    // Create some links to verify parent behavior in baseURI change.
    const link_rel1 = document.createElement("a");
    link_rel1.href = "resources/baseurl-test.html";
    link_rel1.id = 'link_rel1';
    document.body.appendChild(link_rel1);
    const link_rel2 = document.createElement("a");
    // Note: link_rel2 below is relative to the origin (it starts with a /),
    // and not the path, of the main document, and so will be different from
    // link_rel1.
    link_rel2.href = "/resources/baseurl-test.html";
    link_rel2.id = 'link_rel2';
    document.body.appendChild(link_rel2);

    // Verify the link urls are what we expect.
    const base_url = new URL(document.baseURI);
    assert_equals(
        base_url.origin + '/resources/baseurl-test.html', link_rel2.href);
    const last_slash_index = base_url.pathname.lastIndexOf('/');
    const test_page_url = base_url.origin +
        base_url.pathname.substr(0, last_slash_index) +
        '/resources/baseurl-test.html';
    assert_equals(test_page_url, link_rel1.href);

    // Create sandboxed srcdoc iframe for test.
    const iframe1 = document.createElement("iframe");
    iframe1.sandbox = "allow-scripts";
    iframe1.srcdoc = html_src;
    await load_iframe(iframe1);

    // Create regular srcdoc iframe for test.
    const iframe2 = document.createElement("iframe");
    iframe2.srcdoc = html_src;
    await load_iframe(iframe2);

    // Create data src iframe for test.
    let data_src = 'data:text/html,' + html_src;
    const iframe3 = document.createElement("iframe");
    iframe3.src = data_src;
    await load_iframe(iframe3);
    // Need to do a read-back here as '%0A' in original gets transcribed to '\n'
    // during the load, and will be represented as such in the child's baseURI.
    data_src = iframe3.src;

    // Create regular src iframe for test.
    const iframe4 = document.createElement("iframe");
    iframe4.src = test_page_url;
    await load_iframe(iframe4);

    // Create src-as-srcdoc frame to test.
    const iframe5 = document.createElement("iframe");
    iframe5.src = 'about:srcdoc';
    await load_iframe(iframe5);
    iframe5.contentDocument.write(html_src);
    await test.step_wait(() => iframe5.contentWindow.script_loaded);

    // Create about:blank frame to test.
    const iframe6 = document.createElement("iframe");
    iframe6.src = 'about:blank';
    await load_iframe(iframe6);
    iframe6.contentDocument.write(html_src);
    await test.step_wait(() => iframe6.contentWindow.script_loaded);

    // Trigger the test scenario by changing the parent's baseURI, then querying
    // the child.
    const old_base_uri = document.baseURI;
    const base_element = document.createElement('base');
    const new_base_uri = "https://foo.com/";
    base_element.href = new_base_uri;
    document.head.appendChild(base_element);
    assert_equals(new_base_uri, document.baseURI);

    // Verify the parent link urls change as expected.
    const new_test_url = new URL(new_base_uri);
    assert_equals(
        new_test_url.origin + '/resources/baseurl-test.html', link_rel2.href);
    const new_last_slash_index = new_test_url.pathname.lastIndexOf('/');
    const new_test_page_url = new_test_url.origin +
        new_test_url.pathname.substr(0, new_last_slash_index) +
        '/resources/baseurl-test.html';
    assert_equals(new_test_page_url, link_rel1.href);

    // sandboxed srcdoc iframe
    await check_results(iframe1, old_base_uri);

    // regular srcdoc iframe
    await check_results(iframe2, old_base_uri);

    // data iframe
    await check_results(iframe3, data_src);

    // regular same-site iframe
    await check_results(iframe4, test_page_url);

    // about:srcdoc iframe
    await check_results(iframe5, old_base_uri);

    // about:blank iframe
    await check_results(iframe6, old_base_uri);
  }, 'iframe doesn\'t see change in parent baseURI');
}
