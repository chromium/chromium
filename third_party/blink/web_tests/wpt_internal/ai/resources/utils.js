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
    const capabilities = await ai.assistant.capabilities();
    const status = capabilities.available;
    if (status !== "readily") {
      return {
        success: false,
        error: "cannot create text session"
      };
    }

    const session = await ai.assistant.create({
      topK: 3,
      temperature: 0.8,
      systemPrompt: "Let's talk about Mauritius."
    });
    return testSession(session);
  } catch (e) {
    return {
      success: false,
      error: e
    };
  }
};
