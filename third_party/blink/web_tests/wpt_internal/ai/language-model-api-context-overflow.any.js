// META: script=resources/workaround-for-362676838.js
// META: timeout=long

promise_test(async () => {
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  // TODO(crbug.com/376789810): make it a PRECONDITION_FAILED if the model is
  // not ready.
  assert_true(status !== "no");
  // Start a new session.
  const session = await ai.languageModel.create();
  const maxTokens = session.maxTokens;
  // Our goal is to test the prompt API with context overflow, but it depends
  // on the implementation details of the model execution and context
  // management. Here, we will make two assumptions:
  // 1. The context will accumulate if we keep calling the prompt API. This is
  // to ensure the context will eventually overflow.
  // 2. The context will accumulate faster if the input is larger. We will
  // keep the input as large as possible to to minimize the number of prompt()
  // calls to avoid timeout.
  const promptParts = "hello ";
  // Find the largest repeat count that won't make the prompt text exceed
  // the limit.
  // Alternatively we could try to construct a small prompt, and keep doubling
  // it until the token count exceeds half of the limit (L/2) to ensure we
  // reach context overflow with two prompt call. However, it's not guaranteed
  // that the number of tokens gets doubled when we double the prompt. In this
  // case, we may end up with a prompt consisting of T tokens, where T<L/2 and
  // doubling the prompt makes the tokens count become T' where T'>L. It may
  // require three prompt calls instead of two.
  let left = 1, right = maxTokens;
  while (left < right) {
    const mid = Math.floor((left + right + 1) / 2);
    if (await session.countPromptTokens(promptParts.repeat(mid)) <= maxTokens) {
      left = mid;
    } else {
      right = mid - 1;
    }
  }
  // Construct the prompt input.
  const promptString = promptParts.repeat(left);
  // Register the event listener.
  let isContextOverflowEventFired = false;
  const promise = new Promise(resolve => {
    session.addEventListener("contextoverflow", () => {
      isContextOverflowEventFired = true;
      resolve(true);
    });
  });
  // Keep calling `prompt()` until the `contextoverflow` event is triggered.
  const runPrompt = async () => {
    if (!isContextOverflowEventFired) {
      await session.prompt(promptString);
      // Calling `SetTimeout()` each time ensures that the event has a
      // chance to fire.
      setTimeout(runPrompt, 0);
    }
  };
  runPrompt();
  assert_true(await promise);
}, "event listener should be triggered when the context overflows.");
