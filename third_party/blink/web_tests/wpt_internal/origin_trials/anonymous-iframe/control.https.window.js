test(t => {
  assert_false('credentialless' in window);
}, 'Credentialless iframe is disabled by default');
