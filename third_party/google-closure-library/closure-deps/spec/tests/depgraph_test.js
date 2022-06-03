/**
 * @license
 * Copyright 2018 The Closure Library Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const depGraph = require('../../lib/depgraph');

/**
 * @param {...!depGraph.Dependency} deps
 * @return {?}
 */
function invalidCycleWith(...deps) {
  return {
    asymmetricMatch(value) {
      if (!(value instanceof depGraph.InvalidCycleError)) {
        expect(value).toBe(jasmine.any(depGraph.InvalidCycleError));
        return false;
      }
      for (const dep of deps) {
        if (value.deps.indexOf(dep) < 0) {
          expect(value.deps).toContain(dep);
          return false;
        }
      }
      expect(value.deps.length).toBe(deps.length);
      return value.deps.length === deps.length;
    },

    jasmineToString() {
      return `Invalid cycle with: ${deps.map(d => d.path).join(', ')}.`;
    },
  };
}

/**
 * @param {!Array<!depGraph.Dependency>} dependencies
 * @param {!depGraph.ModuleResolver=} moduleResolver
 * @return {!depGraph.Graph}
 */
function makeValidatedGraph(dependencies, resolver = undefined) {
  const g = new depGraph.Graph(dependencies, resolver);
  g.validate();
  return g;
}

