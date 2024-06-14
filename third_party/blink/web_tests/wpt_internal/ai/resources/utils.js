const testPromptAPI = async () => {
  if (!ai) {
    return {
      success: false,
      error: "ai is not defined in the scope"
    };
  }

  try {
    const canCreate = await ai.canCreateTextSession();
    if (canCreate !== "readily") {
      return {
        success: false,
        error: "cannot create text session"
      };
    }

    const session = await ai.createTextSession();
    const result = await session.prompt("What is the result of 0*2?");
    if (typeof result !== "string" || result.length === 0) {
      return {
        success: false,
        error: "the prompt API doesn't receive any response"
      };
    }

    return {
      success: true
    };
  } catch (e) {
    return {
      success: false,
      error: e
    };
  }
}
