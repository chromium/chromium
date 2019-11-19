// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox User Event Detail object.
 *
 * This is the detail object for a CustomEvent that ChromeVox sends to the
 * current node in the DOM when the user wants to perform a ChromeVox action
 * that could potentially be handled better by the underlying web app.
 *
 * ChromeVox events are sent with status "PENDING" and an action that maps to
 * the command list in ChromeVoxUserCommands.
 *
 * If a web app wishes to handle the action, it can perform the action then set
 * the status of the event to either "SUCCESS" or "FAILURE".
 *
 * When the event bubbles back up to the document, ChromeVox will process it.
 * If the status is "PENDING", ChromeVox will perform the action itself.
 * If the status is "FAILURE", ChromeVox will speak the associated error message
 * for that action. (For example: "No next heading.")
 * If the status is "SUCCESS", then ChromeVox will check to see if an associated
 * resultNode is added to the event. If this node exists, then ChromeVox will
 * treat this node as the result of the action and sync/speak it as if it had
 * gotten to this node through ChromeVox's default algorithm. If this field is
 * left as null, then ChromeVox assumes that the web app has already handled
 * speaking/syncing to the result and will do nothing more for this action.
 *
 */

goog.provide('cvox.UserEventDetail');

goog.require('cvox.ChromeVox');



/**
 * Enables web apps to use its own algorithms to handle certain ChromeVox
 * actions where the web app may be able to do a better job than the default
 * ChromeVox algorithms because it has much more information about its own
 * content and functionality.
 * @constructor
 *
 * @param {Object<{command: (string),
 *                         status: (undefined|string),
 *                         resultNode: (undefined|Node),
 *                         customCommand: (undefined|string)
 *                         }>} detailObj
 * command: The ChromeVox UserCommand to be performed.
 * status: The status of the event. If the status is left at PENDING, ChromeVox
 * will run its default algorithm for performing the action. Otherwise, it means
 * the underlying web app has performed the action itself. If the status is set
 * to FAILURE, ChromeVox will speak the default error message for this command
 * to the user.
 * resultNode: The result of the action if it has been performed and there is a
 * result. If this is a valid node and the status is set to SUCCESS, ChromeVox
 * will sync to this node and speak it.
 * customCommand: The custom command to be performed. This is designed to allow
 * web apps / other extensions to define custom actions that can be shown in
 * ChromeVox (for example, inside the context menu) and then dispatched back to
 * the page.
 */
cvox.UserEventDetail = function(detailObj) {
  /**
   * The category of command that should be performed.
   * @type {string}
   */
  this.category = '';

  /**
   * The user command that should be performed.
   * @type {string}
   */
  this.command = '';
  if (cvox.UserEventDetail.JUMP_COMMANDS.indexOf(detailObj.command) != -1) {
    this.command = detailObj.command;
    this.category = cvox.UserEventDetail.Category.JUMP;
  }

  /**
   * The custom command that should be performed.
   * @type {string}
   */
  this.customCommand = '';
  if (detailObj.customCommand) {
    this.customCommand = detailObj.customCommand;
    this.category = cvox.UserEventDetail.Category.CUSTOM;
  }

  /**
   * The status of the event.
   * @type {string}
   */
  this.status = cvox.UserEventDetail.Status.PENDING;
  switch (detailObj.status) {
    case cvox.UserEventDetail.Status.SUCCESS:
      this.status = cvox.UserEventDetail.Status.SUCCESS;
      break;
    case cvox.UserEventDetail.Status.FAILURE:
      this.status = cvox.UserEventDetail.Status.FAILURE;
      break;
  }

  /**
   * The result of performing the command.
   *
   * @type {Node}
   */
  this.resultNode = null;
  if (detailObj.resultNode &&
      cvox.DomUtil.isAttachedToDocument(detailObj.resultNode)) {
    this.resultNode = detailObj.resultNode;
  }
};

/**
 * Category of the user event. This is the event name that the web app should
 * be listening for.
 * @enum {string}
 */
cvox.UserEventDetail.Category = {
  JUMP: 'ATJumpEvent',
  CUSTOM: 'ATCustomEvent'
};

/**
 * Status of the cvoxUserEvent. Events start off as PENDING. If the underlying
 * web app has handled this event, it should set the event to either SUCCESS or
 * FAILURE to prevent ChromeVox from trying to redo the same command.
 * @enum {string}
 */
cvox.UserEventDetail.Status = {
  PENDING: 'PENDING',
  SUCCESS: 'SUCCESS',
  FAILURE: 'FAILURE'
};


/**
 * List of commands that are dispatchable to the page.
 *
 * @type {Array}
 */
// TODO (clchen): Integrate this with command_store.js.
cvox.UserEventDetail.JUMP_COMMANDS = [
  'nextCheckbox', 'previousCheckbox', 'nextRadio', 'previousRadio',
  'nextSlider', 'previousSlider', 'nextGraphic', 'previousGraphic',
  'nextButton', 'previousButton', 'nextComboBox', 'previousComboBox',
  'nextEditText', 'previousEditText', 'nextHeading', 'previousHeading',
  'nextHeading1', 'previousHeading1', 'nextHeading2', 'previousHeading2',
  'nextHeading3', 'previousHeading3', 'nextHeading4', 'previousHeading4',
  'nextHeading5', 'previousHeading5', 'nextHeading6', 'previousHeading6',
  'nextLink', 'previousLink', 'nextMath', 'previousMath', 'nextTable',
  'previousTable', 'nextList', 'previousList', 'nextListItem',
  'previousListItem', 'nextFormField', 'previousFormField', 'nextLandmark',
  'previousLandmark', 'nextSection', 'previousSection', 'nextControl',
  'previousControl'
];

/**
 * Creates a custom event object from the UserEventDetail.
 *
 * @return {!Event} A custom event object that can be dispatched to the page.
 */
cvox.UserEventDetail.prototype.createEventObject = function() {
  // We use CustomEvent so that it will go all the way to the page and come back
  // into the ChromeVox context correctly.
  var evt = document.createEvent('CustomEvent');
  evt.initCustomEvent(this.category, true, true, this);
  return evt;
};
