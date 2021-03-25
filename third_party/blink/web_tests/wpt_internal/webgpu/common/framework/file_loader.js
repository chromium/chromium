/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { parseQuery } from './query/parseQuery.js';

import { loadTreeForQuery } from './tree.js';
import { assert } from './util/util.js';

// A listing file, e.g. either of:
// - `src/webgpu/listing.ts` (which is dynamically computed, has a Promise<TestSuiteListing>)
// - `out/webgpu/listing.js` (which is pre-baked, has a TestSuiteListing)

// Base class for DefaultTestFileLoader and FakeTestFileLoader.
export class TestFileLoader {
  importSpecFile(suite, path) {
    return this.import(`${suite}/${path.join('/')}.spec.js`);
  }

  async loadTree(query, subqueriesToExpand = []) {
    return loadTreeForQuery(
      this,
      query,
      subqueriesToExpand.map(s => {
        const q = parseQuery(s);
        assert(q.level >= 2, () => `subqueriesToExpand entries should not be multi-file:\n  ${q}`);
        return q;
      })
    );
  }

  async loadCases(query) {
    const tree = await this.loadTree(query);
    return tree.iterateLeaves();
  }
}

export class DefaultTestFileLoader extends TestFileLoader {
  async listing(suite) {
    return (await import(`../../${suite}/listing.js`)).listing;
  }

  import(path) {
    return import(`../../${path}`);
  }
}
