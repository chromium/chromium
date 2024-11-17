const testScriptUrl = 'resources/cache.py';

async function loadScriptInMainFrame(name, bustMemoryCache) {
  if (bustMemoryCache) {
    await fetch(name);
  }

  const script = document.createElement('script');
  script.src = name;
  const p = new Promise((resolve, reject) => {
    script.onload = resolve;
    script.onerror = reject;
  });
  document.head.appendChild(script);
  await p;
}

async function test() {
  // First script load.
  await loadScriptInMainFrame(testScriptUrl, false);

  // Verify the first script compilation.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the first compilation');
  assert_false(internals.lastCompiledScriptUsedCodeCache(document),
               'Did not use the code cache after the first compilation');

  // Second script load produces the code cache.
  await loadScriptInMainFrame(testScriptUrl, false);

  // Verify the second script compilation.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the second compilation');
  assert_false(internals.lastCompiledScriptUsedCodeCache(document),
               'Did not use the code cache after the second compilation');

  // Third script load uses the code cache. Set bustMemoryCache = true to force resource
  // revalidation, so we trigger the code path for HTTP 304 handling.
  await loadScriptInMainFrame(testScriptUrl, true);

  // Verify that the code cache was used.
  assert_true(internals.lastCompiledScriptFileName(document).endsWith(testScriptUrl),
              'Last compiled script name after the third compilation');
  assert_true(internals.lastCompiledScriptUsedCodeCache(document),
              'Did use the code cache after the third compilation');
}

promise_test(test);
