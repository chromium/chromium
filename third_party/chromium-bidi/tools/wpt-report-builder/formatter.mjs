/**
 * Copyright 2023 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
export function escapeHtml(str) {
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
    .replace(/\//g, '&#47;');
}

export function flattenSingleTest(test) {
  if (test.status !== 'OK' && test.subtests.length === 0) {
    return [
      {
        path: test.test,
        name: null,
        status: test.status,
        message: test.message ?? null,
      },
    ];
  }

  return test.subtests.map((subtest) => ({
    path: `${test.test}/${escapeHtml(subtest.name)}`,
    name: subtest.name,
    status: subtest.status,
    message: subtest.message ?? null,
  }));
}

/**
 *  "Tentative" means a WPT test was written even though consensus was never
 *  reached in the spec whether it will actually take place.
 *
 *  Tentative tests will usually never pass and they are not final.
 *  Since these are not spec finalized, they only add noise to the report.
 *
 *  Once (if ever) consensus is reached upon upstream, the tentative suffix
 *  is removed from the WPT test file, then the tests appear in the report
 *  as usual.
 */
export function removeWebDriverBiDiPrefix(name) {
  return name.replace('/webdriver/tests/bidi/', '');
}

export function excludeTentativeTests(test) {
  return !test.test.includes('_tentative.py');
}

export function flattenTests(report) {
  return report.results
    .filter(excludeTentativeTests)
    .map(flattenSingleTest)
    .flat()
    .sort(
      (a, b) =>
        a.path?.localeCompare(b.path) || a.name?.localeCompare(b.name) || 0
    );
}

export function groupTests(tests) {
  const pathMap = {
    group: '',
    children: new Map(),
    stat: {
      all: 0,
      pass: 0,
    },
  };

  for (const test of tests) {
    let currentPathMap = pathMap;
    const parts = test.path.split('/');
    if (parts[0] === '') {
      parts.shift();
    }

    let currentPath = '';
    for (const part of parts) {
      currentPath = `${currentPath}/${part}`;
      if (!currentPathMap.children.has(part)) {
        currentPathMap.children.set(part, {
          group: currentPath,
          children: new Map(),
          stat: {
            all: 0,
            pass: 0,
          },
        });
      }
      currentPathMap = currentPathMap.children.get(part);

      currentPathMap.stat.all++;
      currentPathMap.stat.pass += test.status === 'PASS' ? 1 : 0;
    }
    currentPathMap.test = test;
  }

  return mergeSingleChildren(pathMap);
}

function mergeSingleChildren(map) {
  // If this is a leaf node, return the test.
  if (map.test) {
    return map.test;
  }

  if (map.children?.size === 1) {
    const child = map.children.values().next().value;
    // Don't collapse leaf node into parent.
    if (!child.test) {
      return mergeSingleChildren(child);
    }
  }

  // Recursively flatten children.
  return {
    message: null,
    path: map.group,
    name: null,
    status: null,
    stat: map.stat,
    children: Array.from(map.children.values()).map(mergeSingleChildren),
  };
}

export function generateHtml(map) {
  const date = new Date().toISOString().slice(0, 'yyyy-mm-dd'.length);

  return `
    <!DOCTYPE html>
    <html lang="en">
    <meta charset="utf-8">
    <title>BiDi-CDP Mapper</title>
    <style>
      body { font-family: Roboto, serif; font-size: 13px; color: #202124; }
      .path { font-family: Menlo, Consolas, Monaco, Liberation Mono, Lucida Console, monospace; line-height: 180%; padding: 5px 18px; margin: 0; }
      .top { box-shadow: 0 1px 4px rgba(0, 0, 0, 0.15), 0 1px 6px rgba(0, 0, 0, 0.2); border-radius: 8px; margin: auto; padding: 60px; max-width: 1200px; }
      .test-card { padding-left: 20px; max-width: 1200px; }
      .divider { margin-left: 20px; height: 1px; background: #a0a0a0; }
      .non-collapsible-item { padding-left: 27px; padding-right: 15px; }
      .stat { float: right }
      .pass { background: #D5F2D7; }
      .part { background: #F2EDD5; }
      .fail { background: #F2D7D5; }
      .hidden { display: none; }

      .header {
        display: flex;
        align-items: center;
      }

      .headings {
        flex-grow: 1;
      }
    </style>
    <div class="top">
      <div class="header">
        <div class="headings">
          <h1>WPT test results (${date})</h1>
          <h2>${map.stat.pass} / ${map.stat.all} (${
            map.stat.all - map.stat.pass
          } remaining)</h2>
        </div>
        <div>
          <button class="toggle" type="button">Long</button>
        </div>
      </div>
      <div>
        ${Array.from(map.children.values())
          .map((t) => generateTestReport(t, map.path))
          .join('')}
      </div>
      <script>
        const button = document.querySelector('.toggle');
        let state = 'short';
        button.addEventListener('click', () => {
          button.innerText = state.charAt(0).toUpperCase() + state.slice(1);

          const toHide = document.querySelectorAll(\`.\${state}-name\`);
          for(const element of toHide){
            element.classList.add("hidden");
          }

          state = state === 'short' ? 'long' : 'short';
          const toShow = document.querySelectorAll(\`.\${state}-name\`);
          for(const element of toShow){
            element.classList.remove("hidden");
          }
        })
      </script>
    </div>`;
}

function generateTestReport(map, parent) {
  if (!map.children) {
    return generateSubtestReport(map);
  }

  let name = removeWebDriverBiDiPrefix(map.path.replace(parent.path, ''));
  if (name.startsWith('/')) {
    name = name.replace('/', '');
  }

  return `
    <div class="divider"></div>
    <div class="test-card">
      <details>
        <summary class="path ${
          map.stat.all === map.stat.pass
            ? 'pass'
            : map.stat.pass === 0
              ? 'fail'
              : 'part'
        }">
          <span class="short-name">${escapeHtml(name)}</span>
          <span class="long-name hidden">${escapeHtml(map.path)}</span>
          <span class="stat"><b>${map.stat.pass}/${map.stat.all}</b></span>
        </summary>
        ${map.children
          .map((child) => {
            return generateTestReport(child, map);
          })
          .join('')}
      </details>
    </div>`;
}

function generateSubtestReport(subtest) {
  const name =
    subtest.name ?? removeWebDriverBiDiPrefix(subtest.path).split('/').at(-1);
  return `<div class="divider"></div>
      <div class="test-card">
        <p class="non-collapsible-item path ${
          subtest.status === 'PASS' ? 'pass' : 'fail'
        }">
          <span class="short-name">${escapeHtml(name)}</span>
          <span class="long-name hidden">${escapeHtml(subtest.path)}</span>
          ${
            subtest.message
              ? `<br /><small>${escapeHtml(subtest.message)}</small>`
              : ''
          }
          <span class="stat"><b>${escapeHtml(subtest.status)}</b></span>
        </p>
      </div>`;
}

export function generateReport(rawReport) {
  return generateHtml(groupTests(flattenTests(rawReport)));
}
