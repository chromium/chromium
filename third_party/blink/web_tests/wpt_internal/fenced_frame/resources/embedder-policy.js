// This file should be loaded alongside with utils.js and
// /common/get-host-info.sub.js.
//
// This file is loaded by:
// - embedder-no-coep.https.html
// - embedder-require-corp.https.html

// Make input list to be used as a wptserve pipe
// (https://web-platform-tests.org/writing-tests/server-pipes.html).
// e.g.
// args: ['content-type,text/plain','Age,0']
// return: 'header(content-type,text/plain)|header(Age,0)'
function generateHeader(headers) {
  return headers.map((h) => {
    return 'header(' + h + ')';
  }).join('|');
}

// Setup a fenced frame for embedder-* WPTs.
async function setupTest(test_type, uuid) {
  let headers = ["Supports-Loading-Mode,fenced-frame"];
  let hostname = ''
  switch (test_type) {
    case "coep:require-corp":
      headers.push("cross-origin-embedder-policy,require-corp");
      break;
    case "no coep":
      break;
    case "cross:coep:require-corp":
      // Make sure a different site is used.
      hostname = get_host_info().REMOTE_HOST;
      headers.push("cross-origin-embedder-policy,require-corp");
      break;
    default:
      assert_unreachable("unknown test_type:" + test_type);
      break;
  }
  const tmp_url = new URL('resources/embeddee.html', location.href);
  if (hostname) {
    tmp_url.hostname = hostname;
  }
  tmp_url.searchParams.append("pipe", generateHeader(headers));
  const url = generateURL(tmp_url.toString(), [uuid]);
  return attachFencedFrame(url);
}
