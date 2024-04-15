promise_test(async t => {
  // Make sure the model api is enabled.
  assert_true(!!model);
  // Make sure the session could be created.
  const status = await model.canCreateGenericSession();
  assert_true(status === 'readily');
  // Start a new session.
  const session = await model.createGenericSession();
  // Test the streaming execute API.
  const streamingResponse = session.executeStreaming("What is 1+2?");
  assert_true(Object.prototype.toString.call(streamingResponse) === "[object ReadableStream]");
  const reader = streamingResponse.getReader();
  let result = "";
  while (true) {
    const { value, done } = await reader.read();
    if (done) {
      break;
    }
    result = value;
  }
  assert_true(result.length > 0);
});