describe('depgraph', function() {
  describe('single', function() {
    it('should accept closure provided file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'path', ['my.example'], []);
      const g = makeValidatedGraph([d]);
      expect(g.order(d)).toEqual([d]);
    });

    it('should accept closure module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'path', ['my.example'], []);
      const g = makeValidatedGraph([d]);
      expect(g.order(d)).toEqual([d]);
    });

    it('should accept es6 module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'path', [], []);
      const g = makeValidatedGraph([d]);
      expect(g.order(d)).toEqual([d]);
    });
  });

  describe('closure provide can require', function() {
    it('closure provided file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('closure module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('es6 module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'required', ['my.required'], []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });
  });

  describe('closure module can require', function() {
    it('closure provided file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('closure module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('es6 module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'required', ['my.required'], []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });
  });

  describe('es6 module requires', function() {
    it('closure provided file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('closure module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, 'required', ['my.required'],
          []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });

    it('es6 module file', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'example', ['my.example'],
          [new depGraph.GoogRequire('my.required')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'required', ['my.required'], []);
      expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
      expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
    });
  });

  describe('es6 imports', function() {
    it('closure provided file is error', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'example', [],
          [new depGraph.Es6Import('/required.js')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, '/required.js',
          ['my.required'], []);
      const g = new depGraph.Graph([d, r]);
      expect(() => g.validate()).toThrow();
    });

    it('closure module file is error', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, 'example', [],
          [new depGraph.Es6Import('/required.js')]);
      const r = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/required.js',
          ['my.required'], []);
      const g = new depGraph.Graph([d, r]);
      expect(() => g.validate()).toThrow();
    });

    describe('es6 module', function() {
      it('absolute path', function() {
        const d = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, 'example', [],
            [new depGraph.Es6Import('/required.js')]);
        const r = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, '/required.js', [], []);
        expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
        expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
      });

      it('relative path', function() {
        const d = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, '/foo/bar/example.js', [],
            [new depGraph.Es6Import('../baz/required.js')]);
        const r = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, '/foo/baz/required.js', [], []);
        expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
        expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
      });

      it('custom id', function() {
        const resolver = new (class extends depGraph.ModuleResolver {
          resolve(from, to) {
            expect(from).toEqual('/example.js');
            expect(to).toEqual('@wacky+id');
            return '/required.js';
          }
        })();
        const d = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, '/example.js', [],
            [new depGraph.Es6Import('@wacky+id')]);
        const r = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, '/required.js', [], []);
        expect(makeValidatedGraph([d, r], resolver).order(d)).toEqual([r, d]);
        expect(makeValidatedGraph([r, d], resolver).order(d)).toEqual([r, d]);
      });

      it('ambiguous input path', function() {
        const d = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, 'foo/bar/example.js', [],
            [new depGraph.Es6Import('../baz/required.js')]);
        const r = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, 'foo/baz/required.js', [], []);
        expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
        expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
      });

      it('relative input path', function() {
        const d = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, './foo/bar/example.js', [],
            [new depGraph.Es6Import('../baz/required.js')]);
        const r = new depGraph.Dependency(
            depGraph.DependencyType.ES6_MODULE, './foo/baz/required.js', [], []);
        expect(makeValidatedGraph([d, r]).order(d)).toEqual([r, d]);
        expect(makeValidatedGraph([r, d]).order(d)).toEqual([r, d]);
      });
    });
  });

  describe('circular', function() {
    it('two es6 modules is okay', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, '/a.js', [],
          [new depGraph.Es6Import('/b.js')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, '/b.js', [],
          [new depGraph.Es6Import('/a.js')]);

      const g1 = makeValidatedGraph([a, b]);
      const g2 = makeValidatedGraph([b, a]);

      expect(g1.order(a)).toEqual([b, a]);
      expect(g1.order(a)).toEqual([b, a]);
      expect(g2.order(b)).toEqual([a, b]);
      expect(g2.order(b)).toEqual([a, b]);
    });

    it('two closure provides is error', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, '/a.js', ['a'],
          [new depGraph.GoogRequire('b')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, '/b.js', ['b'],
          [new depGraph.GoogRequire('a')]);

      const g = new depGraph.Graph([a, b]);
      expect(() => g.validate()).toThrow();
    });

    it('two closure modules is error', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/a.js', ['a'],
          [new depGraph.GoogRequire('b')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/b.js', ['b'],
          [new depGraph.GoogRequire('a')]);

      const g = new depGraph.Graph([a, b]);
      expect(() => g.validate()).toThrow();
    });

    it('closure provide and module is error', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_PROVIDE, '/a.js', ['a'],
          [new depGraph.GoogRequire('b')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/b.js', ['b'],
          [new depGraph.GoogRequire('a')]);


      let g = new depGraph.Graph([a, b]);
      expect(() => g.validate()).toThrow();

      g = new depGraph.Graph([b, a]);
      expect(() => g.validate()).toThrow();
    });

    it('closure and es6 cycle is error', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, '/a.js', ['a'],
          [new depGraph.GoogRequire('b')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/b.js', ['b'],
          [new depGraph.GoogRequire('a')]);

      let g = new depGraph.Graph([a, b]);
      expect(() => g.validate()).toThrow(invalidCycleWith(a, b));

      g = new depGraph.Graph([b, a]);
      expect(() => g.validate()).toThrow(invalidCycleWith(a, b));
    });

    it('closure between es6 cycle is error', function() {
      const a = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, '/a.js', ['a'],
          [new depGraph.GoogRequire('b')]);
      const b = new depGraph.Dependency(
          depGraph.DependencyType.CLOSURE_MODULE, '/b.js', ['b'],
          [new depGraph.GoogRequire('c')]);
      const c = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, '/c.js', ['c'],
          [new depGraph.Es6Import('/a.js')]);

      const g = new depGraph.Graph([a, b, c]);
      expect(() => g.validate()).toThrow(invalidCycleWith(a, b, c));
    });
  });

  it('closure strongly connected with es6 is error', function() {
    // a <------> b  <--- d
    // |          ^
    // ---> c ----|
    // Assume a and b are es6 but c is closure.
    // Normal DFS cycle detection cannot catch this case.
    // If the DFS order is a, c, b then it does not look like c
    // is in a cycle. So instead we test for strongly connected
    // components.
    const a = new depGraph.Dependency(
        depGraph.DependencyType.ES6_MODULE, '/a.js', ['a'],
        [new depGraph.Es6Import('/b.js'), new depGraph.GoogRequire('c')]);
    const b = new depGraph.Dependency(
        depGraph.DependencyType.ES6_MODULE, '/b.js', ['b'],
        [new depGraph.Es6Import('/a.js')]);
    const c = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_MODULE, '/c.js', ['c'],
        [new depGraph.GoogRequire('b')]);
    const d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_MODULE, '/d.js', ['d'],
        [new depGraph.GoogRequire('b')]);

    let g = new depGraph.Graph([a, b, c]);
    expect(() => g.validate()).toThrow(invalidCycleWith(a, b, c));
    g = new depGraph.Graph([a, b, c, d]);
    expect(() => g.validate()).toThrow(invalidCycleWith(a, b, c));
  });
});
