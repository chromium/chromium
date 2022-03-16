// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './shared_style.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {Permission} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {PermissionType, PermissionTypeIndex, TriState} from './permission_constants.js';
import {createBoolPermission, createTriStatePermission, getBoolPermissionValue, getTriStatePermissionValue, isBoolValue, isTriStateValue} from './permission_util.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {getPermission, getPermissionValueBool, recordAppManagementUserAction} from './util.js';

export class AppManagementPermissionItemElement extends PolymerElement {
  static get is() {
    return 'app-management-permission-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The name of the permission, to be displayed to the user.
       */
      permissionLabel: String,

      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: String,

      icon: String,

      /**
       * If set to true, toggling the permission item will not set the
       * permission in the backend. Call `syncPermission()` to set the
       * permission to reflect the current UI state.
       */
      syncPermissionManually: Boolean,

      app: Object,

      /**
       * True if the permission type is available for the app.
       */
      available_: {
        type: Boolean,
        computed: 'isAvailable_(app, permissionType)',
        reflectToAttribute: true,
      },

      disabled_: {
        type: Boolean,
        computed: 'isManaged_(app, permissionType)',
        reflectToAttribute: true,
      },
    };
  }

  app: App;
  permissionLabel: string;
  permissionType: PermissionTypeIndex;
  icon: string;
  private syncPermissionManually: boolean;
  private available_: boolean;
  private disabled_: boolean;


  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.togglePermission_);
  }

  private isAvailable_(app: App, permissionType: PermissionTypeIndex): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return getPermission(app, permissionType) !== undefined;
  }

  private isManaged_(app: App, permissionType: PermissionTypeIndex): boolean {
    if (app === undefined || permissionType === undefined ||
        !this.isAvailable_(app, permissionType)) {
      return false;
    }

    assert(app);
    const permission = getPermission(app, permissionType);

    assert(permission);
    return permission.isManaged;
  }

  private getValue_(app: App, permissionType: PermissionTypeIndex): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }
    assert(app);

    return getPermissionValueBool(app, permissionType);
  }

  resetToggle() {
    const currentValue = this.getValue_(this.app, this.permissionType);
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.setToggle(
            currentValue);
  }

  private onClick_() {
    this.shadowRoot!
        .querySelector<AppManagementToggleRowElement>('#toggle-row')!.click();
  }

  private togglePermission_() {
    if (!this.syncPermissionManually) {
      this.syncPermission();
    }
  }

  /**
   * Set the permission to match the current UI state. This only needs to be
   * called when `syncPermissionManually` is set.
   */
  syncPermission() {
    assert(this.app);

    let newPermission: Permission|undefined = undefined;

    let newBoolState = false;  // to keep the closure compiler happy.
    const permissionValue = getPermission(this.app, this.permissionType).value;
    if (isBoolValue(permissionValue)) {
      newPermission =
          this.getUIPermissionBoolean_(this.app, this.permissionType);
      newBoolState = getBoolPermissionValue(newPermission.value);
    } else if (isTriStateValue(permissionValue)) {
      newPermission =
          this.getUIPermissionTriState_(this.app, this.permissionType);

      newBoolState =
          getTriStatePermissionValue(newPermission.value) === TriState.kAllow;
    } else {
      assertNotReached();
    }

    BrowserProxy.getInstance().handler.setPermission(
        this.app.id, newPermission!);

    recordAppManagementUserAction(
        this.app.type,
        this.getUserMetricActionForPermission_(
            newBoolState, this.permissionType));
  }

  /**
   * Gets the permission boolean based on the toggle's UI state.
   */
  private getUIPermissionBoolean_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    const currentPermission = getPermission(app, permissionType);

    assert(isBoolValue(currentPermission.value));

    const newPermissionValue = !getBoolPermissionValue(currentPermission.value);

    return createBoolPermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  }

  /**
   * Gets the permission tristate based on the toggle's UI state.
   */
  private getUIPermissionTriState_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    let newPermissionValue;
    const currentPermission = getPermission(app, permissionType);

    assert(isTriStateValue(currentPermission.value));

    switch (getTriStatePermissionValue(currentPermission.value)) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        // TODO(rekanorman): Eventually TriState.kAsk, but currently changing a
        // permission to kAsk then opening the site settings page for the app
        // produces the error:
        // "Only extensions or enterprise policy can change the setting to ASK."
        newPermissionValue = TriState.kBlock;
        break;
      default:
        assertNotReached();
        newPermissionValue = TriState.kBlock;
    }

    assert(newPermissionValue !== undefined);
    return createTriStatePermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  }

  private getUserMetricActionForPermission_(
      permissionValue: boolean,
      permissionType: PermissionTypeIndex): AppManagementUserAction {
    switch (permissionType) {
      case 'kNotifications':
        return permissionValue ?
            AppManagementUserAction.NOTIFICATIONS_TURNED_ON :
            AppManagementUserAction.NOTIFICATIONS_TURNED_OFF;

      case 'kLocation':
        return permissionValue ? AppManagementUserAction.LOCATION_TURNED_ON :
                                 AppManagementUserAction.LOCATION_TURNED_OFF;

      case 'kCamera':
        return permissionValue ? AppManagementUserAction.CAMERA_TURNED_ON :
                                 AppManagementUserAction.CAMERA_TURNED_OFF;

      case 'kMicrophone':
        return permissionValue ? AppManagementUserAction.MICROPHONE_TURNED_ON :
                                 AppManagementUserAction.MICROPHONE_TURNED_OFF;

      case 'kContacts':
        return permissionValue ? AppManagementUserAction.CONTACTS_TURNED_ON :
                                 AppManagementUserAction.CONTACTS_TURNED_OFF;

      case 'kStorage':
        return permissionValue ? AppManagementUserAction.STORAGE_TURNED_ON :
                                 AppManagementUserAction.STORAGE_TURNED_OFF;

      case 'kPrinting':
        return permissionValue ? AppManagementUserAction.PRINTING_TURNED_ON :
                                 AppManagementUserAction.PRINTING_TURNED_OFF;

      default:
        assertNotReached();
        return AppManagementUserAction.NOTIFICATIONS_TURNED_ON;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-permission-item': AppManagementPermissionItemElement;
  }
}

customElements.define(
    AppManagementPermissionItemElement.is, AppManagementPermissionItemElement);
