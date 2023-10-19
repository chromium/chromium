// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A command is an abstraction of an action a user can do in the
 * UI.
 *
 * When the focus changes in the document for each command a canExecute event
 * is dispatched on the active element. By listening to this event you can
 * enable and disable the command by setting the event.canExecute property.
 *
 * When a command is executed a command event is dispatched on the active
 * element. Note that you should stop the propagation after you have handled the
 * command if there might be other command listeners higher up in the DOM tree.
 */

// clang-format off
import {assert} from 'chrome://resources/ash/common/assert.js';
import {define as crUiDefine} from '../../../common/js/ui.js';
import {KeyboardShortcutList} from 'chrome://resources/ash/common/keyboard_shortcut_list_js.js';
import {dispatchPropertyChange, getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {MenuItem} from './menu_item.js';
// clang-format on

  /**
   * Creates a new command element.
   * @constructor
   * @extends {HTMLElement}
   */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const Command = crUiDefine('command');

Command.prototype = {
  __proto__: HTMLElement.prototype,

  /**
   * Initializes the command.
   */
  decorate() {
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    CommandManager.init(assert(this.ownerDocument));

    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    if (this.hasAttribute('shortcut')) {
      // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
      // HTMLElement | undefined): void; setHidden(value: boolean): void;
      // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
      // shortcut: string; matchesEvent(e: Event): boolean; }'.
      this.shortcut = this.getAttribute('shortcut');
    }
  },

  /**
   * Executes the command by dispatching a command event on the given element.
   * If |element| isn't given, the active element is used instead.
   * If the command is {@code disabled} this does nothing.
   * @param {HTMLElement=} opt_element Optional element to dispatch event on.
   */
  execute(opt_element) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    if (this.disabled) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    const doc = this.ownerDocument;
    if (doc.activeElement) {
      const e = new Event('command', {bubbles: true});
      // @ts-ignore: error TS2339: Property 'command' does not exist on type
      // 'Event'.
      e.command = this;

      (opt_element || doc.activeElement).dispatchEvent(e);
    }
  },

  /**
   * Sets 'hidden' property of a Command instance which dispatches
   * 'hiddenChange' event automatically, so that associated MenuItem can
   * handle the event.
   *
   * @param {boolean} value New value of hidden property.
   */
  setHidden(value) {
    // @ts-ignore: error TS2339: Property 'hidden' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    this.hidden = value;
  },

  /**
   * Call this when there have been changes that might change whether the
   * command can be executed or not.
   * @param {Node=} opt_node Node for which to actuate command state.
   */
  canExecuteChange(opt_node) {
    dispatchCanExecuteEvent(
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type '{ __proto__: HTMLElement; decorate(): void;
        // execute(opt_element?: HTMLElement | undefined): void;
        // setHidden(value: boolean): void; canExecuteChange(opt_node?: Node |
        // undefined): void; shortcut_: string; shortcut: string;
        // matchesEvent(e: Event): boolean; }'.
        this, opt_node || this.ownerDocument.activeElement);
  },

  /**
   * The keyboard shortcut that triggers the command. This is a string
   * consisting of a key (as reported by WebKit in keydown) as
   * well as optional key modifiers joinded with a '|'.
   *
   * Multiple keyboard shortcuts can be provided by separating them by
   * whitespace.
   *
   * For example:
   *   "F1"
   *   "Backspace|Meta" for Apple command backspace.
   *   "a|Ctrl" for Control A
   *   "Delete Backspace|Meta" for Delete and Command Backspace
   *
   * @type {string}
   */
  shortcut_: '',
  get shortcut() {
    return this.shortcut_;
  },
  set shortcut(shortcut) {
    const oldShortcut = this.shortcut_;
    if (shortcut !== oldShortcut) {
      // @ts-ignore: error TS2339: Property 'keyboardShortcuts_' does not exist
      // on type '{ __proto__: HTMLElement; decorate(): void;
      // execute(opt_element?: HTMLElement | undefined): void; setHidden(value:
      // boolean): void; canExecuteChange(opt_node?: Node | undefined): void;
      // shortcut_: string; shortcut: string; matchesEvent(e: Event): boolean;
      // }'.
      this.keyboardShortcuts_ = new KeyboardShortcutList(shortcut);

      // Set this after the keyboardShortcuts_ since that might throw.
      this.shortcut_ = shortcut;
      dispatchPropertyChange(
          // @ts-ignore: error TS2345: Argument of type '{ __proto__:
          // HTMLElement; decorate(): void; execute(opt_element?: HTMLElement |
          // undefined): void; setHidden(value: boolean): void;
          // canExecuteChange(opt_node?: Node | undefined): void; shortcut_:
          // string; shortcut: string; matchesEvent(e: Event): boolean; }' is
          // not assignable to parameter of type 'EventTarget'.
          this, 'shortcut', this.shortcut_, oldShortcut);
    }
  },

  /**
   * Whether the event object matches the shortcut for this command.
   * @param {!Event} e The key event object.
   * @return {boolean} Whether it matched or not.
   */
  matchesEvent(e) {
    // @ts-ignore: error TS2339: Property 'keyboardShortcuts_' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    if (!this.keyboardShortcuts_) {
      return false;
    }
    // @ts-ignore: error TS2339: Property 'keyboardShortcuts_' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; execute(opt_element?:
    // HTMLElement | undefined): void; setHidden(value: boolean): void;
    // canExecuteChange(opt_node?: Node | undefined): void; shortcut_: string;
    // shortcut: string; matchesEvent(e: Event): boolean; }'.
    return this.keyboardShortcuts_.matchesEvent(e);
  },
};

/**
 * The label of the command.
 * @type {string}
 */
