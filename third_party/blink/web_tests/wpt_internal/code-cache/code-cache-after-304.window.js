const testScriptUrl = 'resources/cache.py';

async function loadScriptInMainFrame(name) {
  // This is needed to disable caches and force revalidation.
  await fetch(name);

  const script = document.createElement('script');
  script.src = name;
  const p = new Promise((resolve, reject) => {
    script.onload = () => { resolve(); };
    script.onerror = e => { reject(e); };
  });
  document.head.appendChild(script);
  await p;
}

async function test() {
  // First script load.
  await loadScriptInMainFrame(testScriptUrl);

  // Verify the first script compilation.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the first compilation');
  assert_false(internals.lastCompiledScriptUsedCodeCache(document),
               'Did not use the code cache after the first compilation');

  // Second script load produces the code cache.
  await loadScriptInMainFrame(testScriptUrl);

  // Verify the second script compilation.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the second compilation');
  assert_false(internals.lastCompiledScriptUsedCodeCache(document),
               'Did not use the code cache after the second compilation');

  // Third script load uses the code cache.
  await loadScriptInMainFrame(testScriptUrl);

  // Verify that the code cache was used.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the third compilation');
  assert_true(internals.lastCompiledScriptUsedCodeCache(document),
              'Did use the code cache after the third compilation');
}

promise_test(test);
