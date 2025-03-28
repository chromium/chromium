// Ensure that the language model is available.
// TODO(crbug.com/382640509): Remove this when `LanguageModel.create()` can
// resolve even when it is called soon after the browser start up.
promise_test(async () => {
  // There is an issue that `LanguageModel.create()` doesn't resolve forever if
  // called soon after the browser start up. To avoid this issue, we call
  // `LanguageModel.create()` multiple times.
  while (true) {
    const result = await new Promise(resolve => {
      LanguageModel.create().then((model) => {
        model.destroy();
        resolve('Created');
      }).catch((e) => {
        console.error(e.toString());
        resolve('Failed to create a language model.');
      });
      setTimeout(() => { resolve('Timeout'); }, 1000);
    });
    if (result === 'Created') {
      return;
    }
  }
}, 'A workaround step for crbug.com/382640509');
