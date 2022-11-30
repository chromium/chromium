test(t => {
  assert_false('anonymouslyFramed' in window);
}, 'Anonymous iframe is disabled by default');
