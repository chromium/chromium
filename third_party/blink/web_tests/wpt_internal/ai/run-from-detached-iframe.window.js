// META: script=resources/workaround-for-362676838.js

promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Create the iframe and append it to the document.
  let iframe = document.createElement("iframe");
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  let iframeWindow = iframe.contentWindow;
  iframeWindow.assistant = iframeWindow.ai.assistant;
  let iframeDOMException = iframeWindow.DOMException;
  // Detach the iframe.
  iframe.remove();
  // Calling `ai.assistant.capabilities()` from an invalid script state will trigger
  // the "The execution context is not valid." exception.
  await promise_rejects_dom(
    t, 'InvalidStateError', iframeDOMException, iframeWindow.assistant.capabilities(),
    "The promise should be rejected with InvalidStateError if the execution context is invalid."
  );
});