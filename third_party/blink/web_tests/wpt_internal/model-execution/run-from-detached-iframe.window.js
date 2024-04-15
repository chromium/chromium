promise_test(async t => {
  // Make sure the model api is enabled.
  assert_true(!!model);
  // Create the iframe and append it to the document.
  let iframe = document.createElement("iframe");
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  let iframeWindow = iframe.contentWindow;
  let iframeDOMException = iframeWindow.DOMException;
  // Detach the iframe.
  iframe.remove();
  // Calling `model.defaultGenericSessionOptions()` from an invalid script
  // state will trigger the "The execution context is not valid." exception.
  await promise_rejects_dom(
    t, 'InvalidStateError', iframeDOMException, iframeWindow.model.defaultGenericSessionOptions(),
    "The promise should be rejected with InvalidStateError if the execution context is invalid."
  );
});