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

goog.module('goog.dom.patternTest');
goog.setTestOnly();

const AllChildren = goog.require('goog.dom.pattern.AllChildren');
const ChildMatches = goog.require('goog.dom.pattern.ChildMatches');
const EndTag = goog.require('goog.dom.pattern.EndTag');
const FullTag = goog.require('goog.dom.pattern.FullTag');
const MatchType = goog.require('goog.dom.pattern.MatchType');
const NodeType = goog.require('goog.dom.NodeType');
const PatternNodeType = goog.require('goog.dom.pattern.NodeType');
const PatternText = goog.require('goog.dom.pattern.Text');
const Repeat = goog.require('goog.dom.pattern.Repeat');
const Sequence = goog.require('goog.dom.pattern.Sequence');
const StartTag = goog.require('goog.dom.pattern.StartTag');
const TagWalkType = goog.require('goog.dom.TagWalkType');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

// TODO(robbyw): write a test that checks if backtracking works in Sequence

testSuite({
  testStartTag() {
    const pattern = new StartTag('DIV');
    assertEquals(
        'StartTag(div) should match div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(div) should not match span', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(div) should not match /div', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
  },

  testStartTagCase() {
    const pattern = new StartTag('diV');
    assertEquals(
        'StartTag(diV) should match div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(diV) should not match span', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
  },

  testStartTagRegex() {
    const pattern = new StartTag(/D/);
    assertEquals(
        'StartTag(/D/) should match div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(/D/) should not match span', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(/D/) should not match /div', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
  },

  testStartTagAttributes() {
    const pattern = new StartTag('DIV', {id: 'div1'});
    assertEquals(
        'StartTag(div,id:div1) should match div1', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(div,id:div2) should not match div1', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div2'), TagWalkType.START_TAG));
  },

  testStartTagStyle() {
    const pattern = new StartTag('SPAN', null, {color: 'red'});
    assertEquals(
        'StartTag(span,null,color:red) should match span1', MatchType.MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(span,null,color:blue) should not match span1',
        MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('span2'), TagWalkType.START_TAG));
  },

  testStartTagAttributeRegex() {
    const pattern = new StartTag('SPAN', {id: /span\d/});
    assertEquals(
        'StartTag(span,id:/span\\d/) should match span1', MatchType.MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'StartTag(span,id:/span\\d/) should match span2', MatchType.MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
  },

  testEndTag() {
    const pattern = new EndTag('DIV');
    assertEquals(
        'EndTag should match div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
  },

  testEndTagRegex() {
    const pattern = new EndTag(/D/);
    assertEquals(
        'EndTag(/D/) should match /div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
    assertEquals(
        'EndTag(/D/) should not match /span', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'EndTag(/D/) should not match div', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
  },

  testChildMatches() {
    const pattern = new ChildMatches(new StartTag('DIV'), 2);

    assertEquals(
        'ChildMatches should match div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'ChildMatches should match /div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
    assertEquals(
        'ChildMatches should match div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div2'), TagWalkType.START_TAG));
    assertEquals(
        'ChildMatches should match /div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div2'), TagWalkType.END_TAG));
    assertEquals(
        'ChildMatches should finish match at /body', MatchType.BACKTRACK_MATCH,
        pattern.matchToken(document.body, TagWalkType.END_TAG));

    assertEquals(
        'ChildMatches should match div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div2'), TagWalkType.START_TAG));
    assertEquals(
        'ChildMatches should match /div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div2'), TagWalkType.END_TAG));
    assertEquals(
        'ChildMatches should fail to match at /body: not enough child matches',
        MatchType.NO_MATCH,
        pattern.matchToken(document.body, TagWalkType.END_TAG));
  },

  testFullTag() {
    const pattern = new FullTag('DIV');
    assertEquals(
        'FullTag(div) should match div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'FullTag(div) should match /div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));

    assertEquals(
        'FullTag(div) should start match at div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'FullTag(div) should continue to match span', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'FullTag(div) should continue to match /span', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'FullTag(div) should finish match at /div', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
  },

  testAllChildren() {
    const pattern = new AllChildren();
    assertEquals(
        'AllChildren(div) should match div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'AllChildren(div) should match /div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
    assertEquals(
        'AllChildren(div) should match at /body', MatchType.BACKTRACK_MATCH,
        pattern.matchToken(document.body, TagWalkType.END_TAG));

    assertEquals(
        'AllChildren(div) should start match at div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'AllChildren(div) should continue to match span', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'AllChildren(div) should continue to match /span', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'AllChildren(div) should continue to match at /div', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
    assertEquals(
        'AllChildren(div) should finish match at /body',
        MatchType.BACKTRACK_MATCH,
        pattern.matchToken(document.body, TagWalkType.END_TAG));
  },

  testText() {
    const pattern = new PatternText('Text');
    assertEquals(
        'Text should match div3/text()', MatchType.MATCH,
        pattern.matchToken(
            dom.getElement('div3').firstChild, TagWalkType.OTHER));
    assertEquals(
        'Text should not match div4/text()', MatchType.NO_MATCH,
        pattern.matchToken(
            dom.getElement('div4').firstChild, TagWalkType.OTHER));
    assertEquals(
        'Text should not match div3', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div3'), TagWalkType.START_TAG));
  },

  testTextRegex() {
    const pattern = new PatternText(/Text/);
    assertEquals(
        'Text(regex) should match div3/text()', MatchType.MATCH,
        pattern.matchToken(
            dom.getElement('div3').firstChild, TagWalkType.OTHER));
    assertEquals(
        'Text(regex) should match div4/text()', MatchType.MATCH,
        pattern.matchToken(
            dom.getElement('div4').firstChild, TagWalkType.OTHER));
  },

  testNodeType() {
    const pattern = new PatternNodeType(NodeType.COMMENT);
    assertEquals(
        'Comment matcher should match a comment', MatchType.MATCH,
        pattern.matchToken(
            dom.getElement('nodeTypes').firstChild, TagWalkType.OTHER));
    assertEquals(
        'Comment matcher should not match a text node', MatchType.NO_MATCH,
        pattern.matchToken(
            dom.getElement('nodeTypes').lastChild, TagWalkType.OTHER));
  },

  testSequence() {
    const pattern = new Sequence([
      new StartTag('DIV'),
      new StartTag('SPAN'),
      new EndTag('SPAN'),
      new EndTag('DIV'),
    ]);

    assertEquals(
        'Sequence[0] should match div1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[1] should match span1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[2] should match /span1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'Sequence[3] should match /div1', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));

    assertEquals(
        'Sequence[0] should match div1 again', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[1] should match span1 again', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[2] should match /span1 again', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'Sequence[3] should match /div1 again', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));

    assertEquals(
        'Sequence[0] should match div1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[1] should not match div1', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));

    assertEquals(
        'Sequence[0] should match div1 after failure', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[1] should match span1 after failure', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.START_TAG));
    assertEquals(
        'Sequence[2] should match /span1 after failure', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('span1'), TagWalkType.END_TAG));
    assertEquals(
        'Sequence[3] should match /div1 after failure', MatchType.MATCH,
        pattern.matchToken(dom.getElement('div1'), TagWalkType.END_TAG));
  },

  testRepeat() {
    const pattern = new Repeat(new StartTag('B'));

    // Note: this test does not mimic an actual matcher because it is only
    // passing the START_TAG events.

    assertEquals(
        'Repeat[B] should match b1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('b1'), TagWalkType.START_TAG));
    assertEquals(
        'Repeat[B] should match b2', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('b2'), TagWalkType.START_TAG));
    assertEquals(
        'Repeat[B] should backtrack match i1', MatchType.BACKTRACK_MATCH,
        pattern.matchToken(dom.getElement('i1'), TagWalkType.START_TAG));
    assertEquals('Repeat[B] should have match count of 2', 2, pattern.count);

    assertEquals(
        'Repeat[B] should backtrack match i1 even with no b matches',
        MatchType.BACKTRACK_MATCH,
        pattern.matchToken(dom.getElement('i1'), TagWalkType.START_TAG));
    assertEquals('Repeat[B] should have match count of 0', 0, pattern.count);
  },

  testRepeatWithMinimum() {
    const pattern = new Repeat(new StartTag('B'), 1);

    // Note: this test does not mimic an actual matcher because it is only
    // passing the START_TAG events.

    assertEquals(
        'Repeat[B,1] should match b1', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('b1'), TagWalkType.START_TAG));
    assertEquals(
        'Repeat[B,1] should match b2', MatchType.MATCHING,
        pattern.matchToken(dom.getElement('b2'), TagWalkType.START_TAG));
    assertEquals(
        'Repeat[B,1] should backtrack match i1', MatchType.BACKTRACK_MATCH,
        pattern.matchToken(dom.getElement('i1'), TagWalkType.START_TAG));
    assertEquals('Repeat[B,1] should have match count of 2', 2, pattern.count);

    assertEquals(
        'Repeat[B,1] should not match i1', MatchType.NO_MATCH,
        pattern.matchToken(dom.getElement('i1'), TagWalkType.START_TAG));
  },

  testRepeatWithMaximum() {
    const pattern = new Repeat(new StartTag('B'), 1, 1);

    // Note: this test does not mimic an actual matcher because it is only
    // passing the START_TAG events.

    assertEquals(
        'Repeat[B,1] should match b1', MatchType.MATCH,
        pattern.matchToken(dom.getElement('b1'), TagWalkType.START_TAG));
  },

  testSequenceBacktrack() {
    const pattern = new Sequence([
      new Repeat(new StartTag('SPAN')),
      new PatternText('X'),
    ]);

    const root = dom.getElement('span3');
    assertEquals(
        'Sequence[Repeat[SPAN],"X"] should match span3', MatchType.MATCHING,
        pattern.matchToken(root, TagWalkType.START_TAG));
    assertEquals(
        'Sequence[Repeat[SPAN],"X"] should match span3.firstChild',
        MatchType.MATCHING,
        pattern.matchToken(root.firstChild, TagWalkType.START_TAG));
    assertEquals(
        'Sequence[Repeat[SPAN],"X"] should match span3.firstChild.firstChild',
        MatchType.MATCHING,
        pattern.matchToken(root.firstChild.firstChild, TagWalkType.START_TAG));
    assertEquals(
        'Sequence[Repeat[SPAN],"X"] should finish match text node',
        MatchType.MATCH,
        pattern.matchToken(
            root.firstChild.firstChild.firstChild, TagWalkType.OTHER));
  },
});
