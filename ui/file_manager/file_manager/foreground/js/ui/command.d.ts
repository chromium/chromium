// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Command extends HTMLElement {
  new(...args: any[]): Command;
  label: string;
  disabled: boolean;
  hidden: any;
  checked: boolean;
  hideShortcutText: boolean;
  execute(element?: HTMLElement): void;
  canExecuteChange(node?: Node): void;
}
