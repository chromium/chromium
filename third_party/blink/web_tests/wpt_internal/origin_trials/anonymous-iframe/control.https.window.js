test(t => {
  assert_false('isAnonymouslyFramed' in window);
}, 'Anonymous iframe is disabled by default');
