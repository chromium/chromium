/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.ActionHandlerTest');
goog.setTestOnly();

const ActionHandler = goog.require('goog.events.ActionHandler');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let actionHandler;

// Tests to see that both the BEFOREACTION and ACTION events are fired

// Tests to see that the ACTION event is fired, even if there is no
// BEFOREACTION handler.

// If the BEFOREACTION listener swallows the event, it should cancel the
// ACTION event.

testSuite({
  setUp() {
    actionHandler = new ActionHandler(dom.getElement('actionDiv'));
  },

  tearDown() {
    actionHandler.dispose();
  },

  testActionHandlerWithBeforeActionHandler() {
    let actionEventFired = false;
    let beforeActionFired = false;
    events.listen(actionHandler, ActionHandler.EventType.ACTION, (e) => {
      actionEventFired = true;
    });
    events.listen(actionHandler, ActionHandler.EventType.BEFOREACTION, (e) => {
      beforeActionFired = true;
    });
    testingEvents.fireClickSequence(dom.getElement('actionDiv'));
    assertTrue('BEFOREACTION event was not fired', beforeActionFired);
    assertTrue('ACTION event was not fired', actionEventFired);
  },

  testActionHandlerWithoutBeforeActionHandler() {
    let actionEventFired = false;
    events.listen(actionHandler, ActionHandler.EventType.ACTION, (e) => {
      actionEventFired = true;
    });
    testingEvents.fireClickSequence(dom.getElement('actionDiv'));
    assertTrue('ACTION event was not fired', actionEventFired);
  },

  testBeforeActionCancel() {
    const actionEventFired = false;
    let beforeActionFired = false;
    events.listen(actionHandler, ActionHandler.EventType.ACTION, (e) => {
      /** @suppress {undefinedVars} suppression added to enable type checking */
      actionEvent = e;
    });
    events.listen(actionHandler, ActionHandler.EventType.BEFOREACTION, (e) => {
      beforeActionFired = true;
      e.preventDefault();
    });
    testingEvents.fireClickSequence(dom.getElement('actionDiv'));
    assertTrue(beforeActionFired);
    assertFalse(actionEventFired);
  },
});
