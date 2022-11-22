test(t => {
  assert_false('credentialless' in window);

  // Generated using:
  // ./tools/origin_trials/generate_token.py \
  //  --expire-days 5000 \
  //  --version 3  \
  //  https://web-platform.test:8444/ \
  //  AnonymousIframeOriginTrial
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content = 'A97Jrmm+pQTssprM9VjN36d5QjspaFUk9Wj6EuMtjgCpib5dsTKei/4Z8XZdzF5EJmpGlgIy8cr4x2EBedR7qAIAAACBeyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQW5vbnltb3VzSWZyYW1lT3JpZ2luVHJpYWwiLCAiZXhwaXJ5IjogMjA5MjY4Mjc4MiwgImlzVGhpcmRQYXJ0eSI6IHRydWV9';
  document.getElementsByTagName('head')[0].appendChild(meta);

  assert_true('credentialless' in window);
}, 'Credentialless iframe is enabled using meta tag');
