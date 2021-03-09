async function referrer_test(expected) {
  const bc = new BroadcastChannel('prerender-channel');

  const gotMessage = new Promise(resolve => {
    bc.addEventListener('message', e => {
      resolve(e.data);
    }, {once: true});
  });

  // Start prerendering a page that will echo its referrer.
  startPrerendering(`resources/echo-referrer.py`);

  const result = await gotMessage;
  assert_equals(result.referrer, expected, 'referrer');
}
