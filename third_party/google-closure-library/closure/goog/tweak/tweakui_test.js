/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.tweak.TweakUiTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const TweakUi = goog.require('goog.tweak.TweakUi');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
/** @suppress {extraRequire} needed for createRegistryEntries. */
const testhelpers = goog.require('goog.tweak.testhelpers');
const tweak = goog.require('goog.tweak');

let root;
let registry;
const EXPECTED_ENTRIES_COUNT = 14;

function createUi(collapsible) {
  const tweakUiElem =
      collapsible ? TweakUi.createCollapsible() : TweakUi.create();
  root.appendChild(tweakUiElem);
}

/** @suppress {visibility} suppression added to enable type checking */
function getAllEntryDivs() {
  return dom.getElementsByTagNameAndClass(
      TagName.DIV, TweakUi.ENTRY_CSS_CLASS_);
}

function getEntryDiv(entry) {
  /** @suppress {visibility} suppression added to enable type checking */
  const label = TweakUi.getNamespacedLabel_(entry);
  const allDivs = getAllEntryDivs();
  let ret;
  for (let i = 0, div; div = allDivs[i]; i++) {
    const divText = dom.getTextContent(div);
    if (googString.startsWith(divText, label) &&
        googString.contains(divText, entry.description)) {
      assertFalse('Found multiple divs matching entry ' + entry.getId(), !!ret);
      ret = div;
    }
  }
  assertTrue('getEntryDiv failed for ' + entry.getId(), !!ret);
  return ret;
}

function getEntryInput(entry) {
  const div = getEntryDiv(entry);
  return dom.getElementsByTagName(TagName.INPUT, div)[0] ||
      dom.getElementsByTagName(TagName.SELECT, div)[0];
}

function assertEntryOrder(entryId1, entryId2) {
  const entry1 = registry.getEntry(entryId1);
  const entry2 = registry.getEntry(entryId2);
  const div1 = getEntryDiv(entry1);
  const div2 = getEntryDiv(entry2);
  const order = dom.compareNodeOrder(div1, div2);
  assertTrue(entry1.getId() + ' should be before ' + entry2.getId(), order < 0);
}

