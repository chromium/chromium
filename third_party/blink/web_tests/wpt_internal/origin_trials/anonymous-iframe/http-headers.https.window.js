test(t => {
  assert_true('credentialless' in window);
}, 'Credentialless iframe is enabled using HTTP headers');
