// META: global=window,worker

test(t => {
  let message;
  try {
    new URLPattern({ pathname: '/(foo)/(.???).(jpg|png)' });
  } catch(e) {
    message = e.message;
  }
  assert_true(message.includes('.???'));
}, "URLPattern exception includes invalid regexp group.");
