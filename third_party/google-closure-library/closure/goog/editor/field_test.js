/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Shared tests for Field and ContentEditableField.
 * Since ContentEditableField switches many of the internal code paths in Field
 * (such as via usesIframe()) it's important to re-run a lot of the same tests.
 */

/** @suppress {extraProvide} */
goog.module('goog.editor.field_test');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const EventType = goog.require('goog.events.EventType');
const Field = goog.require('goog.editor.Field');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const LooseMock = goog.require('goog.testing.LooseMock');
const MockClock = goog.require('goog.testing.MockClock');
const Plugin = goog.require('goog.editor.Plugin');
const Range = goog.require('goog.dom.Range');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const editorRange = goog.require('goog.editor.range');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googDom = goog.require('goog.dom');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

/** Hard-coded HTML for the tests. */
const HTML = '<div id="testField">I am text.</div>';

// Tests for the plugin interface.
/**
 * Dummy plugin for test usage.
 * @final
 */
class TestPlugin extends Plugin {
  constructor() {
    super();

    this.getTrogClassId = () => 'TestPlugin';

    /** @suppress {checkTypes} suppression added to enable type checking */
    this.handleKeyDown = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.handleKeyPress = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.handleKeyUp = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.handleKeyboardShortcut = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.isSupportedCommand = goog.nullFunction;
    this.execCommandInternal = goog.nullFunction;
    this.queryCommandValue = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.activeOnUneditableFields = goog.nullFunction;
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.handleSelectionChange = goog.nullFunction;
  }
}

const STRING_KEY = String.fromCharCode(KeyCodes.A).toLowerCase();

/**
 * @return {!events.Event} Returns an event for a keyboard shortcut for the
 *     letter 'a'.
 */
function getBrowserEvent() {
  const e = new BrowserEvent();
  e.ctrlKey = true;
  e.metaKey = true;
  e.charCode = KeyCodes.A;
  return e;
}

/**
 * @param {boolean} followLinkInNewWindow Whether activating a hyperlink in the
 *     editable field will open a new window or not.
 * @return {!Field} Returns an editable field after its load phase.
 */
function createEditableFieldWithListeners(followLinkInNewWindow) {
  const editableField = new FieldConstructor('testField');
  editableField.setFollowLinkInNewWindow(followLinkInNewWindow);

  const originalElement = editableField.getOriginalElement();
  editableField.setupFieldObject(originalElement);
  editableField.handleFieldLoad();

  return editableField;
}

function getListenerTarget(editableField) {
  const elt = editableField.getElement();
  const listenerTarget =
      BrowserFeature.USE_DOCUMENT_FOR_KEY_EVENTS && editableField.usesIframe() ?
      elt.ownerDocument :
      elt;
  return listenerTarget;
}

function assertClickDefaultActionIsCanceled(editableField) {
  /** @suppress {visibility} suppression added to enable type checking */
  const cancelClickDefaultActionListener = events.getListener(
      getListenerTarget(editableField), EventType.CLICK, Field.cancelLinkClick_,
      undefined, editableField);

  assertNotNull(cancelClickDefaultActionListener);
}

function assertClickDefaultActionIsNotCanceled(editableField) {
  /** @suppress {visibility} suppression added to enable type checking */
  const cancelClickDefaultActionListener = events.getListener(
      getListenerTarget(editableField), EventType.CLICK, Field.cancelLinkClick_,
      undefined, editableField);

  assertNull(cancelClickDefaultActionListener);
}

function focusFieldSync(field) {
  field.focus();

  // IE fires focus events async, so create a fake focus event
  // synchronously.
  testingEvents.fireFocusEvent(field.getElement());
}

/**
 * Helper to test that the cursor is placed at the beginning of the editable
 * field's contents.
 * @param {string=} html Html to replace the test file default field contents
 *     with.
 * @param {string=} parentId Id of the parent of the node where the cursor is
 *     expected to be placed. If omitted, will expect cursor to be placed in the
 *     first child of the field element (or, if the field has no content, in the
 *     field element itself).
 */
