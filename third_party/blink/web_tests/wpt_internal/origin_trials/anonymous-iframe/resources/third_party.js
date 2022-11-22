test(t => {
  assert_false('anonymouslyFramed' in window);

  // Generated using:
  // ./tools/origin_trials/generate_token.py \
  //  --expire-days 5000 \
  //  --version 3  \
  //  --is-third-party  \
  //  https://www1.web-platform.test:8444/ \
  //  AnonymousIframeOriginTrial
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content = 'A8m7AzjLzS+9KpoZAzoG+UbvPeh/DbN0BcSwnW5fjSICjYqqjFUZw5Zgmvru6FQXYLceUgNvwgnMfbinYdMKYgsAAACGeyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cxLndlYi1wbGF0Zm9ybS50ZXN0Ojg0NDQiLCAiZmVhdHVyZSI6ICJBbm9ueW1vdXNJZnJhbWVPcmlnaW5UcmlhbCIsICJleHBpcnkiOiAyMDkyNzMzNjczLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=';
  document.getElementsByTagName('head')[0].appendChild(meta);

  assert_true('credentialless' in window);

  window.script_done();
}, 'Credentialless iframe is enabled from a third party (third party POV)');
