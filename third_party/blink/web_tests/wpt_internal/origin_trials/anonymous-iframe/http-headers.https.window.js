test(t => {
  assert_true('isAnonymouslyFramed' in window);
}, 'Anonymous iframe is enabled using HTTP headers');