Command.prototype.label;
Object.defineProperty(
    Command.prototype, 'label',
    getPropertyDescriptor('label', PropertyKind.ATTR));

/**
 * Whether the command is disabled or not.
 * @type {boolean}
 */
Command.prototype.disabled;
Object.defineProperty(
    Command.prototype, 'disabled',
    getPropertyDescriptor('disabled', PropertyKind.BOOL_ATTR));

/**
 * Whether the command is hidden or not.
 */
Object.defineProperty(
    Command.prototype, 'hidden',
    getPropertyDescriptor('hidden', PropertyKind.BOOL_ATTR));

/**
 * Whether the command is checked or not.
 * @type {boolean}
 */
Command.prototype.checked;
Object.defineProperty(
    Command.prototype, 'checked',
    getPropertyDescriptor('checked', PropertyKind.BOOL_ATTR));

/**
 * The flag that prevents the shortcut text from being displayed on menu.
 *
 * If false, the keyboard shortcut text (eg. "Ctrl+X" for the cut command)
 * is displayed in menu when the command is associated with a menu item.
 * Otherwise, no text is displayed.
 * @type {boolean}
 */
Command.prototype.hideShortcutText;
Object.defineProperty(
    Command.prototype, 'hideShortcutText',
    getPropertyDescriptor('hideShortcutText', PropertyKind.BOOL_ATTR));

/**
 * Dispatches a canExecute event on the target.
 * @param {!Command} command The command that we are testing for.
 * @param {EventTarget} target The target element to dispatch the event on.
 */
function dispatchCanExecuteEvent(command, target) {
  const e = new CanExecuteEvent(command);
  // @ts-ignore: error TS2345: Argument of type 'CanExecuteEvent' is not
  // assignable to parameter of type 'Event'.
  target.dispatchEvent(e);
  command.disabled = !e.canExecute;
}

  /**
   * The command managers for different documents.
   * @type {!Map<!Document, !CommandManager>}
   */
  const commandManagers = new Map();

  /**
   * Keeps track of the focused element and updates the commands when the focus
   * changes.
   * @param {!Document} doc The document that we are managing the commands for.
   * @constructor
   */
  function CommandManager(doc) {
    doc.addEventListener('focus', this.handleFocus_.bind(this), true);
    // Make sure we add the listener to the bubbling phase so that elements can
    // prevent the command.
    doc.addEventListener('keydown', this.handleKeyDown_.bind(this), false);
  }

  /**
   * Initializes a command manager for the document as needed.
   * @param {!Document} doc The document to manage the commands for.
   */
  CommandManager.init = function(doc) {
    if (!commandManagers.has(doc)) {
      commandManagers.set(doc, new CommandManager(doc));
    }
  };

  CommandManager.prototype = {

    /**
     * Handles focus changes on the document.
     * @param {Event} e The focus event object.
     * @private
     * @suppress {checkTypes}
     * TODO(vitalyp): remove the suppression.
     */
    handleFocus_(e) {
      const target = e.target;

      // Ignore focus on a menu button or command item.
      // @ts-ignore: error TS2339: Property 'command' does not exist on type
      // 'EventTarget'.
      if (target.menu || target.command || (target instanceof MenuItem)) {
        return;
      }

      const commands = Array.prototype.slice.call(
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type 'EventTarget'.
          target.ownerDocument.querySelectorAll('command'));

      commands.forEach(function(command) {
        // @ts-ignore: error TS2345: Argument of type 'EventTarget | null' is
        // not assignable to parameter of type 'EventTarget'.
        dispatchCanExecuteEvent(command, target);
      });
    },

    /**
     * Handles the keydown event and routes it to the right command.
     * @param {!Event} e The keydown event.
     */
    handleKeyDown_(e) {
      const target = e.target;
      const commands = Array.prototype.slice.call(
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type 'EventTarget'.
          target.ownerDocument.querySelectorAll('command'));

      for (let i = 0, command; command = commands[i]; i++) {
        if (command.matchesEvent(e)) {
          // When invoking a command via a shortcut, we have to manually check
          // if it can be executed, since focus might not have been changed
          // what would have updated the command's state.
          command.canExecuteChange();

          if (!command.disabled) {
            e.preventDefault();
            // We do not want any other element to handle this.
            e.stopPropagation();
            command.execute();
            return;
          }
        }
      }
    },
  };

  /**
   * The event type used for canExecute events.
   * @param {!Command} command The command that we are evaluating.
   * @extends {Event}
   * @constructor
   * @class
   */
  // @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
  export function CanExecuteEvent(command) {
    const e = new Event('canExecute', {bubbles: true, cancelable: true});
    // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
    // 'Event'.
    e.__proto__ = CanExecuteEvent.prototype;
    // @ts-ignore: error TS2339: Property 'command' does not exist on type
    // 'Event'.
    e.command = command;
    return e;
  }

  CanExecuteEvent.prototype = {
    __proto__: Event.prototype,

    /**
     * The current command
     * @type {Command}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'Command'.
    command: null,

    /**
     * Whether the target can execute the command. Setting this also stops the
     * propagation and prevents the default. Callers can tell if an event has
     * been handled via |this.defaultPrevented|.
     * @type {boolean}
     */
    canExecute_: false,
    get canExecute() {
      return this.canExecute_;
    },
    set canExecute(canExecute) {
      this.canExecute_ = !!canExecute;
      // @ts-ignore: error TS2339: Property 'stopPropagation' does not exist on
      // type '{ __proto__: Event; command: Command; canExecute_: boolean;
      // canExecute: boolean; }'.
      this.stopPropagation();
      // @ts-ignore: error TS2339: Property 'preventDefault' does not exist on
      // type '{ __proto__: Event; command: Command; canExecute_: boolean;
      // canExecute: boolean; }'.
      this.preventDefault();
    },
  };