function doTestPlaceCursorAtStart(html = undefined, parentId = undefined) {
  const editableField = new FieldConstructor('testField', document);
  editableField.makeEditable();
  // Initially place selection not at the start of the editable field.
  let textNode = editableField.getElement().firstChild;
  Range.createFromNodes(textNode, 1, textNode, 2).select();
  if (html != null) {
    editableField.getElement().innerHTML = html;
  }

  editableField.placeCursorAtStart();
  const range = editableField.getRange();
  assertNotNull(
      'Placing the cursor should result in a range object being available',
      range);
  assertTrue('The range should be collapsed', range.isCollapsed());
  textNode = editableField.getElement().firstChild;

  // We check whether getAttribute exist because textNode may be an actual
  // TextNode, which does not have getAttribute.
  const hasBogusNode = textNode &&
      ((textNode.getAttribute &&
        textNode.getAttribute('_moz_editor_bogus_node')) ||
       (userAgent.GECKO && textNode.tagName === TagName.BR &&
        textNode.parentNode.children.length === 1));
  if (hasBogusNode) {
    // At least in FF >= 6, assigning '' to innerHTML of a contentEditable
    // element will results in textNode being modified into:
    // <br _moz_editor_bogus_node="TRUE" _moz_dirty=""> instead of nulling
    // it. So we should null it ourself.
    // This was changed in FF >= 70 to simply be a single <br>.
    textNode = null;
  }

  let startNode = parentId ?
      editableField.getEditableDomHelper().getElement(parentId).firstChild :
      textNode ? textNode : editableField.getElement();
  assertEquals(
      'The range should start at the specified expected node', startNode,
      range.getStartNode());
  assertEquals(
      'The range should start at the beginning of the node', 0,
      range.getStartOffset());
}

/**
 * Helper to test that the cursor is placed at the beginning of the editable
 * field's contents.
 * @param {string=} html Html to replace the test file default field contents
 *     with.
 * @param {string=} parentId Id of the parent of the node where the cursor is
 *     expected to be placed. If omitted, will expect cursor to be placed in the
 *     first child of the field element (or, if the field has no content, in the
 *     field element itself).
 * @param {number=} opt_offset The offset to expect for the end position.
 */
function doTestPlaceCursorAtEnd(
    html = undefined, parentId = undefined, opt_offset) {
  const editableField = new FieldConstructor('testField', document);
  editableField.makeEditable();

  // Initially place selection not at the end of the editable field.
  let textNode = editableField.getElement().firstChild;
  Range.createFromNodes(textNode, 0, textNode, 1).select();
  if (html != null) {
    editableField.getElement().innerHTML = html;
  }

  editableField.placeCursorAtEnd();
  const range = editableField.getRange();
  assertNotNull(
      'Placing the cursor should result in a range object being available',
      range);
  assertTrue('The range should be collapsed', range.isCollapsed());
  textNode = editableField.getElement().firstChild;

  // We check whether getAttribute exist because textNode may be an actual
  // TextNode, which does not have getAttribute.
  const hasBogusNode = textNode &&
      ((textNode.getAttribute &&
        textNode.getAttribute('_moz_editor_bogus_node')) ||
       (userAgent.GECKO && textNode.tagName === TagName.BR &&
        textNode.parentNode.children.length === 1));
  if (hasBogusNode) {
    // At least in FF >= 6, assigning '' to innerHTML of a contentEditable
    // element will results in textNode being modified into:
    // <br _moz_editor_bogus_node="TRUE" _moz_dirty=""> instead of nulling
    // it. So we should null it ourself.
    // This was changed in FF >= 70 to simply be a single <br>.
    textNode = null;
  }

  const endNode = parentId ?
      editableField.getEditableDomHelper().getElement(parentId).lastChild :
      textNode ? textNode : editableField.getElement();
  assertEquals(
      'The range should end at the specified expected node', endNode,
      range.getEndNode());
  const offset = (opt_offset != null) ?
      opt_offset :
      textNode ? endNode.nodeValue.length : endNode.childNodes.length - 1;
  if (hasBogusNode) {
    assertEquals(
        'The range should end at the ending of the bogus node ' +
            'added by FF',
        offset + 1, range.getEndOffset());
  } else {
    assertEquals(
        'The range should end at the ending of the node', offset,
        range.getEndOffset());
  }
}

