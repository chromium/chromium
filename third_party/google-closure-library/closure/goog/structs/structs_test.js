/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structsTest');
goog.setTestOnly();

const StructsMap = goog.require('goog.structs.Map');
const StructsSet = goog.require('goog.structs.Set');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

/*

 This one does not test Map or Set
 It tests Array, Object, String and a NodeList

*/

function stringifyObject(obj) {
  const sb = [];
  for (let key in obj) {
    sb.push(key + obj[key]);
  }
  return sb.join('');
}

function getTestElement() {
  return document.getElementById('test');
}

function getAll() {
  return getTestElement().getElementsByTagName('*');
}

let node;

function addNode() {
  node = dom.createElement(TagName.SPAN);
  getTestElement().appendChild(node);
}

function removeNode() {
  getTestElement().removeChild(node);
}

function nodeNames(nl) {
  const sb = [];
  for (let i = 0, n; n = nl[i]; i++) {
    sb.push(n.nodeName);
  }
  return sb.join(',');
}

const allTagNames1 = 'HR,P,P,P,P,P,P,P,P,H1';
const allTagNames2 = `${allTagNames1},SPAN`;

// Map

// Filter

// Some

// Every

// For each

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetCount() {
    const arr = ['a', 'b', 'c'];
    assertEquals('count, should be 3', 3, structs.getCount(arr));
    arr.push('d');
    assertEquals('count, should be 4', 4, structs.getCount(arr));
    googArray.remove(arr, 'd');
    assertEquals('count, should be 3', 3, structs.getCount(arr));

    const obj = {a: 0, b: 1, c: 2};
    assertEquals('count, should be 3', 3, structs.getCount(obj));
    obj.d = 3;
    assertEquals('count, should be 4', 4, structs.getCount(obj));
    delete obj.d;
    assertEquals('count, should be 3', 3, structs.getCount(obj));

    let s = 'abc';
    assertEquals('count, should be 3', 3, structs.getCount(s));
    s += 'd';
    assertEquals('count, should be 4', 4, structs.getCount(s));

    const all = getAll();
    assertEquals('count, should be 10', 10, structs.getCount(all));
    addNode();
    assertEquals('count, should be 11', 11, structs.getCount(all));
    removeNode();
    assertEquals('count, should be 10', 10, structs.getCount(all));

    const aMap = new StructsMap({a: 0, b: 1, c: 2});
    assertEquals('count, should be 3', 3, structs.getCount(aMap));
    aMap.set('d', 3);
    assertEquals('count, should be 4', 4, structs.getCount(aMap));
    aMap.remove('a');
    assertEquals('count, should be 3', 3, structs.getCount(aMap));

    /** @suppress {checkTypes} suppression added to enable type checking */
    const aSet = new StructsSet('abc');
    assertEquals('count, should be 3', 3, structs.getCount(aSet));
    aSet.add('d');
    assertEquals('count, should be 4', 4, structs.getCount(aSet));
    aSet.remove('a');
    assertEquals('count, should be 3', 3, structs.getCount(aSet));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetValues() {
    const arr = ['a', 'b', 'c', 'd'];
    assertEquals('abcd', structs.getValues(arr).join(''));

    const obj = {a: 0, b: 1, c: 2, d: 3};
    assertEquals('0123', structs.getValues(obj).join(''));

    let s = 'abc';
    assertEquals('abc', structs.getValues(s).join(''));
    s += 'd';
    assertEquals('abcd', structs.getValues(s).join(''));

    const all = getAll();
    assertEquals(allTagNames1, nodeNames(structs.getValues(all)));
    addNode();
    assertEquals(allTagNames2, nodeNames(structs.getValues(all)));
    removeNode();
    assertEquals(allTagNames1, nodeNames(structs.getValues(all)));

    const aMap = new StructsMap({a: 1, b: 2, c: 3});
    assertEquals('123', structs.getValues(aMap).join(''));
    const nativeMap = new Map([['a', 1], ['b', 2], ['c', 3]]);
    assertEquals('123', structs.getValues(nativeMap).join(''));

    const aSet = new StructsSet([1, 2, 3]);
    assertEquals('123', structs.getValues(aSet).join(''));
    const nativeSet = new Set([1, 2, 3]);
    assertEquals('123', structs.getValues(nativeSet).join(''));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetKeys() {
    const arr = ['a', 'b', 'c', 'd'];
    assertEquals('0123', structs.getKeys(arr).join(''));

    const obj = {a: 0, b: 1, c: 2, d: 3};
    assertEquals('abcd', structs.getKeys(obj).join(''));

    let s = 'abc';
    assertEquals('012', structs.getKeys(s).join(''));
    s += 'd';
    assertEquals('0123', structs.getKeys(s).join(''));

    const all = getAll();
    assertEquals('0123456789', structs.getKeys(all).join(''));
    addNode();
    assertEquals('012345678910', structs.getKeys(all).join(''));
    removeNode();
    assertEquals('0123456789', structs.getKeys(all).join(''));

    const aMap = new StructsMap({a: 1, b: 2, c: 3});
    assertEquals('abc', structs.getKeys(aMap).join(''));
    const nativeMap = new Map([['a', 1], ['b', 2], ['c', 3]]);
    assertEquals('abc', structs.getKeys(nativeMap).join(''));

    const aSet = new StructsSet([1, 2, 3]);
    assertUndefined(structs.getKeys(aSet));
    const nativeSet = new Set([1, 2, 3]);
    assertUndefined(structs.getKeys(nativeSet));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testContains() {
    const arr = ['a', 'b', 'c', 'd'];
    assertTrue('contains, Should contain \'a\'', structs.contains(arr, 'a'));
    assertFalse(
        'contains, Should not contain \'e\'', structs.contains(arr, 'e'));

    const obj = {a: 0, b: 1, c: 2, d: 3};
    assertTrue('contains, Should contain \'0\'', structs.contains(obj, 0));
    assertFalse('contains, Should not contain \'4\'', structs.contains(obj, 4));

    const s = 'abc';
    assertTrue('contains, Should contain \'a\'', structs.contains(s, 'a'));
    assertFalse('contains, Should not contain \'d\'', structs.contains(s, 'd'));

    const all = getAll();
    assertTrue(
        'contains, Should contain \'h1\'',
        structs.contains(all, document.getElementById('h1')));
    assertFalse(
        'contains, Should not contain \'document.body\'',
        structs.contains(all, document.body));

    const aMap = new StructsMap({a: 1, b: 2, c: 3});
    assertTrue('contains, Should contain \'1\'', structs.contains(aMap, 1));
    assertFalse(
        'contains, Should not contain \'4\'', structs.contains(aMap, 4));

    const aSet = new StructsSet([1, 2, 3]);
    assertTrue('contains, Should contain \'1\'', structs.contains(aSet, 1));
    assertFalse(
        'contains, Should not contain \'4\'', structs.contains(aSet, 4));
  },

  testClear() {
    const arr = ['a', 'b', 'c', 'd'];
    structs.clear(arr);
    assertTrue('cleared so it should be empty', structs.isEmpty(arr));
    assertFalse(
        'cleared so it should not contain \'a\'', structs.contains(arr, 'a'));

    const obj = {a: 0, b: 1, c: 2, d: 3};
    structs.clear(obj);
    assertTrue('cleared so it should be empty', structs.isEmpty(obj));
    assertFalse(
        'cleared so it should not contain \'a\' key', structs.contains(obj, 0));

    const aMap = new StructsMap({a: 1, b: 2, c: 3});
    structs.clear(aMap);
    assertTrue('cleared map so it should be empty', structs.isEmpty(aMap));
    assertFalse(
        'cleared map so it should not contain \'1\' value',
        structs.contains(aMap, 1));

    const aSet = new StructsSet([1, 2, 3]);
    structs.clear(aSet);
    assertTrue('cleared set so it should be empty', structs.isEmpty(aSet));
    assertFalse(
        'cleared set so it should not contain \'1\'',
        structs.contains(aSet, 1));

    // cannot clear a string or a NodeList
  },

  testMap() {
    const RV = {};
    const obj = {
      map: function(g) {
        assertEquals(f, g);
        assertEquals(this, obj);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.map(obj, f));
  },

  testMap2() {
    const THIS_OBJ = {};
    const RV = {};
    const obj = {
      map: function(g, obj2) {
        assertEquals(f, g);
        assertEquals(this, obj);
        assertEquals(THIS_OBJ, obj2);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.map(obj, f, THIS_OBJ));
  },

  testMapArrayLike() {
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v * v;
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f));
  },

  testMapArrayLike2() {
    const THIS_OBJ = {};
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v * v;
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f, THIS_OBJ));
  },

  testMapString() {
    const col = '012';
    function f(v, i, col2) {
      // The SpiderMonkey Array.map for strings turns the string into a String
      // so we cannot use assertEquals because it uses ===.
      assertTrue(col == col2);
      assertEquals('number', typeof i);
      return Number(v) * Number(v);
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f));
  },

  testMapString2() {
    const THIS_OBJ = {};
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return Number(v) * Number(v);
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f, THIS_OBJ));
  },

  testMapMap() {
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      return v * v;
    }
    assertObjectEquals({a: 0, b: 1, c: 4}, structs.map(col, f));
  },

  testMapMap2() {
    const THIS_OBJ = {};
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      assertEquals(THIS_OBJ, this);
      return v * v;
    }
    assertObjectEquals({a: 0, b: 1, c: 4}, structs.map(col, f, THIS_OBJ));
  },

  testMapSet() {
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      return v * v;
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f));
  },

  testMapSet2() {
    const THIS_OBJ = {};
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      assertEquals(THIS_OBJ, this);
      return v * v;
    }
    assertArrayEquals([0, 1, 4], structs.map(col, f, THIS_OBJ));
  },

  testMapNodeList() {
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v.tagName;
    }
    assertEquals('HRPPPPPPPPH1', structs.map(col, f).join(''));
  },

  testMapNodeList2() {
    const THIS_OBJ = {};
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v.tagName;
    }
    assertEquals('HRPPPPPPPPH1', structs.map(col, f, THIS_OBJ).join(''));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFilter() {
    const RV = {};
    const obj = {
      filter: function(g) {
        assertEquals(f, g);
        assertEquals(this, obj);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.filter(obj, f));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFilter2() {
    const THIS_OBJ = {};
    const RV = {};
    const obj = {
      filter: function(g, obj2) {
        assertEquals(f, g);
        assertEquals(this, obj);
        assertEquals(THIS_OBJ, obj2);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.filter(obj, f, THIS_OBJ));
  },

  testFilterArrayLike() {
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v > 0;
    }
    assertArrayEquals([1, 2], structs.filter(col, f));
  },

  testFilterArrayLike2() {
    const THIS_OBJ = {};
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v > 0;
    }
    assertArrayEquals([1, 2], structs.filter(col, f, THIS_OBJ));
  },

  testFilterString() {
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      return Number(v) > 0;
    }
    assertArrayEquals(['1', '2'], structs.filter(col, f));
  },

  testFilterString2() {
    const THIS_OBJ = {};
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return Number(v) > 0;
    }
    assertArrayEquals(['1', '2'], structs.filter(col, f, THIS_OBJ));
  },

  testFilterMap() {
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      return v > 0;
    }
    assertObjectEquals({b: 1, c: 2}, structs.filter(col, f));
  },

  testFilterMap2() {
    const THIS_OBJ = {};
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > 0;
    }
    assertObjectEquals({b: 1, c: 2}, structs.filter(col, f, THIS_OBJ));
  },

  testFilterSet() {
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      return v > 0;
    }
    assertArrayEquals([1, 2], structs.filter(col, f));
  },

  testFilterSet2() {
    const THIS_OBJ = {};
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > 0;
    }
    assertArrayEquals([1, 2], structs.filter(col, f, THIS_OBJ));
  },

  testFilterNodeList() {
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v.tagName == TagName.P;
    }
    assertEquals('P,P,P,P,P,P,P,P', nodeNames(structs.filter(col, f)));
  },

  testFilterNodeList2() {
    const THIS_OBJ = {};
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v.tagName == TagName.P;
    }
    assertEquals(
        'P,P,P,P,P,P,P,P', nodeNames(structs.filter(col, f, THIS_OBJ)));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSome() {
    const RV = {};
    const obj = {
      some: function(g) {
        assertEquals(f, g);
        assertEquals(this, obj);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.some(obj, f));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSome2() {
    const THIS_OBJ = {};
    const RV = {};
    const obj = {
      some: function(g, obj2) {
        assertEquals(f, g);
        assertEquals(this, obj);
        assertEquals(THIS_OBJ, obj2);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.some(obj, f, THIS_OBJ));
  },

  testSomeArrayLike() {
    let limit = 0;
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v > limit;
    }
    assertTrue(structs.some(col, f));
    limit = 2;
    assertFalse(structs.some(col, f));
  },

  testSomeArrayLike2() {
    const THIS_OBJ = {};
    let limit = 0;
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertTrue(structs.some(col, f, THIS_OBJ));
    limit = 2;
    assertFalse(structs.some(col, f, THIS_OBJ));
  },

  testSomeString() {
    let limit = 0;
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      return Number(v) > limit;
    }
    assertTrue(structs.some(col, f));
    limit = 2;
    assertFalse(structs.some(col, f));
  },

  testSomeString2() {
    const THIS_OBJ = {};
    let limit = 0;
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return Number(v) > limit;
    }
    assertTrue(structs.some(col, f, THIS_OBJ));
    limit = 2;
    assertFalse(structs.some(col, f, THIS_OBJ));
  },

  testSomeMap() {
    let limit = 0;
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      return v > limit;
    }
    assertObjectEquals(true, structs.some(col, f));
    limit = 2;
    assertFalse(structs.some(col, f));
  },

  testSomeMap2() {
    const THIS_OBJ = {};
    let limit = 0;
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertObjectEquals(true, structs.some(col, f, THIS_OBJ));
    limit = 2;
    assertFalse(structs.some(col, f, THIS_OBJ));
  },

  testSomeSet() {
    let limit = 0;
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      return v > limit;
    }
    assertTrue(structs.some(col, f));
    limit = 2;
    assertFalse(structs.some(col, f));
  },

  testSomeSet2() {
    const THIS_OBJ = {};
    let limit = 0;
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertTrue(structs.some(col, f, THIS_OBJ));
    limit = 2;
    assertFalse(structs.some(col, f, THIS_OBJ));
  },

  testSomeNodeList() {
    let tagName = TagName.P;
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v.tagName == tagName;
    }
    assertTrue(structs.some(col, f));
    tagName = 'MARQUEE';
    assertFalse(structs.some(col, f));
  },

  testSomeNodeList2() {
    const THIS_OBJ = {};
    let tagName = TagName.P;
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v.tagName == tagName;
    }
    assertTrue(structs.some(col, f, THIS_OBJ));
    tagName = 'MARQUEE';
    assertFalse(structs.some(col, f, THIS_OBJ));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEvery() {
    const RV = {};
    const obj = {
      every: function(g) {
        assertEquals(f, g);
        assertEquals(this, obj);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.every(obj, f));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEvery2() {
    const THIS_OBJ = {};
    const RV = {};
    const obj = {
      every: function(g, obj2) {
        assertEquals(f, g);
        assertEquals(this, obj);
        assertEquals(THIS_OBJ, obj2);
        return RV;
      },
    };
    function f() {}
    assertEquals(RV, structs.every(obj, f, THIS_OBJ));
  },

  testEveryArrayLike() {
    let limit = -1;
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v > limit;
    }
    assertTrue(structs.every(col, f));
    limit = 1;
    assertFalse(structs.every(col, f));
  },

  testEveryArrayLike2() {
    const THIS_OBJ = {};
    let limit = -1;
    const col = [0, 1, 2];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertTrue(structs.every(col, f, THIS_OBJ));
    limit = 1;
    assertFalse(structs.every(col, f, THIS_OBJ));
  },

  testEveryString() {
    let limit = -1;
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      return Number(v) > limit;
    }
    assertTrue(structs.every(col, f));
    limit = 1;
    assertFalse(structs.every(col, f));
  },

  testEveryString2() {
    const THIS_OBJ = {};
    let limit = -1;
    const col = '012';
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return Number(v) > limit;
    }
    assertTrue(structs.every(col, f, THIS_OBJ));
    limit = 1;
    assertFalse(structs.every(col, f, THIS_OBJ));
  },

  testEveryMap() {
    let limit = -1;
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      return v > limit;
    }
    assertObjectEquals(true, structs.every(col, f));
    limit = 1;
    assertFalse(structs.every(col, f));
  },

  testEveryMap2() {
    const THIS_OBJ = {};
    let limit = -1;
    const col = new StructsMap({a: 0, b: 1, c: 2});
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertObjectEquals(true, structs.every(col, f, THIS_OBJ));
    limit = 1;
    assertFalse(structs.every(col, f, THIS_OBJ));
  },

  testEverySet() {
    let limit = -1;
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      return v > limit;
    }
    assertTrue(structs.every(col, f));
    limit = 1;
    assertFalse(structs.every(col, f));
  },

  testEverySet2() {
    const THIS_OBJ = {};
    let limit = -1;
    const col = new StructsSet([0, 1, 2]);
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      assertEquals(THIS_OBJ, this);
      return v > limit;
    }
    assertTrue(structs.every(col, f, THIS_OBJ));
    limit = 1;
    assertFalse(structs.every(col, f, THIS_OBJ));
  },

  testEveryNodeList() {
    let nodeType = 1;  // ELEMENT
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      return v.nodeType == nodeType;
    }
    assertTrue(structs.every(col, f));
    nodeType = 3;  // TEXT
    assertFalse(structs.every(col, f));
  },

  testEveryNodeList2() {
    const THIS_OBJ = {};
    let nodeType = 1;  // ELEMENT
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      return v.nodeType == nodeType;
    }
    assertTrue(structs.every(col, f, THIS_OBJ));
    nodeType = 3;  // TEXT
    assertFalse(structs.every(col, f, THIS_OBJ));
  },

  testForEach() {
    let called = false;
    const obj = {
      forEach: function(g) {
        assertEquals(f, g);
        assertEquals(this, obj);
        called = true;
      },
    };
    function f() {}
    structs.forEach(obj, f);
    assertTrue(called);
  },

  testForEach2() {
    let called = false;
    const THIS_OBJ = {};
    const obj = {
      forEach: function(g, obj2) {
        assertEquals(f, g);
        assertEquals(this, obj);
        assertEquals(THIS_OBJ, obj2);
        called = true;
      },
    };
    function f() {}
    structs.forEach(obj, f, THIS_OBJ);
    assertTrue(called);
  },

  testForEachArrayLike() {
    const col = [0, 1, 2];
    const values = [];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      values.push(v * v);
    }
    structs.forEach(col, f);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachArrayLike2() {
    const THIS_OBJ = {};
    const col = [0, 1, 2];
    const values = [];
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      values.push(v * v);
    }
    structs.forEach(col, f, THIS_OBJ);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachString() {
    const col = '012';
    const values = [];
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      values.push(Number(v) * Number(v));
    }
    structs.forEach(col, f);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachString2() {
    const THIS_OBJ = {};
    const col = '012';
    const values = [];
    function f(v, i, col2) {
      // for some reason the strings are equal but identical???
      assertEquals(String(col), String(col2));
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      values.push(Number(v) * Number(v));
    }
    structs.forEach(col, f, THIS_OBJ);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachMap() {
    const col = new StructsMap({a: 0, b: 1, c: 2});
    const values = [];
    const keys = [];
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      values.push(v * v);
      keys.push(key);
    }
    structs.forEach(col, f);
    assertArrayEquals([0, 1, 4], values);
    assertArrayEquals(['a', 'b', 'c'], keys);
  },

  testForEachMap2() {
    const THIS_OBJ = {};
    const col = new StructsMap({a: 0, b: 1, c: 2});
    const values = [];
    const keys = [];
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('string', typeof key);
      assertEquals(THIS_OBJ, this);
      values.push(v * v);
      keys.push(key);
    }
    structs.forEach(col, f, THIS_OBJ);
    assertArrayEquals([0, 1, 4], values);
    assertArrayEquals(['a', 'b', 'c'], keys);
  },

  testForEachSet() {
    const col = new StructsSet([0, 1, 2]);
    const values = [];
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      values.push(v * v);
    }
    structs.forEach(col, f);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachSet2() {
    const THIS_OBJ = {};
    const col = new StructsSet([0, 1, 2]);
    const values = [];
    function f(v, key, col2) {
      assertEquals(col, col2);
      assertEquals('undefined', typeof key);
      assertEquals(THIS_OBJ, this);
      values.push(v * v);
    }
    structs.forEach(col, f, THIS_OBJ);
    assertArrayEquals([0, 1, 4], values);
  },

  testForEachNodeList() {
    const values = [];
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      values.push(v.tagName);
    }
    structs.forEach(col, f);
    assertEquals('HRPPPPPPPPH1', values.join(''));
  },

  testForEachNodeList2() {
    const THIS_OBJ = {};
    const values = [];
    const col = getAll();
    function f(v, i, col2) {
      assertEquals(col, col2);
      assertEquals('number', typeof i);
      assertEquals(THIS_OBJ, this);
      values.push(v.tagName);
    }
    structs.forEach(col, f, THIS_OBJ);
    assertEquals('HRPPPPPPPPH1', values.join(''));
  },
});