testSuite({
  setUp() {
    root = document.getElementById('root');
    // Make both test cases use the same entries in order to be able to test
    // that having two UIs on the same page does not cause trouble.
    createRegistryEntries('');
    registry = tweak.getRegistry();
  },

  tearDown() {
    /** @suppress {visibility} suppression added to enable type checking */
    tweak.activeBooleanGroup_ = null;
    // When debugging a single test, don't clear out the DOM.
    if (window.location.search.indexOf('runTests') == -1) {
      dom.removeChildren(root);
    }
  },

  tearDownPage() {
    // When debugging a single test, don't clear out the DOM.
    if (window.location.search.indexOf('runTests') != -1) {
      return;
    }
    // Create both registries for interactive testing.
    createRegistryEntries('');
    registry = tweak.getRegistry();
    // Add an extra tweak for testing the creation of tweaks after the UI has
    // already been rendered.
    let entryCounter = 0;
    tweak.registerButton(
        'CreateNewTweak',
        'Creates a new tweak. Meant ' +
            'to simulate a tweak being registered in a lazy-loaded module.',
        () => {
          // use computed properties to avoid compiler checks of tweak
          tweak['registerBoolean'](
              'Lazy' + ++entryCounter, 'Lazy-loaded tweak.');
        });
    tweak.registerButton(
        'CreateNewTweakInNamespace1',
        'Creates a new tweak within a namespace. Meant to simulate a tweak ' +
            'being registered in a lazy-loaded module.',
        () => {
          // use computed properties to avoid compiler checks of tweak
          tweak['registerString'](
              'foo.bar.Lazy' + ++entryCounter, 'Lazy-loaded tweak.');
        });
    tweak.registerButton(
        'CreateNewTweakInNamespace2',
        'Creates a new tweak within a namespace. Meant to simulate a tweak ' +
            'being registered in a lazy-loaded module.',
        () => {
          // use computed properties to avoid compiler checks of tweak
          tweak['registerNumber'](
              'foo.bar.baz.Lazy' + ++entryCounter, 'Lazy combo', 3,
              {validValues: [1, 2, 3], label: 'Lazy!'});
        });

    let label = document.createElement('h3');
    dom.setTextContent(label, 'TweakUi:');
    root.appendChild(label);
    createUi(false);

    label = document.createElement('h3');
    dom.setTextContent(label, 'Collapsible:');
    root.appendChild(label);
    createUi(true);
  },

  testCreate() {
    createUi(false);
    assertEquals(
        'Wrong number of entry divs.', EXPECTED_ENTRIES_COUNT,
        getAllEntryDivs().length);

    assertFalse(
        'checkbox should not be checked 1', getEntryInput(boolEntry).checked);
    assertTrue(
        'checkbox should be checked 2', getEntryInput(boolEntry2).checked);
    // Enusre custom labels are being used.
    let html = dom.getElementsByTagName(TagName.BUTTON)[0].innerHTML;
    assertTrue('Button label is wrong', html.indexOf('&lt;btn&gt;') > -1);
    html = getEntryDiv(numEnumEntry).innerHTML;
    assertTrue('Enum2 label is wrong', html.indexOf('second&amp;') > -1);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testToggleBooleanSetting() {
    boolEntry.setValue(true);
    createUi(false);

    assertTrue('checkbox should be checked', getEntryInput(boolEntry).checked);

    boolEntry.setValue(false);
    assertFalse(
        'checkbox should not be checked 1', getEntryInput(boolEntry).checked);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testToggleStringSetting() {
    strEntry.setValue('val1');
    createUi(false);

    assertEquals(
        'Textbox has wrong value 1', 'val1', getEntryInput(strEntry).value);

    strEntry.setValue('val2');
    assertEquals(
        'Textbox has wrong value 2', 'val2', getEntryInput(strEntry).value);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testToggleStringEnumSetting() {
    strEnumEntry.setValue('B');
    createUi(false);

    assertEquals('wrong value 1', 'B', getEntryInput(strEnumEntry).value);

    strEnumEntry.setValue('C');
    assertEquals('wrong value 2', 'C', getEntryInput(strEnumEntry).value);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testToggleNumericSetting() {
    numEntry.setValue(3);
    createUi(false);

    assertEquals('wrong value 1', '3', getEntryInput(numEntry).value);

    numEntry.setValue(4);
    assertEquals('wrong value 2', '4', getEntryInput(numEntry).value);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testToggleNumericEnumSetting() {
    numEnumEntry.setValue(2);
    createUi(false);

    assertEquals('wrong value 1', '2', getEntryInput(numEnumEntry).value);

    numEnumEntry.setValue(3);
    assertEquals('wrong value 2', '3', getEntryInput(numEnumEntry).value);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testClickBooleanSetting() {
    createUi(false);

    const input = getEntryInput(boolEntry);
    input.checked = true;
    input.onchange();
    assertTrue('setting should be true', boolEntry.getNewValue());
    input.checked = false;
    input.onchange();
    assertFalse('setting should be false', boolEntry.getNewValue());
  },

  /**
     @suppress {checkTypes,strictMissingProperties} suppression added to enable
     type checking
   */
  testToggleDescriptions() {
    createUi(false);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const toggleLink = dom.getElementsByTagName(TagName.A, root)[0];
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const heightBefore = root.offsetHeight;
    toggleLink.onclick();
    assertTrue(
        'Expected div height to grow from toggle descriptions.',
        root.offsetHeight > heightBefore);
    toggleLink.onclick();
    assertEquals(
        'Expected div height to revert from toggle descriptions.', heightBefore,
        root.offsetHeight);
  },

  testAddEntry() {
    createUi(false);
    // use computed properties to avoid compiler checks of tweak
    tweak['registerBoolean']('Lazy1', 'Lazy-loaded tweak.');
    tweak['registerBoolean'](
        'Lazy2', 'Lazy-loaded tweak.',
        /* defaultValue */ false, {restartRequired: false});
    tweak.beginBooleanGroup('LazyGroup', 'Lazy-loaded tweak.');
    tweak['registerBoolean']('Lazy3', 'Lazy-loaded tweak.');
    tweak.endBooleanGroup();

    assertEquals(
        'Wrong number of entry divs.', EXPECTED_ENTRIES_COUNT + 4,
        getAllEntryDivs().length);
    assertEntryOrder('Enum2', 'Lazy1');
    assertEntryOrder('Lazy1', 'Lazy2');
    assertEntryOrder('Lazy2', 'Num');
    assertEntryOrder('BoolGroup', 'Lazy3');
  },

  testAddNamespacedEntries() {
    createUi(false);
    tweak.beginBooleanGroup('NS.LazyGroup', 'Lazy-loaded tweak.');
    // use computed properties to avoid compiler checks of tweak
    tweak['registerBoolean']('NS.InGroup', 'Lazy-loaded tweak.');
    tweak.endBooleanGroup();
    tweak['registerBoolean']('NS.Banana', 'Lazy-loaded tweak.');
    tweak['registerBoolean']('NS.Apple', 'Lazy-loaded tweak.');

    assertEquals(
        'Wrong number of entry divs.', EXPECTED_ENTRIES_COUNT + 5,
        getAllEntryDivs().length);
    assertEntryOrder('Enum2', 'NS.Apple');
    assertEntryOrder('NS.Apple', 'NS.Banana');
    assertEntryOrder('NS.Banana', 'NS.InGroup');
  },

  testCollapsibleIsLazy() {
    if (document.createEvent) {
      createUi(true);
      assertEquals('Expected no entry divs.', 0, getAllEntryDivs().length);
      /** @suppress {checkTypes} suppression added to enable type checking */
      const showLink = dom.getElementsByTagName(TagName.A, root)[0];
      const event = document.createEvent('MouseEvents');
      event.initMouseEvent(
          'click', true, true, window, 0, 0, 0, 0, 0, false, false, false,
          false, 0, null);
      showLink.dispatchEvent(event);
      assertEquals(
          'Wrong number of entry divs.', EXPECTED_ENTRIES_COUNT,
          getAllEntryDivs().length);
    }
  },
});
