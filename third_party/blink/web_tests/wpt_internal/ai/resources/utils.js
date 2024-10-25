const testSession = async (session) => {
  if (typeof session.topK !== 'number') {
    return {success: false, error: 'session topK property is not properly set'};
  }

  if (typeof session.temperature !== 'number') {
    return {
      success: false,
      error: 'session temperature property is not properly set'
    };
  }

  if (typeof session.maxTokens !== 'number' ||
      typeof session.tokensSoFar !== 'number' ||
      typeof session.tokensLeft !== 'number') {
    return {
      success: false,
      error: 'session token properties is not properly set'
    };
  }

  if (session.tokensLeft + session.tokensSoFar != session.maxTokens) {
    return {
      success: false,
      error:
          'the sum of tokensLeft and tokensSoFar should be equal to maxTokens'
    };
  }

  const prevTokenSoFar = session.tokensSoFar;
  const prevTokensLeft = session.tokensLeft;

  const result = await session.prompt("What is the result of 0*2?");
  if (typeof result !== "string" || result.length === 0) {
    return {
      success: false,
      error: "the prompt API doesn't receive any response"
    };
  }

  if (session.tokensLeft + session.tokensSoFar != session.maxTokens) {
    return {
      success: false,
      error:
          'the sum of tokensLeft and tokensSoFar should be equal to maxTokens'
    };
  }

  // Note that the tokensSoFar may stay unchanged even if the
  // result is non-empty, because the session may evict some old
  // context when the token overflows.

  return {
    success: true
  };
};

const testPromptAPI = async () => {
  if (!ai) {
    return {
      success: false,
      error: "ai is not defined in the scope"
    };
  }

  try {
    const capabilities = await ai.languageModel.capabilities();
    const status = capabilities.available;
    if (status === "no") {
      return {
        success: false,
        error: "cannot create text session"
      };
    }

    isDownloadProgressEventTriggered = false;
    let isWaitingForModelDownload = status === "after-download";

    const session = await ai.languageModel.create({
      topK: 3,
      temperature: 0.8,
      systemPrompt: "Let's talk about Mauritius.",
      monitor(m) {
        m.addEventListener("downloadprogress", e => {
          isDownloadProgressEventTriggered = true;
        });
      }
    });

    if (isWaitingForModelDownload && !isDownloadProgressEventTriggered) {
      return {
        success: false,
        error:
          "when the status is 'after-download', the creation request " +
          "should wait for the model download, and the `downloadprogress` " +
          "event should be triggered."
      };
    }
    return testSession(session);
  } catch (e) {
    return {
      success: false,
      error: e
    };
  }
};

// The method should take the AbortSignal as an option and return a promise.
const testAbort = async (t, method) => {
  // Test abort signal without custom error.
  {
    const controller = new AbortController();
    const promise = method(controller.signal);
    controller.abort();
    await promise_rejects_dom(t, "AbortError", promise);

    // Using the same aborted controller will get the `AbortError` as well.
    const anotherPromise = method(controller.signal);
    await promise_rejects_dom(t, "AbortError", anotherPromise);
  }

  // Test abort signal with custom error.
  {
    const err = new Error("test");
    const controller = new AbortController();
    const promise = method(controller.signal);
    controller.abort(err);
    await promise_rejects_exactly(t, err, promise);

    // Using the same aborted controller will get the same error as well.
    const anotherPromise = method(controller.signal);
    await promise_rejects_exactly(t, err, anotherPromise);
  }
}
