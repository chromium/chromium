// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Rollup configuration to bunle Lit using a custom plugin.
 */

import * as path from 'path';
import * as url from 'url';

// Plugin for Rollup to resolve bare imports when bundling LitElement.
function plugin() {
  // The aboslute path of the directory where the current file resides.
  const hereDir = url.fileURLToPath(new URL('.', import.meta.url));

  // The absolute path to third_party/node/node_modules/.
  const nodeModulesDir = path.join(hereDir, '../../node/node_modules');

  // Relative path from the current working directory to
  // third_party/node/node_modules/.
  const pathToNodeModules = path.relative(process.cwd(), nodeModulesDir);

  // URL mappings from bare import URLs to file paths.
  const redirects = new Map([
    ['lit/index.js', path.join(pathToNodeModules, 'lit/index.js')],
    ['lit-element/lit-element.js',
      path.join(pathToNodeModules, 'lit-element/lit-element.js')],
    ['lit-html/is-server.js',
      path.join(pathToNodeModules, 'lit-html/is-server.js')],
    ['lit-html', path.join(pathToNodeModules, 'lit-html/lit-html.js')],
    ['@lit/reactive-element',
      path.join(pathToNodeModules, '@lit/reactive-element/reactive-element.js')],
  ]);

  return {
    name: 'lit-path-resolver-plugin',

    resolveId(source, origin) {
      if (source.startsWith('.')) {
        // Let Rollup handle relative paths.
        return null;
      }

      if (origin === undefined) {
        return null;
      }

      const redirect = redirects.get(source) || null;
      if (redirect) {
        return redirect;
      }

      throw new Error(`Unresolved import '${source}' in '${origin}'`);
      return null;
    },
  };
}

export default ({
  plugins: [plugin()],
});
