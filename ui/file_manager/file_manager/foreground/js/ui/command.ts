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

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {KeyboardShortcutList} from 'chrome://resources/ash/common/keyboard_shortcut_list_js.js';
import {assert} from 'chrome://resources/js/assert.js';

import {boolAttrSetter, convertToKebabCase, domAttrSetter} from '../../../common/js/cr_ui.js';

import {MenuItem} from './menu_item.js';

/**
 * Creates a new command element.
 */
export class Command extends HTMLElement {
  private shortcut_: string|null = null;
  private keyboardShortcuts_: KeyboardShortcutList|null = null;

  /**
   * Initializes the command.
   */
  initialize() {
    assert(this.ownerDocument);
    CommandManager.init(this.ownerDocument);

    if (this.hasAttribute('shortcut')) {
      this.shortcut = this.getAttribute('shortcut')!;
    }
  }

  /**
   * Executes the command by dispatching a command event on the given element.
   * If `element` isn't given, the active element is used instead.
   * If the command is `disabled` this does nothing.
   * @param element Optional element to dispatch event on.
   */
  execute(element?: HTMLElement|null) {
    if (this.disabled) {
      return;
    }

    const doc = this.ownerDocument;
    if (doc.activeElement) {
      const e = new CustomEvent('command', {
        bubbles: true,
        detail: {
          command: this,
        },
      });

      (element || doc.activeElement).dispatchEvent(e);
    }
  }

  /**
   * Sets 'hidden' property of a Command instance which dispatches
   * 'hiddenChange' event automatically, so that associated MenuItem can
   * handle the event.
   *
   * @param value New value of hidden property.
   */
  setHidden(value: boolean) {
    this.hidden = value;
  }

  /**
   * Call this when there have been changes that might change whether the
   * command can be executed or not.
   * @param node Node for which to actuate command state.
   */
  canExecuteChange(node?: Node|null) {
    dispatchCanExecuteEvent(this, node ?? this.ownerDocument.activeElement!);
  }

  /**
   * The keyboard shortcut that triggers the command. This is a string
   * consisting of a key (as reported by WebKit in keydown) as
   * well as optional key modifiers joined with a '|'.
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
   */
  get shortcut() {
    return this.shortcut_ ?? '';
  }

  set shortcut(shortcut: string) {
    const oldShortcut = this.shortcut_;
    if (shortcut !== oldShortcut) {
      this.keyboardShortcuts_ = new KeyboardShortcutList(shortcut);

      // Set this after the keyboardShortcuts_ since that might throw.
      this.shortcut_ = shortcut;
      dispatchPropertyChange(this, 'shortcut', this.shortcut_, oldShortcut);
    }
  }

  /**
   * Whether the event object matches the shortcut for this command.
   * @param e The key event object.
   * @return Whether it matched or not.
   */
  matchesEvent(e: Event): boolean {
    if (!this.keyboardShortcuts_) {
      return false;
    }
    return this.keyboardShortcuts_.matchesEvent(e);
  }

  /**
   * The label of the command.
   */
  get label(): string {
    return this.getAttribute(convertToKebabCase('label')) ?? '';
  }

  set label(value: string) {
    domAttrSetter(this, 'label', value);
  }

  /**
   * Whether the command is disabled or not.
   */
  get disabled(): boolean {
    return this.hasAttribute(convertToKebabCase('disabled'));
  }

  set disabled(value: boolean) {
    boolAttrSetter(this, 'disabled', value);
  }

  /**
   * Whether the command is hidden or not.
   */
  override get hidden(): boolean {
    return this.hasAttribute(convertToKebabCase('hidden'));
  }

  override set hidden(value: boolean) {
    boolAttrSetter(this, 'hidden', value);
  }

  /**
   * Whether the command is checked or not.
   */
  get checked(): boolean {
    return this.hasAttribute(convertToKebabCase('checked'));
  }

  set checked(value: boolean) {
    boolAttrSetter(this, 'checked', value);
  }

  /**
   * The flag that prevents the shortcut text from being displayed on menu.
   *
   * If false, the keyboard shortcut text (eg. "Ctrl+X" for the cut command)
   * is displayed in menu when the command is associated with a menu item.
   * Otherwise, no text is displayed.
   */
  get hideShortcutText(): boolean {
    return this.hasAttribute(convertToKebabCase('hideShortcutText'));
  }

  set hideShortcutText(value: boolean) {
    boolAttrSetter(this, 'hideShortcutText', value);
  }
}

/**
 * Dispatches a canExecute event on the target.
 * @param command The command that we are testing for.
 * @param target The target element to dispatch the event on.
 */
function dispatchCanExecuteEvent(command: Command, target: Node|HTMLElement) {
  const e = new CanExecuteEvent(command);
  target.dispatchEvent(e);
  command.disabled = !e.canExecute;
}

/**
 * The command managers for different documents.
 */
const commandManagers = new Map<Document, CommandManager>();

/**
 * Keeps track of the focused element and updates the commands when the focus
 * changes.
 */
class CommandManager {
  /**
   * @param doc The document that we are managing the commands for.
   */
  constructor(doc: Document) {
    doc.addEventListener('focus', this.handleFocus_.bind(this), true);
    // Make sure we add the listener to the bubbling phase so that elements can
    // prevent the command.
    doc.addEventListener('keydown', this.handleKeyDown_.bind(this), false);
  }

  /**
   * Initializes a command manager for the document as needed.
   * @param doc The document to manage the commands for.
   */
  static init(doc: Document) {
    if (!commandManagers.has(doc)) {
      commandManagers.set(doc, new CommandManager(doc));
    }
  }

  /**
   * Handles focus changes on the document.
   * @param e The focus event object.
   */
  private handleFocus_(e: Event) {
    const target = e.target as HTMLElement;

    // Ignore focus on a menu button or command item.
    if ('menu' in target || 'command' in target ||
        (target instanceof MenuItem)) {
      return;
    }

    for (const command of target.ownerDocument.querySelectorAll<Command>(
             'command')) {
      dispatchCanExecuteEvent(command, target);
    }
  }

  /**
   * Handles the keydown event and routes it to the right command.
   * @param e The keydown event.
   */
  private handleKeyDown_(e: Event) {
    const target = e.target as HTMLElement;

    for (const command of target.ownerDocument.querySelectorAll<Command>(
             'command')) {
      if (!command.matchesEvent) {
        // Because Command uses injected methods the <command> in the DOM might
        // not have been initialized yet.
        continue;
      }

      if (!command.matchesEvent(e)) {
        continue;
      }
      // When invoking a command via a shortcut, we have to manually check if it
      // can be executed, since focus might not have been changed what would
      // have updated the command's state.
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
}

/**
 * The event type used for canExecute events.
 */
export class CanExecuteEvent extends Event {
  /**
   * Whether the target can execute the command. Setting this also stops the
   * propagation and prevents the default. Callers can tell if an event has
   * been handled via |this.defaultPrevented|.
   */
  private canExecute_ = false;

  /**
   * @param command The command that we are evaluating.
   */
  constructor(public command: Command) {
    super('canExecute', {bubbles: true, cancelable: true});
  }

  get canExecute() {
    return this.canExecute_;
  }

  set canExecute(canExecute) {
    this.canExecute_ = !!canExecute;
    this.stopPropagation();
    this.preventDefault();
  }
}

// Event triggered when a Command is executed.
export type CommandEvent = CustomEvent<{command: Command}>;

// These event can bubble, so we can listen to it in any element parent of the
// <command>.
declare global {
  interface HTMLElementEventMap {
    'command': CommandEvent;
    'canExecute': CanExecuteEvent;
  }
}
