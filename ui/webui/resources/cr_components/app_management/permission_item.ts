// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App, InstallReason, Permission, PermissionType, TriState} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {PermissionTypeIndex} from './permission_constants.js';
import {getTemplate} from './permission_item.html.js';
import {createBoolPermission, createTriStatePermission, getBoolPermissionValue, getTriStatePermissionValue, isBoolValue, isTriStateValue} from './permission_util.js';
import {AppManagementToggleRowElement} from './toggle_row.js';
import {getPermission, getPermissionValueBool, recordAppManagementUserAction} from './util.js';

export class AppManagementPermissionItemElement extends PolymerElement {
  static get is() {
    return 'app-management-permission-item';
  }

  static get template() {
    return getTemplate();
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
      permissionType: {
        type: String,
        reflectToAttribute: true,
      },

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

      /**
       * True if the app is managed or is a sub app.
       */
      disabled_: {
        type: Boolean,
        computed: 'isDisabled_(app, permissionType)',
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

  private isAvailable_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    return getPermission(app, permissionType) !== undefined;
  }

  private isManaged_(app: App|undefined, permissionType: PermissionTypeIndex):
      boolean {
    if (app === undefined || permissionType === undefined ||
        !this.isAvailable_(app, permissionType)) {
      return false;
    }

    const permission = getPermission(app, permissionType);
    assert(permission);
    return permission.isManaged;
  }

  private isDisabled_(app: App|undefined, permissionType: PermissionTypeIndex):
      boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    if (app.installReason === InstallReason.kSubApp) {
      return true;
    }

    return this.isManaged_(app, permissionType);
  }

  private getValue_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }
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
    let newPermission: Permission|undefined = undefined;

    let newBoolState = false;
    const permission = getPermission(this.app, this.permissionType);
    assert(permission);
    const permissionValue = permission.value;
    if (isBoolValue(permissionValue)) {
      newPermission =
          this.getUiPermissionBoolean_(this.app, this.permissionType);
      newBoolState = getBoolPermissionValue(newPermission.value);
    } else if (isTriStateValue(permissionValue)) {
      newPermission =
          this.getUiPermissionTriState_(this.app, this.permissionType);

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
  private getUiPermissionBoolean_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    const currentPermission = getPermission(app, permissionType);
    assert(currentPermission);

    assert(isBoolValue(currentPermission.value));

    const newPermissionValue = !getBoolPermissionValue(currentPermission.value);

    return createBoolPermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  }

  /**
   * Gets the permission tristate based on the toggle's UI state.
   */
  private getUiPermissionTriState_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    let newPermissionValue;
    const currentPermission = getPermission(app, permissionType);
    assert(currentPermission);

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
    }

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
