// META: script=resources/workaround-for-362676838.js
// META: script=resources/utils.js

promise_test(async (t) => {
  testAbort(t, (signal) => {
    return ai.summarizer.create({
      signal: signal
    });
  });
}, 'Aborting AISummarizerFactory.create().');

promise_test(async (t) => {
  const summarizer = await ai.summarizer.create();
  testAbort(t, (signal) => {
    return summarizer.summarize(
      "Minccino is a furry, gray chinchilla-like Pokémon with scruffs of fur on its head and neck.",
      { signal: signal }
    );
  });
}, 'Aborting AISummarizer.summarize().');
