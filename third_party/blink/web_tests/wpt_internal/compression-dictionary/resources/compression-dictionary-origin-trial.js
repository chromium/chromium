function enableCompressionDictionaryOriginTrial() {
  // Generate this token with the given commands:
  //   ./tools/origin_trials/generate_token.py \
  //       https://localhost:8444 \
  //       CompressionDictionaryTransport \
  //       --expire-timestamp=2000000000
  const token = 'Axf5ME7tVCxmufN6plYlnRsYj+FbEYqlQMnslKv5wRP73qCVf90tgGLFWmVEY/duYzvJCXc1pUumbPp0JnquAAYAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQ29tcHJlc3Npb25EaWN0aW9uYXJ5VHJhbnNwb3J0IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9'
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content = token;
  document.head.appendChild(meta);
}
