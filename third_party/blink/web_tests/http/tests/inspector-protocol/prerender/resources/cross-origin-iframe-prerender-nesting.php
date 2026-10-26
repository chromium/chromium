<?php
header('Supports-Loading-Mode: prerender-cross-origin-frames');
?>
<!DOCTYPE html>
<script src="/resources/get-host-info.js"></script>
<body>
<a href="?prerendering" id="link">link</a>
<script>
function appendIframe(url) {
  const iframe = document.createElement('iframe');
  iframe.src = url;
  document.body.appendChild(iframe);
}

function main() {
  // 1. Middle iframe WITH SLM header.
  const slmUrl = new URL(
      'cross-origin-iframe-nesting-child.php?slm',
      get_host_info()['HTTP_REMOTE_ORIGIN'] + window.location.pathname);
  window.addEventListener('message', (e) => {
    if (e.data === 'slm_done') {
      // 2. Middle iframe WITHOUT SLM header.
      const noSlmUrl = new URL(
          'cross-origin-iframe-nesting-child.php?no_slm',
          get_host_info()['HTTP_REMOTE_ORIGIN'] + window.location.pathname);
      appendIframe(noSlmUrl.href);
    }
  });
  appendIframe(slmUrl.href);
}

const params = new URLSearchParams(location.search);
if (!params.has('prerendering')) {
  const specScript = document.createElement('script');
  specScript.type = 'speculationrules';
  specScript.textContent = JSON.stringify({
    prerender: [{source: 'list', urls: ['?prerendering']}]
  });
  document.head.appendChild(specScript);
} else {
  main();
}
</script>
</body>
