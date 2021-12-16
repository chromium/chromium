// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './shared_style.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {PermissionType, PermissionValue, TriState} from './permission_constants.js';
import {createBoolPermission, createTriStatePermission, getBoolPermissionValue, getTriStatePermissionValue, isBoolValue, isTriStateValue} from './permission_util.js';
import {getPermission, getPermissionValueBool, getSelectedApp, recordAppManagementUserAction} from './util.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-permission-item',


  properties: {
    /**
     * The name of the permission, to be displayed to the user.
     * @type {string}
     */
    permissionLabel: String,

    /**
     * A string version of the permission type. Must be a value of the
     * permission type enum in apps.mojom.PermissionType.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {string}
     */
    icon: String,

    /**
     * If set to true, toggling the permission item will not set the permission
     * in the backend. Call `syncPermission()` to set the permission to reflect
     * the current UI state.
     *
     * @type {boolean}
     */
    syncPermissionManually: Boolean,

    /**
     * @type {App}
     */
    app_: Object,

    /**
     * True if the permission type is available for the app.
     * @type {boolean}
     * @private
     */
    available_: {
      type: Boolean,
      computed: 'isAvailable_(app_, permissionType)',
      reflectToAttribute: true,
    },

    /**
     * @type {boolean}
     * @private
     */
    disabled_: {
      type: Boolean,
      computed: 'isManaged_(app_, permissionType)',
      reflectToAttribute: true,
    },
  },


  listeners: {click: 'onClick_', change: 'togglePermission_'},

  /**
   * Returns true if the permission type is available for the app.
   *
   * @param {App} app
   * @param {string} permissionType
   * @private
   */
  isAvailable_(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return getPermission(app, permissionType) !== undefined;
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  isManaged_(app, permissionType) {
    if (app === undefined || permissionType === undefined ||
        !this.isAvailable_(app, permissionType)) {
      return false;
    }

    assert(app);
    const permission = getPermission(app, permissionType);

    assert(permission);
    return permission.isManaged;
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  getValue_(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }
    assert(app);

    return getPermissionValueBool(app, permissionType);
  },

  resetToggle() {
    const currentValue = this.getValue_(this.app_, this.permissionType);
    this.$$('#toggle-row').setToggle(currentValue);
  },

  /**
   * @private
   */
  onClick_() {
    this.$$('#toggle-row').click();
  },

  /**
   * @private
   */
  togglePermission_() {
    if (!this.syncPermissionManually) {
      this.syncPermission();
    }
  },

  /**
   * Set the permission to match the current UI state. This only needs to be
   * called when `syncPermissionManually` is set.
   */
  syncPermission() {
    assert(this.app_);

    /** @type {!Permission} */
    let newPermission;

    let newBoolState = false;  // to keep the closure compiler happy.
    const permissionValue = getPermission(this.app_, this.permissionType).value;
    if (isBoolValue(permissionValue)) {
      newPermission =
          this.getUIPermissionBoolean_(this.app_, this.permissionType);
      newBoolState = getBoolPermissionValue(newPermission.value);
    } else if (isTriStateValue(permissionValue)) {
      newPermission =
          this.getUIPermissionTriState_(this.app_, this.permissionType);

      newBoolState =
          getTriStatePermissionValue(newPermission.value) === TriState.kAllow;
    } else {
      assertNotReached();
    }

    BrowserProxy.getInstance().handler.setPermission(
        this.app_.id, newPermission);

    recordAppManagementUserAction(
        this.app_.type,
        this.getUserMetricActionForPermission_(
            newBoolState, this.permissionType));
  },

  /**
   * Gets the permission boolean based on the toggle's UI state.
   *
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getUIPermissionBoolean_(app, permissionType) {
    const currentPermission = getPermission(app, permissionType);

    assert(isBoolValue(currentPermission.value));

    const newPermissionValue = !getBoolPermissionValue(currentPermission.value);

    return createBoolPermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  },

  /**
   * Gets the permission tristate based on the toggle's UI state.
   *
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getUIPermissionTriState_(app, permissionType) {
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
    }

    assert(newPermissionValue !== undefined);
    return createTriStatePermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  },

  /**
   * @param {boolean} permissionValue
   * @param {string} permissionType
   * @return {AppManagementUserAction}
   * @private
   */
  getUserMetricActionForPermission_(permissionValue, permissionType) {
    switch (permissionType) {
      case 'kNotifications':
        return permissionValue ? AppManagementUserAction.NotificationsTurnedOn :
                                 AppManagementUserAction.NotificationsTurnedOff;

      case 'kLocation':
        return permissionValue ? AppManagementUserAction.LocationTurnedOn :
                                 AppManagementUserAction.LocationTurnedOff;

      case 'kCamera':
        return permissionValue ? AppManagementUserAction.CameraTurnedOn :
                                 AppManagementUserAction.CameraTurnedOff;

      case 'kMicrophone':
        return permissionValue ? AppManagementUserAction.MicrophoneTurnedOn :
                                 AppManagementUserAction.MicrophoneTurnedOff;

      case 'kContacts':
        return permissionValue ? AppManagementUserAction.ContactsTurnedOn :
                                 AppManagementUserAction.ContactsTurnedOff;

      case 'kStorage':
        return permissionValue ? AppManagementUserAction.StorageTurnedOn :
                                 AppManagementUserAction.StorageTurnedOff;

      case 'kPrinting':
        return permissionValue ? AppManagementUserAction.PrintingTurnedOn :
                                 AppManagementUserAction.PrintingTurnedOff;

      default:
        assertNotReached();
    }
  },
});
