// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptPolicy: TrustedTypePolicy =
    window.trustedTypes!.createPolicy('webui-test-script', {
      createHTML: () => '',
      createScriptURL: urlString => {
        // Ensure this is a scheme-relative URL requested by a chrome or
        // chrome-untrusted host.
        const url = new URL(window.location.href);
        if (['chrome:', 'chrome-untrusted:'].includes(url.protocol) &&
            urlString.startsWith('//')) {
          return urlString;
        }

        console.error(`Invalid test URL ${urlString} found.`);
        return '';
      },
      createScript: () => '',
    });

// Note: Do not export this method, it is only meant to be used within this
// module, otherwise the fairly loose scriptPolicy above would be exposed.
function loadScript(url: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = scriptPolicy.createScriptURL(url) as unknown as string;
    script.onerror = function() {
      reject(new Error(`test_loader_util: Failed to load ${url}`));
    };
    script.onload = function() {
      resolve();
    };
    document.body.appendChild(script);
  });
}

/**
 * @return Whether a test module was loaded.
 *   - In case where a module was not specified, returns false (used for
 *     providing a way for UIs to wait for any test initialization, if run
 *     within the context of a test).
 *   - In case where loading failed (probably incorrect URL) a rejected Promise
 *     is returned.
 */
export async function loadTestModule(): Promise<boolean> {
  const params = new URLSearchParams(window.location.search);
  const module = params.get('module');
  if (!module) {
    return Promise.resolve(false);
  }

  await loadScript(`//webui-test/${module}`);
  return Promise.resolve(true);
}

export async function loadMochaAdapter(): Promise<boolean> {
  const params = new URLSearchParams(window.location.search);
  const adapter = params.get('adapter') || 'mocha_adapter.js';
  if (!['mocha_adapter.js', 'mocha_adapter_simple.js'].includes(adapter)) {
    return Promise.reject(new Error(`Invalid adapter=${adapter} parameter`));
  }

  await loadScript(`//webui-test/${adapter}`);
  return Promise.resolve(true);
}
