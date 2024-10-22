promise_test(async (t) => {
  const controller = new AbortController();
  const createPromise = ai.summarizer.create({signal: controller.signal});
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', createPromise);

  // Using an aborted controller will get the `AbortError` as well.
  const anotherCreatePromise = ai.summarizer.create({ signal: controller.signal });
  await promise_rejects_dom(t, 'AbortError', anotherCreatePromise);
}, 'Aborting AISummarizerFactory.create().');

promise_test(async (t) => {
  const summarizer = await ai.summarizer.create();
  const controller = new AbortController();
  const summarizerPromise = summarizer.summarize('hello', {signal: controller.signal});
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', summarizerPromise);
}, 'Aborting AISummarizer.summarize().');
