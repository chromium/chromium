<?php
if (isset($_GET['slm'])) {
  header('Supports-Loading-Mode: prerender-cross-origin-frames');
}
?>
<!DOCTYPE html>
<script src="/resources/get-host-info.js"></script>
<body>
<script>
function appendIframe(url) {
  return new Promise(resolve => {
    const iframe = document.createElement('iframe');
    iframe.src = url;
    iframe.onload = resolve;
    document.body.appendChild(iframe);
  });
}

(async () => {
  const suffix = location.search.slice(1);
  // 1. Same-origin nested iframe (relative URL).
  const sameUrl = new URL(`empty.html?same_${suffix}`, location.href);
  const samePromise = appendIframe(sameUrl.href);

  // 2. Cross-origin nested iframe (UNAUTHENTICATED_ORIGIN).
  const crossUrl = new URL(
      `empty.html?cross_${suffix}`,
      get_host_info()['UNAUTHENTICATED_ORIGIN'] + location.pathname);
  const crossPromise = appendIframe(crossUrl.href);
  if (suffix === 'slm') {
    await Promise.all([samePromise, crossPromise]);
    window.parent.postMessage('slm_done', '*');
  }
})();
</script>
</body>
