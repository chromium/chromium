test(t => {
  assert_false('isAnonymouslyFramed' in window);

  // Generated using:
  // ./tools/origin_trials/generate_token.py \
  //  --expire-days 5000 \
  //  --version 3  \
  //  --is-third-party  \
  //  https://www1.web-platform.test:8444/ \
  //  AnonymousIframe
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content =
      'A+Rv4/kLwNxdRK/vI7CUzO69HFKU5PbhJVsYOm7fqfGe5C1BQBr0UewqwYcKJemwUAqXK2Yowr5N6jMqW+2lygIAAAB7eyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cxLndlYi1wbGF0Zm9ybS50ZXN0Ojg0NDQiLCAiZmVhdHVyZSI6ICJBbm9ueW1vdXNJZnJhbWUiLCAiZXhwaXJ5IjogMjA4MzMyMDEwMiwgImlzVGhpcmRQYXJ0eSI6IHRydWV9';
  document.getElementsByTagName('head')[0].appendChild(meta);

  assert_true('isAnonymouslyFramed' in window);

  window.script_done();
}, 'Anonymous iframe is enabled from a third party (third party POV)');
