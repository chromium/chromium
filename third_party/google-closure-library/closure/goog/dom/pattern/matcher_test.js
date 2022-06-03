// Copyright 2007 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.dom.pattern.matcherTest');
goog.setTestOnly();

const CallbackCounter = goog.require('goog.dom.pattern.callback.Counter');
const EndTag = goog.require('goog.dom.pattern.EndTag');
const FullTag = goog.require('goog.dom.pattern.FullTag');
const Matcher = goog.require('goog.dom.pattern.Matcher');
const Repeat = goog.require('goog.dom.pattern.Repeat');
const Sequence = goog.require('goog.dom.pattern.Sequence');
const StartTag = goog.require('goog.dom.pattern.StartTag');
const StopIteration = goog.require('goog.iter.StopIteration');
const TagName = goog.require('goog.dom.TagName');
const Test = goog.require('goog.dom.pattern.callback.Test');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testMatcherAndStartTag() {
    const pattern = new StartTag('P');

    const counter = new CallbackCounter();
    const matcher = new Matcher();
    matcher.addPattern(pattern, counter.getCallback());
    matcher.match(document.body);

    assertEquals('StartTag(p) should match 5 times in body', 5, counter.count);
  },

  testMatcherAndStartTagTwice() {
    const pattern = new StartTag('P');

    const counter = new CallbackCounter();
    const matcher = new Matcher();
    matcher.addPattern(pattern, counter.getCallback());
    matcher.match(document.body);

    assertEquals('StartTag(p) should match 5 times in body', 5, counter.count);

    // Make sure no state got mangled.
    counter.reset();
    matcher.match(document.body);

    assertEquals(
        'StartTag(p) should match 5 times in body again', 5, counter.count);
  },

  testMatcherAndStartTagAttributes() {
    const pattern = new StartTag('SPAN', {id: /./});

    const counter = new CallbackCounter();
    const matcher = new Matcher();
    matcher.addPattern(pattern, counter.getCallback());
    matcher.match(document.body);

    assertEquals(
        'StartTag(span,id) should match 2 times in body', 2, counter.count);
  },

  testMatcherWithTwoPatterns() {
    const pattern1 = new StartTag('SPAN');
    const pattern2 = new StartTag('P');

    const counter = new CallbackCounter();

    const matcher = new Matcher();
    matcher.addPattern(pattern1, counter.getCallback());
    matcher.addPattern(pattern2, counter.getCallback());

    matcher.match(document.body);

    assertEquals(
        'StartTag(span|p) should match 8 times in body', 8, counter.count);
  },

  testMatcherWithQuit() {
    const pattern1 = new StartTag('SPAN');
    const pattern2 = new StartTag('P');

    let count = 0;
    const callback = (node, position) => {
      if (node.nodeName == TagName.SPAN) {
        throw StopIteration;
        return true;
      }
      count++;
    };

    const matcher = new Matcher();
    matcher.addPattern(pattern1, callback);
    matcher.addPattern(pattern2, callback);

    matcher.match(document.body);

    assertEquals('Stopped span|p should match 1 time in body', 1, count);
  },

  testMatcherWithReplace() {
    const pattern1 = new StartTag('B');
    const pattern2 = new StartTag('I');

    let count = 0;
    const callback = (node, position) => {
      count++;
      if (node.nodeName == TagName.B) {
        const i = dom.createDom(TagName.I);
        node.parentNode.insertBefore(i, node);
        dom.removeNode(node);

        position.setPosition(i);

        return true;
      }
    };

    const matcher = new Matcher();
    matcher.addPattern(pattern1, callback);
    matcher.addPattern(pattern2, callback);

    matcher.match(dom.getElement('div1'));

    assertEquals('i|b->i should match 5 times in div1', 5, count);
  },

  testMatcherAndFullTag() {
    const pattern = new FullTag('P');

    const test = new Test();

    const matcher = new Matcher();
    matcher.addPattern(pattern, test.getCallback());

    matcher.match(dom.getElement('p1'));

    assert('FullTag(p) should match on p1', test.matched);

    test.reset();
    matcher.match(dom.getElement('div1'));

    assert('FullTag(p) should not match on div1', !test.matched);
  },

  testMatcherAndSequence() {
    const pattern = new Sequence(
        [
          new StartTag('P'),
          new StartTag('SPAN'),
          new EndTag('SPAN'),
          new EndTag('P'),
        ],
        true);

    const counter = new CallbackCounter();
    const matcher = new Matcher();
    matcher.addPattern(pattern, counter.getCallback());
    matcher.match(document.body);

    assertEquals('Sequence should match 1 times in body', 1, counter.count);
  },

  testMatcherAndRepeatFullTag() {
    const pattern = new Repeat(new FullTag('P'), 1);

    let count = 0;
    let tcount = 0;
    const matcher = new Matcher();
    matcher.addPattern(pattern, () => {
      count++;
      tcount += pattern.count;
    });
    matcher.match(document.body);

    assertEquals('Repeated p should match 2 times in body', 2, count);
    assertEquals('Repeated p should match 5 total times in body', 5, tcount);
  },
});
