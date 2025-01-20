// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
promise_test(async t => {
  await testAbortPromise(t, signal => {
    return ai.languageModel.create({
      signal: signal
    });
  });
}, "Aborting AILanguageModelFactory.create().");

promise_test(async t => {
  const session = await ai.languageModel.create();
  await testAbortPromise(t, signal => {
    return session.clone({
      signal: signal
    });
  });
}, "Aborting AILanguageModel.clone().");

promise_test(async t => {
  const session = await ai.languageModel.create();
  await testAbortPromise(t, signal => {
    return session.prompt(kTestPrompt, { signal: signal });
  });
}, "Aborting AILanguageModel.prompt().");

promise_test(async t => {
  const session = await ai.languageModel.create();
  await testAbortReadableStream(t, signal => {
    return session.promptStreaming(
      kTestPrompt, { signal: signal }
    );
  });
}, "Aborting AILanguageModel.promptStreaming().");
