// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const fs = require('fs');
const path = require('path');
const { JSDOM } = require('jsdom');
const vm = require('vm');

if (process.argv.length < 3) {
  console.error("Usage: node run_readability.cjs <input.html> [output.html]");
  process.exit(1);
}

const inputFile = process.argv[2];
const outputFile = process.argv[3]; // Optional

try {
  const code = fs.readFileSync(path.join(__dirname, 'Readability.js'), 'utf8');
  const html = fs.readFileSync(inputFile, 'utf8');

  const dom = new JSDOM(html, { url: "http://fakehost/test/page.html" });
  const { window } = dom;

  // Readability expects a global-ish environment or it uses its own 'this' if
  // it's in a function. The script ends with module.exports = Readability if
  // module is an object.
  const script = new vm.Script(code);
  const context = {
    console: console,
    module: { exports: {} },
    URL: window.URL,
    CSS: { escape: (id) => id.replace(/\./g, '\\.') },
  };
  script.runInNewContext(context);
  const Readability = context.module.exports;

  const reader = new Readability(window.document, { debug: true });
  const article = reader.parse();

  if (article) {
    if (outputFile) {
      fs.writeFileSync(outputFile, article.content);
      console.log(`Successfully wrote article content to ${outputFile}`);
    } else {
      console.log("--- RESULT ---");
      console.log(article.content);
      console.log("--------------");
    }
    console.log("Title:", article.title);
    console.log("Length:", article.content.length);
  } else {
    console.log("No article found.");
  }
} catch (error) {
  console.error("Error running Readability:", error);
  process.exit(1);
}
