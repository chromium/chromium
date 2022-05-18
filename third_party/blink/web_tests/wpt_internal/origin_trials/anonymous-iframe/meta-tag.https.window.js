test(t => {
  assert_false('isAnonymouslyFramed' in window);

  // Generated using:
  // ./tools/origin_trials/generate_token.py \
  //  --expire-days 5000 \
  //  --version 3  \
  //  https://www.web-platform.test:8444/ \
  //  AnonymousIframe
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content =
      'AwdHTTICbNy8uTXRBoXUyIR2BqCZTs2wEYHEChRfeyzgFI06chb5ud7lfDB3it3gFS5X9z4H/vxF0M58xWmLfwMAAABgeyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQW5vbnltb3VzSWZyYW1lIiwgImV4cGlyeSI6IDIwODMzMTQ1MTR9';
  document.getElementsByTagName('head')[0].appendChild(meta);

  assert_true('isAnonymouslyFramed' in window);
}, 'Anonymous iframe is enabled using meta tag');
