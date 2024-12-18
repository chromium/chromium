// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js

promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);

  const session = await iframe.contentWindow.ai.languageModel.create();
  session.prompt(kTestPrompt);
  // Detach the iframe.
  iframe.remove();
}, 'Detaching iframe while running prompt() should not cause memory leak');
