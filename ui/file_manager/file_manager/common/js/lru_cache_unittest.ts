// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {LruCache} from './lru_cache.js';

export function testLruCache() {
  const cache = new LruCache<string>(3);

  // Querying by non-existent key will get null.
  assertEquals(null, cache.get('a'));

  // Add initial cache set.
  // Cached keys will be: MRU<-['c','b','a']->LRU
  cache.put('a', 'AAA');
  cache.put('b', 'BBB');
  cache.put('c', 'CCC');
  assertEquals(3, cache.size());

  // Reference failure doesn't change the order.
  assertEquals(null, cache.get('d'));

  // Now: MRU<-['c','b','a']->LRU

  // Putting new value will evict 'a'.
  assertEquals('AAA', cache.peek('a'));
  cache.put('d', 'DDD');
  assertEquals(null, cache.get('a'));

  // Now: MRU<-['d','c','b']->LRU

  // Referencing 'b' makes it to MRU.
  assertEquals('BBB', cache.get('b'));

  // Now: MRU<-['b','d','c']->LRU

  // Putting new key will evict 'c'.
  cache.put('e', 'EEE');
  assertEquals('BBB', cache.peek('b'));
  assertEquals(null, cache.peek('c'));

  // Now: MRU<-['e','b','d']->LRU

  // Putting new value for existing key will make it MRU.
  cache.put('b', 'newBBB');

  // Now: MRU<-['b','e','d']->LRU

  // Putting 2 new keys will evict 'e', 'd'.
  assertEquals('newBBB', cache.peek('b'));
  assertEquals('EEE', cache.peek('e'));
  assertEquals('DDD', cache.peek('d'));
  cache.put('f', 'FFF');
  cache.put('g', 'GGG');
  assertEquals(null, cache.peek('e'));
  assertEquals(null, cache.peek('d'));
  assertEquals('GGG', cache.peek('g'));
  assertEquals('FFF', cache.peek('f'));
  assertEquals('newBBB', cache.peek('b'));

  // Now: MRU<-['g', 'f', 'b']->LRU

  // Removing non-existent key won't change the size of cache.
  assertEquals(3, cache.size());
  cache.remove('a');
  assertEquals(3, cache.size());

  // Removing existent key will change the size of cache.
  cache.remove('g');
  assertEquals(2, cache.size());
  cache.remove('f');
  assertEquals(1, cache.size());
  cache.remove('b');
  assertEquals(0, cache.size());
}

export function testLRUCacheWithIndividualSizes() {
  const cache = new LruCache(10);

  // Querying by non-existent key will get null.
  assertEquals(null, cache.get('a'));

  // Add initial cache set.
  // Cached keys will be: MRU<-['c(4)','b(3)','a(2)']->LRU
  cache.put('a', 'AAA', 2);
  cache.put('b', 'BBB', 3);
  cache.put('c', 'CCC', 4);
  assertEquals(9, cache.size());

  // Make 'a' MRU.
  assertEquals('AAA', cache.get('a'));

  // Now: MRU<-['a(2)','c(4)','b(3)']->LRU

  // Adding small item will fit.
  cache.put('d', 'DDD');
  assertEquals('AAA', cache.peek('a'));
  assertEquals('BBB', cache.peek('b'));
  assertEquals(10, cache.size());

  // Now: MRU<-['d(1)','a(2)','c(4)','b(3)']->LRU

  // Adding an item whose size is 5 will evict c and b.
  assertEquals('BBB', cache.peek('b'));
  assertEquals('CCC', cache.peek('c'));
  cache.put('e', 'EEE', 5);
  assertEquals(null, cache.peek('b'));
  assertEquals(null, cache.peek('c'));
  assertEquals('AAA', cache.peek('a'));
  assertEquals('DDD', cache.peek('d'));
  assertEquals(8, cache.size());

  // Adding an item whose size is bigger than the max size will be ignored.
  cache.put('f', 'FFF', 11);
  assertEquals(null, cache.get('f'));
  assertEquals('AAA', cache.get('a'));
  assertEquals('DDD', cache.get('d'));
  assertEquals(8, cache.size());
}

class RandomNumberGenerator {
  private x_: number;
  constructor(seed: number) {
    this.x_ = seed;
  }

  random() {
    this.x_ = (32453 * this.x_ + 254119) % (1 << 24);
    return this.x_ >> 4;
  }
}

function generateRandom3letters(generator: RandomNumberGenerator) {
  const ALPHA = 'abcdefghijklmnopqrstuvwxyz';
  let res = '';
  for (let i = 0; i < 3; i++) {
    res += ALPHA[generator.random() % ALPHA.length];
  }
  return res;
}

export function testSizeCalculationByRandomInput() {
  const cache = new LruCache(10000);

  // We need fixed random number sequence to avoid test flakiness, so
  // RandomeNumberGenerator is used instead of Math.random() here.
  const generator = new RandomNumberGenerator(123456);

  // Make fixed set of keys.
  const keys = [];
  for (let i = 0; i < 1000; i++) {
    keys.push(generateRandom3letters(generator));
  }

  // Adding items won't make the cache's size exceed the max size.
  for (let i = 0; i < 10000; i++) {
    const size = generator.random() % 100 + 1;
    const key = keys[generator.random() % keys.length]!;
    cache.put(key, 'random item', size);

    assertTrue(cache.size() <= 10000);
  }

  // Removing all keys will make the cache's size exactly 0.
  for (const key of keys) {
    cache.remove(key);
  }
  assertEquals(0, cache.size());
}

export function testSetMaxSize() {
  const cache = new LruCache(10);
  cache.put('a', 'valueA');
  cache.put('b', 'valueB');
  cache.put('c', 'valueC');
  assertEquals('valueA', cache.peek('a'));
  assertEquals('valueB', cache.peek('b'));
  assertEquals('valueC', cache.peek('c'));
  cache.setMaxSize(1);
  assertEquals(null, cache.peek('a'));
  assertEquals(null, cache.peek('b'));
  assertEquals('valueC', cache.peek('c'));
}
