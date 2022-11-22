// META: script=/common/get-host-info.sub.js

promise_test(async t => {
  assert_false('credentialless' in window);

  const script_executed = new Promise(resolve => window.script_done = resolve);
  const script = document.createElement('script');
  const current_path = location.pathname;
  const current_dir = current_path.substr(0, current_path.lastIndexOf('/'));
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  script.src = origin + current_dir + '/resources/third_party.js';
  document.head.appendChild(script);

  await script_executed;
  assert_true('credentialless' in window);
}, 'Credentialless iframe is enabled from a third party (first party POV)');
