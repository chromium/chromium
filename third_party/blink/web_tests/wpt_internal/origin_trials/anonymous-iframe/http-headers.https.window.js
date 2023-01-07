test(t => {
  assert_true('anonymouslyFramed' in window);
}, 'Anonymous iframe is enabled using HTTP headers');
