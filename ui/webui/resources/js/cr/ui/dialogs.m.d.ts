// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class BaseDialog {
  constructor(parentNode: any);
  setOkLabel(label: string): void;
  setCancelLabel(label: string): void;
  setInitialFocusOnCancel(): void;
  show(
      message: string, onOk?: Function|undefined, onCancel?: Function|undefined,
      onShow?: Function|undefined): void;
  showHtml(
      title: string, message: string, onOk?: Function|undefined,
      onCancel?: Function|undefined, onShow?: Function|undefined): void;
  showWithTitle(
      title: string, message: string, onOk?: Function|undefined,
      onCancel?: Function|undefined, onShow?: Function|undefined): void;
  hide(onHide?: Function|undefined): void;

  /* eslint-disable @typescript-eslint/naming-convention */
  OK_LABEL: string;
  CANCEL_LABEL: string;
  ANIMATE_STABLE_DURATION: number;
  /* eslint-enable @typescript-eslint/naming-convention */
}

export class AlertDialog extends BaseDialog {
  constructor(parentNode: any);
  show(message: any, onOk?: Function|undefined, onShow?: Function|undefined):
      any;
}

export class ConfirmDialog extends BaseDialog {
  constructor(parentNode: any);
}