testSuite({
  setUp() {
    googDom.getElement('parent').innerHTML = HTML;
    assertTrue(
        'FieldConstructor should be set by the test HTML file',
        typeof FieldConstructor === 'function');
  },

  /** @suppress {uselessCode} suppression added to enable type checking */
  tearDown() {
    // NOTE(nicksantos): I think IE is blowing up on this call because
    // it is lame. It manifests its lameness by throwing an exception.
    // Kudos to XT for helping me to figure this out.
    try {
    } catch (e) {
    }
  },

  /**
   * Tests that calling registerPlugin will add the plugin to the
   * plugin map.
   */
  testRegisterPlugin() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    editableField.registerPlugin(plugin);

    assertEquals(
        'Registered plugin must be in protected plugin map.', plugin,
        editableField.plugins_[plugin.getTrogClassId()]);
    assertEquals(
        'Plugin has a keydown handler, should be in keydown map', plugin,
        editableField.indexedPlugins_[Plugin.Op.KEYDOWN][0]);
    assertEquals(
        'Plugin has a keypress handler, should be in keypress map', plugin,
        editableField.indexedPlugins_[Plugin.Op.KEYPRESS][0]);
    assertEquals(
        'Plugin has a keyup handler, should be in keuup map', plugin,
        editableField.indexedPlugins_[Plugin.Op.KEYUP][0]);
    assertEquals(
        'Plugin has a selectionchange handler, should be in selectionchange map',
        plugin, editableField.indexedPlugins_[Plugin.Op.SELECTION][0]);
    assertEquals(
        'Plugin has a shortcut handler, should be in shortcut map', plugin,
        editableField.indexedPlugins_[Plugin.Op.SHORTCUT][0]);
    assertEquals(
        'Plugin has a execCommand, should be in execCommand map', plugin,
        editableField.indexedPlugins_[Plugin.Op.EXEC_COMMAND][0]);
    assertEquals(
        'Plugin has a queryCommand, should be in queryCommand map', plugin,
        editableField.indexedPlugins_[Plugin.Op.QUERY_COMMAND][0]);
    assertEquals(
        'Plugin does not have a prepareContentsHtml,' +
            'should not be in prepareContentsHtml map',
        undefined,
        editableField.indexedPlugins_[Plugin.Op.PREPARE_CONTENTS_HTML][0]);
    assertEquals(
        'Plugin does not have a cleanContentsDom,' +
            'should not be in cleanContentsDom map',
        undefined,
        editableField.indexedPlugins_[Plugin.Op.CLEAN_CONTENTS_DOM][0]);
    assertEquals(
        'Plugin does not have a cleanContentsHtml,' +
            'should not be in cleanContentsHtml map',
        undefined,
        editableField.indexedPlugins_[Plugin.Op.CLEAN_CONTENTS_HTML][0]);

    editableField.dispose();
  },

  /**
   * Tests that calling unregisterPlugin will remove the plugin from
   * the map.
   */
  testUnregisterPlugin() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    editableField.registerPlugin(plugin);
    editableField.unregisterPlugin(plugin);

    assertUndefined(
        'Unregistered plugin must not be in protected plugin map.',
        editableField.plugins_[plugin.getTrogClassId()]);

    editableField.dispose();
  },

  /** Tests that registered plugins can be fetched by their id. */
  testGetPluginByClassId() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    assertNull(
        'Must not be able to get unregistered plugins by class id.',
        editableField.getPluginByClassId(plugin.getTrogClassId()));

    editableField.registerPlugin(plugin);
    assertEquals(
        'Must be able to get registered plugins by class id.', plugin,
        editableField.getPluginByClassId(plugin.getTrogClassId()));
    editableField.dispose();
  },

  /**
   * Tests that plugins get auto disposed by default when the field is
   * disposed. Tests that plugins with setAutoDispose(false) do not get
   * disposed when the field is disposed.
   */
  testDisposed_PluginAutoDispose() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    const noDisposePlugin = new Plugin();
    noDisposePlugin.getTrogClassId = () => 'noDisposeId';
    noDisposePlugin.setAutoDispose(false);

    editableField.registerPlugin(plugin);
    editableField.registerPlugin(noDisposePlugin);
    editableField.dispose();
    assert(editableField.isDisposed());
    assertTrue(plugin.isDisposed());
    assertFalse(noDisposePlugin.isDisposed());
  },

  /** Tests that plugins are disabled when the field is made uneditable. */
  testMakeUneditableDisablesPlugins() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    let calls = 0;
    plugin.disable = (field) => {
      assertEquals(editableField, field);
      assertTrue(field.isUneditable());
      calls++;
    };

    editableField.registerPlugin(plugin);
    editableField.makeEditable();

    assertEquals(0, calls);
    editableField.makeUneditable();

    assertEquals(1, calls);

    editableField.dispose();
  },

  /**
   * Test that if a browser open a new page when clicking a link in a
   * content editable element, a click listener is set to cancel this
   * default action.
   */
  testClickDefaultActionIsCanceledWhenBrowserFollowsClick() {
    // Simulate a browser that will open a new page when activating a link
    // in a content editable element.
    const editableField =
        createEditableFieldWithListeners(true /* followLinkInNewWindow */);
    assertClickDefaultActionIsCanceled(editableField);

    editableField.dispose();
  },

  /**
   * Test that if a browser does not open a new page when clicking a link in
   * a content editable element, the click default action is not canceled.
   */
  testClickDefaultActionIsNotCanceledWhenBrowserDontFollowsClick() {
    // Simulate a browser that will NOT open a new page when activating a
    // link in a content editable element.
    const editableField =
        createEditableFieldWithListeners(false /* followLinkInNewWindow */);
    assertClickDefaultActionIsNotCanceled(editableField);

    editableField.dispose();
  },

  /**
     Test that if a plugin registers keyup, it gets called.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testPluginKeyUp() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin.handleKeyUp(e);
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    editableField.handleKeyUp_(e);

    mockPlugin.$verify();
  },

  /**
     Test that if a plugin registers keydown, it gets called.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testPluginKeyDown() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin.handleKeyDown(e).$returns(true);
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    editableField.handleKeyDown_(e);

    mockPlugin.$verify();
  },

  /**
     Test that if a plugin registers keypress, it gets called.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testPluginKeyPress() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin.handleKeyPress(e).$returns(true);
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    editableField.handleKeyPress_(e);

    mockPlugin.$verify();
  },

  /**
   * If one plugin handles a key event, the rest of the plugins do not get
   * their key handlers invoked.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testHandledKeyEvent() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin1.handleKeyDown(e).$returns(true);
    } else {
      mockPlugin1.handleKeyPress(e).$returns(true);
    }
    mockPlugin1.handleKeyUp(e).$returns(true);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.handleKeyUp_(e);
    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
     Tests to make sure the cut and paste events are not dispatched
     immediately.
   */
  testHandleCutAndPasteEvents() {
    if (BrowserFeature.USE_MUTATION_EVENTS) {
      // Cut and paste events do not raise events at all in Mozilla.
      return;
    }
    const editableField = new FieldConstructor('testField');
    const clock = new MockClock(true);
    const delayedChanges = recordFunction();
    events.listen(editableField, Field.EventType.DELAYEDCHANGE, delayedChanges);

    editableField.makeEditable();

    testingEvents.fireBrowserEvent(
        new GoogTestingEvent('cut', editableField.getElement()));
    assertEquals(
        'Cut event should be on a timer', 0, delayedChanges.getCallCount());
    clock.tick(1000);
    assertEquals(
        'delayed change event should fire within 1s after cut', 1,
        delayedChanges.getCallCount());

    testingEvents.fireBrowserEvent(
        new GoogTestingEvent('paste', editableField.getElement()));
    assertEquals(
        'Paste event should be on a timer', 1, delayedChanges.getCallCount());
    clock.tick(1000);
    assertEquals(
        'delayed change event should fire within 1s after paste', 2,
        delayedChanges.getCallCount());

    clock.dispose();
    editableField.dispose();
  },

  /**
   * If the first plugin does not handle the key event, the next plugin gets
   * a chance to handle it.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testNotHandledKeyEvent() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin1.handleKeyDown(e).$returns(false);
    } else {
      mockPlugin1.handleKeyPress(e).$returns(false);
    }
    mockPlugin1.handleKeyUp(e).$returns(false);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin2.handleKeyDown(e).$returns(true);
    } else {
      mockPlugin2.handleKeyPress(e).$returns(true);
    }
    mockPlugin2.handleKeyUp(e).$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.handleKeyUp_(e);
    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Make sure that handleKeyboardShortcut is called if other key handlers
   * return false.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testKeyboardShortcutCalled() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin.handleKeyDown(e).$returns(false);
    } else {
      mockPlugin.handleKeyPress(e).$returns(false);
    }
    mockPlugin.handleKeyboardShortcut(e, STRING_KEY, true).$returns(false);
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin.$verify();
  },

  /**
   * Make sure that handleKeyboardShortcut is not called if other key
   * handlers return true.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testKeyboardShortcutNotCalled() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin.handleKeyDown(e).$returns(true);
    } else {
      mockPlugin.handleKeyPress(e).$returns(true);
    }
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin.$verify();
  },

  /**
   * Make sure that handleKeyboardShortcut is not called if alt is pressed.
   * @bug 1363959
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testKeyHandlingAlt() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.altKey = true;

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin.handleKeyDown(e).$returns(false);
    } else {
      mockPlugin.handleKeyPress(e).$returns(false);
    }
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin.$verify();
  },

  /**
   * Make sure that handleKeyboardShortcut is called if alt+shift is
   * pressed.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testKeyHandlingAltShift() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.altKey = true;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.shiftKey = true;

    const mockPlugin = new LooseMock(plugin);
    mockPlugin.getTrogClassId().$returns('mockPlugin');
    mockPlugin.registerFieldObject(editableField);
    mockPlugin.isEnabled(editableField).$anyTimes().$returns(true);
    if (BrowserFeature.USES_KEYDOWN) {
      mockPlugin.handleKeyDown(e).$returns(false);
    } else {
      mockPlugin.handleKeyPress(e).$returns(false);
    }
    mockPlugin.handleKeyboardShortcut(e, STRING_KEY, true).$returns(false);
    mockPlugin.$replay();

    editableField.registerPlugin(mockPlugin);

    if (BrowserFeature.USES_KEYDOWN) {
      editableField.handleKeyDown_(e);
    } else {
      editableField.handleKeyPress_(e);
    }

    mockPlugin.$verify();
  },

  /**
   * Test that if a plugin has an execCommand function, it gets called
   * but only for supported commands.
   */
  testPluginExecCommand() {
    const plugin = new TestPlugin();
    let passedCommand;
    let passedArg;
    plugin.execCommand = (command, arg) => {
      passedCommand = command;
      passedArg = arg;
    };

    const editableField = new FieldConstructor('testField');
    editableField.registerPlugin(plugin);
    plugin.enable(editableField);
    plugin.isSupportedCommand = functions.constant(true);

    editableField.execCommand('+indent', true);
    // Verify that the plugin's execCommand was called with the correct
    // args.
    assertEquals('+indent', passedCommand);
    assertTrue(passedArg);

    passedCommand = null;
    passedArg = null;
    plugin.isSupportedCommand = functions.constant(false);

    editableField.execCommand('+outdent', false);
    // Verify that a plugin's execCommand is not called if it isn't a
    // supported command.
    assertNull(passedCommand);
    assertNull(passedArg);

    editableField.dispose();
    plugin.dispose();
  },

  /**
   * Test that if one plugin supports execCommand, no other plugins
   * get a chance to handle the execComand.
   * @suppress {missingProperties,strictMissingProperties} suppression added
   * to enable type checking
   */
  testSupportedExecCommand() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.isSupportedCommand('+indent').$returns(true);
    mockPlugin1.execCommandInternal('+indent').$returns(true);
    mockPlugin1.execCommand('+indent')
        .$does(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockPlugin1.execCommandInternal('+indent');
               });
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.execCommand('+indent');

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Test that if the first plugin does not support execCommand, the other
   * plugins get a chance to handle the execCommand.
   * @suppress {missingProperties,strictMissingProperties} suppression added
   * to enable type checking
   */
  testNotSupportedExecCommand() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.isSupportedCommand('+indent').$returns(false);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.isSupportedCommand('+indent').$returns(true);
    mockPlugin2.execCommandInternal('+indent').$returns(true);
    mockPlugin2.execCommand('+indent')
        .$does(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockPlugin2.execCommandInternal('+indent');
               });
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.execCommand('+indent');

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Tests that if a plugin supports a command that its queryCommandValue
   * gets called and no further plugins can handle the queryCommandValue.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testSupportedQueryCommand() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.isSupportedCommand('+indent').$returns(true);
    mockPlugin1.queryCommandValue('+indent').$returns(true);
    mockPlugin1.activeOnUneditableFields().$returns(true);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.queryCommandValue('+indent');

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Tests that if the first plugin does not support a command that its
   * queryCommandValue do not get called and the next plugin can handle the
   * queryCommandValue.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testNotSupportedQueryCommand() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.isSupportedCommand('+indent').$returns(false);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.isSupportedCommand('+indent').$returns(true);
    mockPlugin2.queryCommandValue('+indent').$returns(true);
    mockPlugin2.activeOnUneditableFields().$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.queryCommandValue('+indent');

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Tests that if a plugin handles selectionChange that it gets called and
   * no further plugins can handle the selectionChange.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testHandledSelectionChange() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.handleSelectionChange(e, undefined).$returns(true);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.dispatchSelectionChangeEvent(e);

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  /**
   * Tests that if the first plugin does not handle selectionChange that
   * the next plugin gets a chance to handle it.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testNotHandledSelectionChange() {
    const editableField = new FieldConstructor('testField');
    const plugin = new TestPlugin();
    const e = getBrowserEvent();

    const mockPlugin1 = new LooseMock(plugin);
    mockPlugin1.getTrogClassId().$returns('mockPlugin1');
    mockPlugin1.registerFieldObject(editableField);
    mockPlugin1.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin1.handleSelectionChange(e, undefined).$returns(false);
    mockPlugin1.$replay();

    const mockPlugin2 = new LooseMock(plugin);
    mockPlugin2.getTrogClassId().$returns('mockPlugin2');
    mockPlugin2.registerFieldObject(editableField);
    mockPlugin2.isEnabled(editableField).$anyTimes().$returns(true);
    mockPlugin2.handleSelectionChange(e, undefined).$returns(true);
    mockPlugin2.$replay();

    editableField.registerPlugin(mockPlugin1);
    editableField.registerPlugin(mockPlugin2);

    editableField.dispatchSelectionChangeEvent(e);

    mockPlugin1.$verify();
    mockPlugin2.$verify();
  },

  // Tests for goog.editor.Field internals.
  testSelectionChange() {
    const editableField = new FieldConstructor('testField', document);
    const clock = new MockClock(true);
    const beforeSelectionChanges = recordFunction();
    events.listen(
        editableField, Field.EventType.BEFORESELECTIONCHANGE,
        beforeSelectionChanges);
    const selectionChanges = recordFunction();
    events.listen(
        editableField, Field.EventType.SELECTIONCHANGE, selectionChanges);

    editableField.makeEditable();

    // Emulate pressing left arrow key, this should result in a
    // BEFORESELECTIONCHANGE event immediately, and a SELECTIONCHANGE event
    // after a short timeout.
    editableField.handleKeyUp_({keyCode: KeyCodes.LEFT});
    assertEquals(
        'Before selection change should fire immediately', 1,
        beforeSelectionChanges.getCallCount());
    assertEquals(
        'Selection change should be on a timer', 0,
        selectionChanges.getCallCount());
    clock.tick(1000);
    assertEquals(
        'Selection change should fire within 1s', 1,
        selectionChanges.getCallCount());

    // Programically place cursor at start. SELECTIONCHANGE event should be
    // fired.
    editableField.placeCursorAtStart();
    assertEquals(
        'Selection change should fire', 2, selectionChanges.getCallCount());

    clock.dispose();
    editableField.dispose();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testSelectionChangeOnMouseUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fakeEvent = new BrowserEvent({type: 'mouseup', target: 'fakeTarget'});
    const editableField = new FieldConstructor('testField', document);
    const clock = new MockClock(true);
    const beforeSelectionChanges = recordFunction();
    events.listen(
        editableField, Field.EventType.BEFORESELECTIONCHANGE,
        beforeSelectionChanges);
    const selectionChanges = recordFunction();
    events.listen(
        editableField, Field.EventType.SELECTIONCHANGE, selectionChanges);

    const plugin = new TestPlugin();
    plugin.handleSelectionChange = recordFunction();
    editableField.registerPlugin(plugin);

    editableField.makeEditable();

    // Emulate a mouseup event, this should result in immediate
    // BEFORESELECTIONCHANGE and SELECTIONCHANGE, plus a second
    // SELECTIONCHANGE in IE after a short timeout.
    editableField.handleMouseUp_(fakeEvent);
    assertEquals(
        'Before selection change should fire immediately', 1,
        beforeSelectionChanges.getCallCount());
    assertEquals(
        'Selection change should fire immediately', 1,
        selectionChanges.getCallCount());
    assertEquals(
        'Plugin should have handled selection change immediately', 1,
        plugin.handleSelectionChange.getCallCount());
    assertEquals(
        'Plugin should have received original browser event to handle',
        fakeEvent,
        plugin.handleSelectionChange.getLastCall().getArguments()[0]);

    // Pretend another plugin fired a SELECTIONCHANGE in the meantime.
    editableField.dispatchSelectionChangeEvent();
    assertEquals(
        'Second selection change should fire immediately', 2,
        selectionChanges.getCallCount());
    assertEquals(
        'Plugin should have handled second selection change immediately', 2,
        plugin.handleSelectionChange.getCallCount());
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const args = plugin.handleSelectionChange.getLastCall().getArguments();
    assertTrue(
        'Plugin should not have received data from extra firing',
        args.length == 0 || !args[0] && (args.length == 1 || !args[1]));

    // Now check for the extra call in IE.
    clock.tick(1000);
    if (userAgent.IE) {
      assertEquals(
          'Additional selection change should fire within 1s', 3,
          selectionChanges.getCallCount());
      assertEquals(
          'Plugin should have handled selection change within 1s', 3,
          plugin.handleSelectionChange.getCallCount());
      assertEquals(
          'Plugin should have received target of original browser event',
          fakeEvent.target,
          plugin.handleSelectionChange.getLastCall().getArguments().pop());
    } else {
      assertEquals(
          'No additional selection change should fire', 2,
          selectionChanges.getCallCount());
      assertEquals(
          'Plugin should not have handled selection change again', 2,
          plugin.handleSelectionChange.getCallCount());
    }

    clock.dispose();
    editableField.dispose();
  },

  testSelectionChangeBeforeUneditable() {
    const editableField = new FieldConstructor('testField', document);
    const clock = new MockClock(true);
    const selectionChanges = recordFunction();
    events.listen(
        editableField, Field.EventType.SELECTIONCHANGE, selectionChanges);

    editableField.makeEditable();
    editableField.handleKeyUp_({keyCode: KeyCodes.LEFT});
    assertEquals(
        'Selection change should be on a timer', 0,
        selectionChanges.getCallCount());
    editableField.makeUneditable();
    assertEquals(
        'Selection change should fire during make uneditable', 1,
        selectionChanges.getCallCount());
    clock.tick(1000);
    assertEquals(
        'No additional selection change should fire', 1,
        selectionChanges.getCallCount());

    clock.dispose();
    editableField.dispose();
  },

  testGetEditableDomHelper() {
    const editableField = new FieldConstructor('testField', document);
    assertNull(
        'Before being made editable, we do not know the dom helper',
        editableField.getEditableDomHelper());
    editableField.makeEditable();
    assertNotNull(
        'After being made editable, we know the dom helper',
        editableField.getEditableDomHelper());
    assertEquals(
        'Document from domHelper should be the editable elements doc',
        googDom.getOwnerDocument(editableField.getElement()),
        editableField.getEditableDomHelper().getDocument());
    editableField.dispose();
  },

  testQueryCommandValue() {
    const editableField = new FieldConstructor('testField', document);
    assertFalse(editableField.queryCommandValue('boo'));
    assertObjectEquals(
        {'boo': false, 'aieee': false},
        editableField.queryCommandValue(['boo', 'aieee']));

    editableField.makeEditable();
    assertFalse(editableField.queryCommandValue('boo'));

    focusFieldSync(editableField);
    assertNull(editableField.queryCommandValue('boo'));
    assertObjectEquals(
        {'boo': null, 'aieee': null},
        editableField.queryCommandValue(['boo', 'aieee']));
    editableField.dispose();
  },

  testSetSafeHtml() {
    const editableField = new FieldConstructor('testField', document);
    const clock = new MockClock(true);

    try {
      let delayedChangeCalled = false;
      events.listen(editableField, Field.EventType.DELAYEDCHANGE, () => {
        delayedChangeCalled = true;
      });

      editableField.makeEditable();
      clock.tick(1000);
      assertFalse(
          'Make editable must not fire delayed change.', delayedChangeCalled);

      editableField.setSafeHtml(
          false, SafeHtml.htmlEscape('bar'),
          true /* Don't fire delayed change */);
      testingDom.assertHtmlContentsMatch('bar', editableField.getElement());
      clock.tick(1000);
      assertFalse(
          'setSafeHtml must not fire delayed change if so configured.',
          delayedChangeCalled);

      editableField.setSafeHtml(
          false, SafeHtml.htmlEscape('foo'), false /* Fire delayed change */);
      testingDom.assertHtmlContentsMatch('foo', editableField.getElement());
      clock.tick(1000);
      assertTrue(
          'setSafeHtml must fire delayed change by default',
          delayedChangeCalled);
    } finally {
      clock.dispose();
      editableField.dispose();
    }
  },

  /**
     Verify that restoreSavedRange() restores the range and sets the focus.
   */
  testRestoreSavedRange() {
    const editableField = new FieldConstructor('testField', document);
    editableField.makeEditable();

    // Create another node to take the focus later.
    const doc = googDom.getOwnerDocument(editableField.getElement());
    const dom = googDom.getDomHelper(editableField.getElement());
    const otherElem = dom.createElement(TagName.DIV);
    otherElem.tabIndex = 1;  // Make it focusable.
    editableField.getElement().parentNode.appendChild(otherElem);

    // Initially place selection not at the start of the editable field.
    const textNode = editableField.getElement().firstChild;
    const range = Range.createFromNodes(textNode, 1, textNode, 2);
    range.select();
    const savedRange = editorRange.saveUsingNormalizedCarets(range);

    // Change range to be a simple cursor at start, and move focus away.
    editableField.placeCursorAtStart();
    otherElem.focus();

    editableField.restoreSavedRange(savedRange);

    // Verify that we have focus and the range is restored.
    assertEquals(
        'Field should be focused', editableField.getElement(),
        googDom.getActiveElement(doc));
    const newRange = editableField.getRange();
    assertEquals('Range startNode', textNode, newRange.getStartNode());
    assertEquals('Range startOffset', 1, newRange.getStartOffset());
    assertEquals('Range endNode', textNode, newRange.getEndNode());
    assertEquals('Range endOffset', 2, newRange.getEndOffset());
  },

  testPlaceCursorAtStart() {
    doTestPlaceCursorAtStart();
  },

  testPlaceCursorAtStartEmptyField() {
    doTestPlaceCursorAtStart('');
  },

  testPlaceCursorAtStartNonImportantTextNode() {
    doTestPlaceCursorAtStart(
        '\n<span id="important">important text</span>', 'important');
  },

  testPlaceCursorAtEnd() {
    doTestPlaceCursorAtEnd();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPlaceCursorAtEndEmptyField() {
    doTestPlaceCursorAtEnd('', null, 0);
  },

  testPlaceCursorAtEndNonImportantTextNode() {
    doTestPlaceCursorAtStart(
        '\n<span id="important">important text</span>', 'important');
  },

  // Tests related to change/delayed change events.
  testClearDelayedChange() {
    const clock = new MockClock(true);
    const editableField = new FieldConstructor('testField', document);
    editableField.makeEditable();

    let delayedChangeCalled = false;
    events.listen(editableField, Field.EventType.DELAYEDCHANGE, () => {
      delayedChangeCalled = true;
    });

    // Clears delayed change timer.
    editableField.delayedChangeTimer_.start();
    editableField.clearDelayedChange();
    assertTrue(delayedChangeCalled);
    if (editableField.changeTimerGecko_) {
      assertFalse(editableField.changeTimerGecko_.isActive());
    }
    assertFalse(editableField.delayedChangeTimer_.isActive());

    // Clears delayed changes caused by changeTimerGecko_
    if (editableField.changeTimerGecko_) {
      delayedChangeCalled = false;
      editableField.changeTimerGecko_.start();
      editableField.clearDelayedChange();
      assertTrue(delayedChangeCalled);
      if (editableField.changeTimerGecko_) {
        assertFalse(editableField.changeTimerGecko_.isActive());
      }
      assertFalse(editableField.delayedChangeTimer_.isActive());
    }
    clock.dispose();
  },

  testHandleChange() {
    if (BrowserFeature.USE_MUTATION_EVENTS) {
      const editableField = new FieldConstructor('testField', document);
      editableField.makeEditable();

      editableField.changeTimerGecko_.start();
      editableField.handleChange();
      assertFalse(editableField.changeTimerGecko_.isActive());
    }
  },

  testDispatchDelayedChange() {
    const editableField = new FieldConstructor('testField', document);
    editableField.makeEditable();

    editableField.delayedChangeTimer_.start();
    editableField.dispatchDelayedChange_();
    assertFalse(editableField.delayedChangeTimer_.isActive());
  },

  testHandleWindowLevelMouseUp() {
    const editableField = new FieldConstructor('testField', document);
    if (editableField.usesIframe()) {
      // Only run this test if the editor does not use an iframe.
      return;
    }
    editableField.setUseWindowMouseUp(true);
    editableField.makeEditable();
    let selectionHasFired = false;
    events.listenOnce(editableField, Field.EventType.SELECTIONCHANGE, (e) => {
      selectionHasFired = true;
    });
    const editableElement = editableField.getElement();
    const otherElement = googDom.createDom(TagName.DIV);
    googDom.insertSiblingAfter(otherElement, document.body.lastChild);

    testingEvents.fireMouseDownEvent(editableElement);
    assertFalse(selectionHasFired);
    testingEvents.fireMouseUpEvent(otherElement);
    assertTrue(selectionHasFired);
  },

  testNoHandleWindowLevelMouseUp() {
    const editableField = new FieldConstructor('testField', document);
    editableField.setUseWindowMouseUp(false);
    editableField.makeEditable();
    let selectionHasFired = false;
    events.listenOnce(editableField, Field.EventType.SELECTIONCHANGE, (e) => {
      selectionHasFired = true;
    });
    const editableElement = editableField.getElement();
    const otherElement = googDom.createDom(TagName.DIV);
    googDom.insertSiblingAfter(otherElement, document.body.lastChild);

    testingEvents.fireMouseDownEvent(editableElement);
    assertFalse(selectionHasFired);
    testingEvents.fireMouseUpEvent(otherElement);
    assertFalse(selectionHasFired);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsGeneratingKey() {
    const regularKeyEvent = new BrowserEvent();
    regularKeyEvent.charCode = KeyCodes.A;

    const ctrlKeyEvent = new BrowserEvent();
    ctrlKeyEvent.ctrlKey = true;
    ctrlKeyEvent.metaKey = true;
    ctrlKeyEvent.charCode = KeyCodes.A;

    const imeKeyEvent = new BrowserEvent();
    imeKeyEvent.keyCode =
        229;  // indicates from an IME - see KEYS_CAUSING_CHANGES

    assertTrue(Field.isGeneratingKey_(regularKeyEvent, true));
    assertFalse(Field.isGeneratingKey_(ctrlKeyEvent, true));
    if (userAgent.WINDOWS && !userAgent.GECKO) {
      assertTrue(Field.isGeneratingKey_(imeKeyEvent, false));
    } else {
      assertFalse(Field.isGeneratingKey_(imeKeyEvent, false));
    }
  },

  testSetEditableClassName() {
    const element = googDom.getElement('testField');
    const editableField = new FieldConstructor('testField');

    assertFalse(classlist.contains(element, 'editable'));
    editableField.makeEditable();
    assertTrue(classlist.contains(element, 'editable'));
    assertEquals(
        1,
        Array.prototype.filter
            .call(classlist.get(element), functions.equalTo('editable'))
            .length);

    // Skip restore won't reset the original element's CSS classes.
    editableField.makeUneditable(true /* opt_skipRestore */);

    editableField.makeEditable();
    assertTrue(classlist.contains(element, 'editable'));
    assertEquals(
        1,
        Array.prototype.filter
            .call(classlist.get(element), functions.equalTo('editable'))
            .length);
  },
});
