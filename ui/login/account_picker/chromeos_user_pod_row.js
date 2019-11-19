// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Variables used for pod placement processing. Width and height should be
   * synced with computed CSS sizes of pods.
   */
  var CROS_POD_WIDTH = 306;
  var CROS_SMALL_POD_WIDTH = 304;
  var CROS_EXTRA_SMALL_POD_WIDTH = 282;
  var DESKTOP_POD_WIDTH = 180;
  var MD_DESKTOP_POD_WIDTH = 160;
  var PUBLIC_EXPANDED_WIDTH = 622;
  var CROS_POD_HEIGHT = 346;
  var CROS_SMALL_POD_HEIGHT = 74;
  var CROS_EXTRA_SMALL_POD_HEIGHT = 60;
  var DESKTOP_POD_HEIGHT = 226;
  var MD_DESKTOP_POD_HEIGHT = 200;
  var PUBLIC_EXPANDED_HEIGHT = 324;
  var POD_ROW_PADDING = 10;
  var DESKTOP_ROW_PADDING = 32;
  var CUSTOM_ICON_CONTAINER_SIZE = 40;
  var CROS_PIN_POD_HEIGHT = 417;
  var SCROLL_MASK_HEIGHT = 112;
  var CROS_POD_HEIGHT_WITH_PIN = 618;
  var PUBLIC_SESSION_ICON_WIDTH = 12;
  var CROS_POD_WARNING_BANNER_OFFSET_Y = 270;

  /**
   * The maximum number of users that each pod placement method can handle.
   */
  var POD_ROW_LIMIT = 2;
  var LANDSCAPE_MODE_LIMIT = 6;
  var PORTRAIT_MODE_LIMIT = 9;

  /**
   * Distance between the bubble and user pod.
   * @type {number}
   * @const
   */
  var BUBBLE_POD_OFFSET = 4;

  /**
   * Maximum time for which the pod row remains hidden until all user images
   * have been loaded.
   * @type {number}
   * @const
   */
  var POD_ROW_IMAGES_LOAD_TIMEOUT_MS = 3000;

  /**
   * The duration of the animation for switching between main pod and small
   * pod. It should be synced with CSS.
   * @type {number}
   * @const
   */
  var POD_SWITCH_ANIMATION_DURATION_MS = 180;

  /**
   * Tab order for user pods. Update these when adding new controls.
   * @enum {number}
   * @const
   */
  var UserPodTabOrder = {
    POD_INPUT: 1,          // Password input field, action box menu button and
                           // the pod itself.
    PIN_KEYBOARD: 2,       // Pin keyboard below the password input field.
    POD_CUSTOM_ICON: 3,    // Pod custom icon next to password input field.
    HEADER_BAR: 4,         // Buttons on the header bar (Shutdown, Add User).
    POD_MENU_ITEM: 5       // User pod menu items (User info, Remove user).
  };

  /**
   * Supported authentication types. Keep in sync with the enum in
   * components/proximity_auth/public/interfaces/auth_type.mojom
   * @enum {number}
   * @const
   */
  var AUTH_TYPE = {
    OFFLINE_PASSWORD: 0,
    ONLINE_SIGN_IN: 1,
    NUMERIC_PIN: 2,
    USER_CLICK: 3,
    EXPAND_THEN_USER_CLICK: 4,
    FORCE_OFFLINE_PASSWORD: 5
  };

  /**
   * Names of authentication types.
   */
  var AUTH_TYPE_NAMES = {
    0: 'offlinePassword',
    1: 'onlineSignIn',
    2: 'numericPin',
    3: 'userClick',
    4: 'expandThenUserClick',
    5: 'forceOfflinePassword'
  };

  /**
   * Supported fingerprint unlock states.
   * @enum {number}
   * @const
   */
  var FINGERPRINT_STATES = {
    HIDDEN: 0,
    DEFAULT: 1,
    SIGNIN: 2,
    FAILED: 3,
  };

  /**
   * The fingerprint states to classes mapping.
   * {@code state} properties indicate current fingerprint unlock state.
   * {@code class} properties are CSS classes used to set the icons' background
   * and password placeholder color.
   * @const {Array<{type: !number, class: !string}>}
   */
  var FINGERPRINT_STATES_MAPPING = [
    {state: FINGERPRINT_STATES.HIDDEN, class: 'hidden'},
    {state: FINGERPRINT_STATES.DEFAULT, class: 'default'},
    {state: FINGERPRINT_STATES.SIGNIN, class: 'signin'},
    {state: FINGERPRINT_STATES.FAILED, class: 'failed'}
  ];

  // Supported multi-profile user behavior values.
  // Keep in sync with the enum in login_user_info.mojom
  var MULTI_PROFILE_USER_BEHAVIOR = {
    UNRESTRICTED: 0,
    PRIMARY_ONLY: 1,
    NOT_ALLOWED: 2,
    OWNER_PRIMARY_ONLY: 3
  };

  // Focus and tab order are organized as follows:
  //
  // (1) all user pods have tab index 1 so they are traversed first;
  // (2) when a user pod is activated, its tab index is set to -1, then its
  // main input field and action box menu button get focus;
  // (3) if pin keyboard is present, it has tab index 2 so it follows the
  // action box menu button;
  // (4) if user pod custom icon is interactive, it has tab index 3;
  // (5) buttons on the header bar have tab index 4;
  // (6) User pod menu items (if present) have tab index 5;
  // (7) lastly, focus jumps to the Status Area and back to user pods.
  //
  // 'Focus' event is handled by a capture handler for the whole document
  // and in some cases 'mousedown' event handlers are used instead of 'click'
  // handlers where it's necessary to prevent 'focus' event from being fired.

  /**
   * Helper function to remove a class from given element.
   * @param {!HTMLElement} el Element whose class list to change.
   * @param {string} cl Class to remove.
   */
  function removeClass(el, cl) {
    el.classList.remove(cl);
  }

  /**
   * Helper function to switch directions for right-to-left languages.
   * @param {!HTMLElement} el Element whose style needs to change.
   */
  function switchDirection(el) {
    var leftStyle = el.style.left;
    el.style.left = el.style.right;
    el.style.right = leftStyle;
  }

  /**
   * Creates a user pod.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPod = cr.ui.define(function() {
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  /**
   * The display style of user pods.
   * @enum {number}
   * @const
   */
  UserPod.Style = {
    LARGE: 0,
    SMALL: 1,
    EXTRA_SMALL: 2
  };

  /**
   * Stops event propagation from the any user pod child element.
   * @param {Event} e Event to handle.
   */
  function stopEventPropagation(e) {
    // Prevent default so that we don't trigger a 'focus' event.
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Creates an element for custom icon shown in a user pod next to the input
   * field.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPodCustomIcon = cr.ui.define(function() {
    var node = document.createElement('div');
    node.classList.add('custom-icon-container');
    node.hidden = true;

    // Create the actual icon element and add it as a child to the container.
    var iconNode = document.createElement('div');
    iconNode.classList.add('custom-icon');
    node.appendChild(iconNode);
    return node;
  });

  /**
   * The supported user pod custom icons.
   * {@code id} properties should be in sync with values set by C++ side.
   * {@code class} properties are CSS classes used to set the icons' background.
   * @const {Array<{id: !string, class: !string}>}
   */
  UserPodCustomIcon.ICONS = [
    {id: 'locked', class: 'custom-icon-locked'},
    {id: 'locked-to-be-activated',
     class: 'custom-icon-locked-to-be-activated'},
    {id: 'locked-with-proximity-hint',
     class: 'custom-icon-locked-with-proximity-hint'},
    {id: 'unlocked', class: 'custom-icon-unlocked'},
    {id: 'hardlocked', class: 'custom-icon-hardlocked'},
    {id: 'spinner', class: 'custom-icon-spinner'}
  ];

  /**
   * The hover state for the icon. When user hovers over the icon, a tooltip
   * should be shown after a short delay. This enum is used to keep track of
   * the tooltip status related to hover state.
   * @enum {string}
   */
  UserPodCustomIcon.HoverState = {
    /** The user is not hovering over the icon. */
    NO_HOVER: 'no_hover',

    /** The user is hovering over the icon but the tooltip is not activated. */
    HOVER: 'hover',

    /**
     * User is hovering over the icon and the tooltip is activated due to the
     * hover state (which happens with delay after user starts hovering).
     */
    HOVER_TOOLTIP: 'hover_tooltip'
  };

  /**
   * If the icon has a tooltip that should be automatically shown, the tooltip
   * is shown even when there is no user action (i.e. user is not hovering over
   * the icon), after a short delay. The tooltip should be hidden after some
   * time. Note that the icon will not be considered autoshown if it was
   * previously shown as a result of the user action.
   * This enum is used to keep track of this state.
   * @enum {string}
   */
  UserPodCustomIcon.TooltipAutoshowState = {
    /** The tooltip should not be or was not automatically shown. */
    DISABLED: 'disabled',

    /**
     * The tooltip should be automatically shown, but the timeout for showing
     * the tooltip has not yet passed.
     */
    ENABLED: 'enabled',

    /** The tooltip was automatically shown. */
    ACTIVE : 'active'
  };

  UserPodCustomIcon.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The id of the icon being shown.
     * @type {string}
     * @private
     */
    iconId_: '',

    /**
     * A reference to the timeout for updating icon hover state. Non-null
     * only if there is an active timeout.
     * @type {?number}
     * @private
     */
    updateHoverStateTimeout_: null,

    /**
     * A reference to the timeout for updating icon tooltip autoshow state.
     * Non-null only if there is an active timeout.
     * @type {?number}
     * @private
     */
    updateTooltipAutoshowStateTimeout_: null,

    /**
     * Callback for click and 'Enter' key events that gets set if the icon is
     * interactive.
     * @type {?function()}
     * @private
     */
    actionHandler_: null,

    /**
     * The current tooltip state.
     * @type {{active: function(): boolean,
     *         autoshow: !UserPodCustomIcon.TooltipAutoshowState,
     *         hover: !UserPodCustomIcon.HoverState,
     *         text: string}}
     * @private
     */
    tooltipState_: {
      /**
       * Utility method for determining whether the tooltip is active, either as
       * a result of hover state or being autoshown.
       * @return {boolean}
       */
      active: function() {
        return this.autoshow == UserPodCustomIcon.TooltipAutoshowState.ACTIVE ||
               this.hover == UserPodCustomIcon.HoverState.HOVER_TOOLTIP;
      },

      /**
       * @type {!UserPodCustomIcon.TooltipAutoshowState}
       */
      autoshow: UserPodCustomIcon.TooltipAutoshowState.DISABLED,

      /**
       * @type {!UserPodCustomIcon.HoverState}
       */
      hover: UserPodCustomIcon.HoverState.NO_HOVER,

      /**
       * The tooltip text.
       * @type {string}
       */
      text: ''
    },

    /** @override */
    decorate: function() {
      this.iconElement.addEventListener(
          'mouseover',
          this.updateHoverState_.bind(this,
                                      UserPodCustomIcon.HoverState.HOVER));
      this.iconElement.addEventListener(
          'mouseout',
          this.updateHoverState_.bind(this,
                                      UserPodCustomIcon.HoverState.NO_HOVER));
      this.iconElement.addEventListener('mousedown',
                                        this.handleMouseDown_.bind(this));
      this.iconElement.addEventListener('click',
                                        this.handleClick_.bind(this));
      this.iconElement.addEventListener('keydown',
                                        this.handleKeyDown_.bind(this));

      // When the icon is focused using mouse, there should be no outline shown.
      // Preventing default mousedown event accomplishes this.
      this.iconElement.addEventListener('mousedown', function(e) {
        e.preventDefault();
      });
    },

    /**
     * Getter for the icon element's div.
     * @return {HTMLDivElement}
     */
    get iconElement() {
      return this.querySelector('.custom-icon');
    },

    /**
     * Updates the icon element class list to properly represent the provided
     * icon.
     * @param {!string} id The id of the icon that should be shown. Should be
     *    one of the ids listed in {@code UserPodCustomIcon.ICONS}.
     */
    setIcon: function(id) {
      this.iconId_ = id;
      UserPodCustomIcon.ICONS.forEach(function(icon) {
        this.iconElement.classList.toggle(icon.class, id == icon.id);
      }, this);
    },

    /**
     * Sets the ARIA label for the icon.
     * @param {!string} ariaLabel
     */
    setAriaLabel: function(ariaLabel) {
      this.iconElement.setAttribute('aria-label', ariaLabel);
    },

    /**
     * Shows the icon.
     */
    show: function() {
      // Show the icon if the current iconId is valid.
      var validIcon = false;
      UserPodCustomIcon.ICONS.forEach(function(icon) {
        validIcon = validIcon || this.iconId_ == icon.id;
      }, this);
      // Update password container width based on the visibility of the
      // custom icon container.
      var parentPod = this.getParentPod_();
      if (parentPod) {
        parentPod.passwordEntryContainerElement.classList.toggle(
            'custom-icon-shown', validIcon);
      }
      this.hidden = !validIcon;
    },

    /**
     * Updates the icon tooltip. If {@code autoshow} parameter is set the
     * tooltip is immediatelly shown. If tooltip text is not set, the method
     * ensures the tooltip gets hidden. If tooltip is shown prior to this call,
     * it remains shown, but the tooltip text is updated.
     * @param {!{text: string, autoshow: boolean}} tooltip The tooltip
     *    parameters.
     */
    setTooltip: function(tooltip) {
      this.iconElement.classList.toggle('icon-with-tooltip', !!tooltip.text);

      this.updateTooltipAutoshowState_(
          tooltip.autoshow ?
              UserPodCustomIcon.TooltipAutoshowState.ENABLED :
              UserPodCustomIcon.TooltipAutoshowState.DISABLED);
      this.tooltipState_.text = tooltip.text;
      this.updateTooltip_();
    },

    /**
     * Sets up icon tabIndex attribute and handler for click and 'Enter' key
     * down events.
     * @param {?function()} callback If icon should be interactive, the
     *     function to get called on click and 'Enter' key down events. Should
     *     be null to make the icon  non interactive.
     */
    setInteractive: function(callback) {
      this.iconElement.classList.toggle('interactive-custom-icon', !!callback);

      // Update tabIndex property if needed.
      if (!!this.actionHandler_ != !!callback) {
        if (callback) {
          this.iconElement.setAttribute('tabIndex',
                                         UserPodTabOrder.POD_CUSTOM_ICON);
        } else {
          this.iconElement.removeAttribute('tabIndex');
        }
      }

      // Set the new action handler.
      this.actionHandler_ = callback;
    },

    /**
     * Hides the icon and cleans its state.
     */
    hide: function() {
      this.hideTooltip_();
      this.clearUpdateHoverStateTimeout_();
      this.clearUpdateTooltipAutoshowStateTimeout_();
      this.setInteractive(null);
      this.hidden = true;
    },

    /**
     * Clears timeout for showing a tooltip if one is set. Used to cancel
     * showing the tooltip when the user starts typing the password.
     */
    cancelDelayedTooltipShow: function() {
      this.updateTooltipAutoshowState_(
          UserPodCustomIcon.TooltipAutoshowState.DISABLED);
      this.clearUpdateHoverStateTimeout_();
    },

    /**
     * Handles mouse down event in the icon element.
     * @param {Event} e The mouse down event.
     * @private
     */
    handleMouseDown_: function(e) {
      this.updateHoverState_(UserPodCustomIcon.HoverState.NO_HOVER);
      this.updateTooltipAutoshowState_(
          UserPodCustomIcon.TooltipAutoshowState.DISABLED);

      // Stop the event propagation so in the case the click ends up on the
      // user pod (outside the custom icon) auth is not attempted.
      stopEventPropagation(e);
    },

    /**
     * Handles click event on the icon element. No-op if
     * {@code this.actionHandler_} is not set.
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      if (!this.actionHandler_)
        return;
      this.actionHandler_();
      stopEventPropagation(e);
    },

    /**
     * Handles key down event on the icon element. Only 'Enter' key is handled.
     * No-op if {@code this.actionHandler_} is not set.
     * @param {Event} e The key down event.
     * @private
     */
    handleKeyDown_: function(e) {
      if (!this.actionHandler_ || e.key != 'Enter')
        return;
      this.actionHandler_(e);
      stopEventPropagation(e);
    },

    /**
     * Changes the tooltip hover state and updates tooltip visibility if needed.
     * @param {!UserPodCustomIcon.HoverState} state
     * @private
     */
    updateHoverState_: function(state) {
      this.clearUpdateHoverStateTimeout_();
      this.sanitizeTooltipStateIfBubbleHidden_();

      if (state == UserPodCustomIcon.HoverState.HOVER) {
        if (this.tooltipState_.active()) {
          this.tooltipState_.hover = UserPodCustomIcon.HoverState.HOVER_TOOLTIP;
        } else {
          this.updateHoverStateSoon_(
              UserPodCustomIcon.HoverState.HOVER_TOOLTIP);
        }
        return;
      }

      if (state != UserPodCustomIcon.HoverState.NO_HOVER &&
          state != UserPodCustomIcon.HoverState.HOVER_TOOLTIP) {
        console.error('Invalid hover state ' + state);
        return;
      }

      this.tooltipState_.hover = state;
      this.updateTooltip_();
    },

    /**
     * Sets up a timeout for updating icon hover state.
     * @param {!UserPodCustomIcon.HoverState} state
     * @private
     */
    updateHoverStateSoon_: function(state) {
      if (this.updateHoverStateTimeout_)
        clearTimeout(this.updateHoverStateTimeout_);
      this.updateHoverStateTimeout_ =
          setTimeout(this.updateHoverState_.bind(this, state), 1000);
    },

    /**
     * Clears a timeout for updating icon hover state if there is one set.
     * @private
     */
    clearUpdateHoverStateTimeout_: function() {
      if (this.updateHoverStateTimeout_) {
        clearTimeout(this.updateHoverStateTimeout_);
        this.updateHoverStateTimeout_ = null;
      }
    },

    /**
     * Changes the tooltip autoshow state and changes tooltip visibility if
     * needed.
     * @param {!UserPodCustomIcon.TooltipAutoshowState} state
     * @private
     */
    updateTooltipAutoshowState_: function(state) {
      this.clearUpdateTooltipAutoshowStateTimeout_();
      this.sanitizeTooltipStateIfBubbleHidden_();

      if (state == UserPodCustomIcon.TooltipAutoshowState.DISABLED) {
        if (this.tooltipState_.autoshow != state) {
          this.tooltipState_.autoshow = state;
          this.updateTooltip_();
        }
        return;
      }

      if (this.tooltipState_.active()) {
        if (this.tooltipState_.autoshow !=
                UserPodCustomIcon.TooltipAutoshowState.ACTIVE) {
          this.tooltipState_.autoshow =
              UserPodCustomIcon.TooltipAutoshowState.DISABLED;
        } else {
          // If the tooltip is already automatically shown, the timeout for
          // removing it should be reset.
          this.updateTooltipAutoshowStateSoon_(
              UserPodCustomIcon.TooltipAutoshowState.DISABLED);
        }
        return;
      }

      if (state == UserPodCustomIcon.TooltipAutoshowState.ENABLED) {
        this.updateTooltipAutoshowStateSoon_(
            UserPodCustomIcon.TooltipAutoshowState.ACTIVE);
      } else if (state == UserPodCustomIcon.TooltipAutoshowState.ACTIVE) {
        this.updateTooltipAutoshowStateSoon_(
            UserPodCustomIcon.TooltipAutoshowState.DISABLED);
      }

      this.tooltipState_.autoshow = state;
      this.updateTooltip_();
    },

    /**
     * Sets up a timeout for updating tooltip autoshow state.
     * @param {!UserPodCustomIcon.TooltipAutoshowState} state
     * @private
     */
    updateTooltipAutoshowStateSoon_: function(state) {
      if (this.updateTooltipAutoshowStateTimeout_)
        clearTimeout(this.updateTooltupAutoshowStateTimeout_);
      var timeout =
          state == UserPodCustomIcon.TooltipAutoshowState.DISABLED ?
              5000 : 1000;
      this.updateTooltipAutoshowStateTimeout_ =
          setTimeout(this.updateTooltipAutoshowState_.bind(this, state),
                     timeout);
    },

    /**
     * Clears the timeout for updating tooltip autoshow state if one is set.
     * @private
     */
    clearUpdateTooltipAutoshowStateTimeout_: function() {
      if (this.updateTooltipAutoshowStateTimeout_) {
        clearTimeout(this.updateTooltipAutoshowStateTimeout_);
        this.updateTooltipAutoshowStateTimeout_ = null;
      }
    },

    /**
     * If tooltip bubble is hidden, this makes sure that hover and tooltip
     * autoshow states are not the ones that imply an active tooltip.
     * Used to handle a case where the tooltip bubble is hidden by an event that
     * does not update one of the states (e.g. click outside the pod will not
     * update tooltip autoshow state). Should be called before making
     * tooltip state updates.
     * @private
     */
    sanitizeTooltipStateIfBubbleHidden_: function() {
      if (!$('bubble').hidden)
        return;

      if (this.tooltipState_.hover ==
              UserPodCustomIcon.HoverState.HOVER_TOOLTIP &&
          this.tooltipState_.text) {
        this.tooltipState_.hover = UserPodCustomIcon.HoverState.NO_HOVER;
        this.clearUpdateHoverStateTimeout_();
      }

      if (this.tooltipState_.autoshow ==
             UserPodCustomIcon.TooltipAutoshowState.ACTIVE) {
        this.tooltipState_.autoshow =
            UserPodCustomIcon.TooltipAutoshowState.DISABLED;
        this.clearUpdateTooltipAutoshowStateTimeout_();
      }
    },

    /**
     * Returns whether the user pod to which the custom icon belongs is focused.
     * @return {boolean}
     * @private
     */
    isParentPodFocused_: function() {
      if ($('account-picker').hidden)
        return false;
      var parentPod = this.getParentPod_();
      return parentPod && parentPod.parentNode.isFocused(parentPod);
    },

    /**
     * Depending on {@code this.tooltipState_}, it updates tooltip visibility
     * and text.
     * @private
     */
    updateTooltip_: function() {
      var parentPod = this.getParentPod_();
      if (this.hidden || !parentPod ||
          parentPod.getPodStyle() != UserPod.Style.LARGE ||
          !this.isParentPodFocused_()) {
        return;
      }

      if (!this.tooltipState_.active() || !this.tooltipState_.text) {
        this.hideTooltip_();
        return;
      }

      // Show the tooltip bubble.
      var bubbleContent = document.createElement('div');
      bubbleContent.textContent = this.tooltipState_.text;

      parentPod.showBubble(bubbleContent);
    },

    /**
     * Hides the tooltip.
     * @private
     */
    hideTooltip_: function() {
      $('bubble').hideForElement(this);
    },

    /**
     * Gets the parent pod (may be null) of this custom icon.
     * @return {?HTMLDivElement}
     */
    getParentPod_: function() {
      var parentPod = this.parentNode;
      while (parentPod && !parentPod.classList.contains('pod'))
        parentPod = parentPod.parentNode;
      return parentPod;
    }
  };

  /**
   * Unique salt added to user image URLs to prevent caching. Dictionary with
   * user names as keys.
   * @type {Object}
   */
  UserPod.userImageSalt_ = {};

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Whether click on the pod can issue a user click auth attempt. The
     * attempt can be issued iff the pod was focused when the click
     * started (i.e. on mouse down event).
     * @type {boolean}
     * @private
     */
    userClickAuthAllowed_: false,

    /**
     * Whether the user has recently authenticated with fingerprint.
     * @type {boolean}
     * @private
     */
    fingerprintAuthenticated_: false,

    /**
     * True iff the pod can display the pin keyboard. The pin keyboard may not
     * always be displayed even if this is true, ie, if the virtual keyboard is
     * also being displayed.
     */
    pinEnabled: false,

    /**
     * If set, a function which hides a persistent detachable base warning
     * bubble. This will be set if a detachable base warning bubble is shown for
     * this pod.
     * @type {?function()}
     * @private
     */
    detachableBaseWarningBubbleHider_: null,

    /** @override */
    decorate: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.actionBoxAreaElement.tabIndex = UserPodTabOrder.POD_INPUT;

      this.addEventListener('keydown', this.handlePodKeyDown_.bind(this));
      this.addEventListener('click', this.handleClickOnPod_.bind(this));
      this.addEventListener('mousedown', this.handlePodMouseDown_.bind(this));

      if (this.pinKeyboard) {
        this.pinKeyboard.passwordElement = this.passwordElement;
        this.pinKeyboard.addEventListener('pin-change',
            this.handleInputChanged_.bind(this));
        this.pinKeyboard.tabIndex = UserPodTabOrder.PIN_KEYBOARD;
      }

      this.actionBoxAreaElement.addEventListener('mousedown',
                                                 stopEventPropagation);
      this.actionBoxAreaElement.addEventListener('click',
          this.handleActionAreaButtonClick_.bind(this));
      this.actionBoxAreaElement.addEventListener('keydown',
          this.handleActionAreaButtonKeyDown_.bind(this));

      this.actionBoxMenuTitleElement.addEventListener('keydown',
          this.handleMenuTitleElementKeyDown_.bind(this));
      this.actionBoxMenuTitleElement.addEventListener('blur',
          this.handleMenuTitleElementBlur_.bind(this));

      this.actionBoxMenuRemoveElement.addEventListener('click',
          this.handleRemoveCommandClick_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('keydown',
          this.handleRemoveCommandKeyDown_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('blur',
          this.handleRemoveCommandBlur_.bind(this));
      this.actionBoxRemoveUserWarningButtonElement.addEventListener('click',
          this.handleRemoveUserConfirmationClick_.bind(this));
      this.actionBoxRemoveUserWarningButtonElement.addEventListener('keydown',
          this.handleRemoveUserConfirmationKeyDown_.bind(this));

      if (this.fingerprintIconElement) {
        this.fingerprintIconElement.addEventListener(
            'mouseover', this.handleFingerprintIconMouseOver_.bind(this));
        this.fingerprintIconElement.addEventListener(
            'mouseout', this.handleFingerprintIconMouseOut_.bind(this));
        this.fingerprintIconElement.addEventListener(
            'mousedown', stopEventPropagation);
      }

      var customIcon = this.customIconElement;
      customIcon.parentNode.replaceChild(new UserPodCustomIcon(), customIcon);
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      this.passwordElement.addEventListener('keydown',
          this.parentNode.handleKeyDown.bind(this.parentNode));
      this.passwordElement.addEventListener('keypress',
          this.handlePasswordKeyPress_.bind(this));
      this.passwordElement.addEventListener('input',
          this.handleInputChanged_.bind(this));

      if (this.submitButton) {
        this.submitButton.addEventListener('click',
            this.handleSubmitButtonClick_.bind(this));
      }

      this.imageElement.addEventListener('load',
          this.parentNode.handlePodImageLoad.bind(this.parentNode, this));

      this.smallPodImageElement.addEventListener(
          'load',
          this.parentNode.handlePodImageLoad.bind(this.parentNode, this));

      var initialAuthType = this.user.initialAuthType ||
          AUTH_TYPE.OFFLINE_PASSWORD;
      this.setAuthType(initialAuthType, null);

      if (this.user.isActiveDirectory)
        this.setAttribute('is-active-directory', '');

      this.userClickAuthAllowed_ = false;
    },

    /**
     * Whether the user pod is disabled.
     * @type {boolean}
     */
    disabled_: false,
    get disabled() {
      return this.disabled_;
    },
    set disabled(value) {
      this.disabled_ = value;
      this.querySelectorAll('button,input').forEach(function(element) {
        element.disabled = value
      });

      this.tabIndex = value ? -1 : UserPodTabOrder.POD_INPUT;
      this.actionBoxAreaElement.tabIndex =
          value ? -1 : UserPodTabOrder.POD_INPUT;

      // Special handling for submit button - the submit button should be
      // enabled only if there is the password value set.
      var submitButton = this.submitButton;
      if (submitButton)
        submitButton.disabled = value || !this.passwordElement.value;
    },

    /**
     * Resets tab order for pod elements to its initial state.
     */
    resetTabOrder: function() {
      // Note: the |mainInput| can be the pod itself.
      this.mainInput.tabIndex = -1;
      this.tabIndex = UserPodTabOrder.POD_INPUT;
    },

    /**
     * Handles keypress event (i.e. any textual input) on password input.
     * @param {Event} e Keypress Event object.
     * @private
     */
    handlePasswordKeyPress_: function(e) {
      // When tabbing from the system tray a tab key press is received. Suppress
      // this so as not to type a tab character into the password field.
      if (e.keyCode == 9) {
        e.preventDefault();
        return;
      }
      this.customIconElement.cancelDelayedTooltipShow();
    },

    /**
     * Handles a click event on submit button.
     * @param {Event} e Click event.
     */
    handleSubmitButtonClick_: function(e) {
      this.parentNode.setActivatedPod(this, e);
    },

    /**
     * Top edge margin number of pixels.
     * @type {?number}
     */
    set top(top) {
      this.style.top = cr.ui.toCssPx(top);
    },

    /**
     * Top edge margin number of pixels.
     */
    get top() {
      return parseInt(this.style.top);
    },

    /**
     * Left edge margin number of pixels.
     * @type {?number}
     */
    set left(left) {
      this.style.left = cr.ui.toCssPx(left);
    },

    /**
     * Left edge margin number of pixels.
     */
    get left() {
      return parseInt(this.style.left);
    },

    /**
     * Height number of pixels.
     */
    get height() {
      return this.offsetHeight;
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.querySelector('.user-image');
    },

    /**
     * Gets animated image element.
     * @type {!HTMLImageElement}
     */
    get animatedImageElement() {
      return this.querySelector('.user-image.animated-image');
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.querySelector('.name');
    },

    /**
     * Gets image element of the small pod.
     * @type {!HTMLImageElement}
     */
    get smallPodImageElement() {
      return this.querySelector('.user-image.small-pod-image');
    },

    /**
     * Gets animated image element.
     * @type {!HTMLImageElement}
     */
    get smallPodAnimatedImageElement() {
      return this.querySelector('.user-image.small-pod-image.animated-image');
    },

    /**
     * Gets name element of the small pod.
     * @type {!HTMLDivElement}
     */
    get smallPodNameElement() {
      return this.querySelector('.small-pod-name');
    },

    /**
     * Gets reauth name hint element.
     * @type {!HTMLDivElement}
     */
    get reauthNameHintElement() {
      return this.querySelector('.reauth-name-hint');
    },

    /**
     * Gets the container holding the password field.
     * @type {!HTMLInputElement}
     */
    get passwordEntryContainerElement() {
      return this.querySelector('.password-entry-container');
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.querySelector('.password');
    },

    /**
     * Gets submit button.
     * @type {!HTMLInputElement}
     */
    get submitButton() {
      return this.querySelector('.submit-button');
    },

    /**
     * Gets the password label, which is used to show a message where the
     * password field is normally.
     * @type {!HTMLInputElement}
     */
    get passwordLabelElement() {
      return this.querySelector('.password-label');
    },

    get pinContainer() {
      return this.querySelector('.pin-container');
    },

    /**
     * Gets the pin-keyboard of the pod.
     * @type {!HTMLElement}
     */
    get pinKeyboard() {
      return this.querySelector('pin-keyboard');
    },

    /**
     * Gets user online sign in hint element.
     * @type {!HTMLDivElement}
     */
    get reauthWarningElement() {
      return this.querySelector('.reauth-hint-container');
    },

    /**
     * Gets action box area.
     * @type {!HTMLInputElement}
     */
    get actionBoxAreaElement() {
      return this.querySelector('.action-box-area');
    },

    /**
     * Gets user type icon area.
     * @type {!HTMLDivElement}
     */
    get userTypeIconAreaElement() {
      return this.querySelector('.user-type-icon-area');
    },

    /**
     * Gets user type bubble like multi-profiles policy restriction message.
     * @type {!HTMLDivElement}
     */
    get userTypeBubbleElement() {
      return this.querySelector('.user-type-bubble');
    },

    /**
     * Gets action box menu.
     * @type {!HTMLDivElement}
     */
    get actionBoxMenu() {
      return this.querySelector('.action-box-menu');
    },

    /**
     * Gets action box menu title (user name and email).
     * @type {!HTMLDivElement}
     */
    get actionBoxMenuTitleElement() {
      return this.querySelector('.action-box-menu-title');
    },

    /**
     * Gets action box menu title, user name item.
     * @type {!HTMLSpanElement}
     */
    get actionBoxMenuTitleNameElement() {
      return this.querySelector('.action-box-menu-title-name');
    },

    /**
     * Gets action box menu title, user email item.
     * @type {!HTMLSpanElement}
     */
    get actionBoxMenuTitleEmailElement() {
      return this.querySelector('.action-box-menu-title-email');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuRemoveElement() {
      return this.querySelector('.action-box-menu-remove');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningElement() {
      return this.querySelector('.action-box-remove-user-warning');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningButtonElement() {
      return this.querySelector('.remove-warning-button');
    },

    /**
     * Gets the custom icon. This icon is normally hidden, but can be shown
     * using the chrome.screenlockPrivate API.
     * @type {!HTMLDivElement}
     */
    get customIconElement() {
      return this.querySelector('.custom-icon-container');
    },

    /**
     * Gets the elements used for statistics display.
     * @type {Object.<string, !HTMLDivElement>}
     */
    get statsMapElements() {
      return {
        'BrowsingHistory':
            this.querySelector('.action-box-remove-user-warning-history'),
            'Passwords':
                this.querySelector('.action-box-remove-user-warning-passwords'),
            'Bookmarks':
                this.querySelector('.action-box-remove-user-warning-bookmarks'),
            'Autofill':
                this.querySelector('.action-box-remove-user-warning-autofill')
      }
    },

    /**
     * Gets the fingerprint icon area.
     * @type {!HTMLDivElement}
     */
    get fingerprintIconElement() {
      return this.querySelector('.fingerprint-icon-container');
    },

    /**
     * Returns true if it's a public session pod.
     * @type {boolean}
     */
    get isPublicSessionPod() {
      return this.classList.contains('public-account');
    },

    /**
     * Sets the pod style.
     * @param {UserPod.Style} style Style set to the pod.
     */
    setPodStyle: function(style) {
      switch (style) {
        case UserPod.Style.LARGE:
          this.querySelector('.large-pod').hidden = false;
          this.querySelector('.small-pod').hidden = true;
          break;
        case UserPod.Style.SMALL:
          this.querySelector('.large-pod').hidden = true;
          this.querySelector('.small-pod').hidden = false;
          this.querySelector('.small-pod').classList.remove('extra-small');
          break;
        case UserPod.Style.EXTRA_SMALL:
          this.querySelector('.large-pod').hidden = true;
          this.querySelector('.small-pod').hidden = false;
          this.querySelector('.small-pod').classList.add('extra-small');
          break;
        default:
          console.error("Attempt to set an invalid pod style.");
          break;
      }
    },

    /**
     * Gets the pod style.
     * @return {UserPod.Style} Pod style.
     */
    getPodStyle: function() {
      if (this.querySelector('.small-pod').hidden)
        return UserPod.Style.LARGE;
      if (this.querySelector('.small-pod').classList.contains('extra-small'))
        return UserPod.Style.EXTRA_SMALL;
      return UserPod.Style.SMALL;
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      var animatedImageSrc = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];
      var imageSrc = animatedImageSrc + '&frame=0';
      this.imageElement.src = imageSrc;
      this.animatedImageElement.src = animatedImageSrc;
      this.smallPodImageElement.src = imageSrc;
      this.smallPodAnimatedImageElement.src = animatedImageSrc;

      this.nameElement.textContent = this.user_.displayName;
      this.smallPodNameElement.textContent = this.user_.displayName;
      this.reauthNameHintElement.textContent = this.user_.displayName;
      this.classList.toggle('signed-in', this.user_.signedIn);

      if (this.isAuthTypeUserClick)
        this.passwordLabelElement.textContent = this.authValue;

      this.updateActionBoxArea();
      this.customizeUserPodPerUserType();
      this.updateAriaLabels_();
    },

    updateActionBoxArea: function() {
      if (this.user_.publicAccount) {
        this.actionBoxAreaElement.hidden = true;
        return;
      }

      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;

      this.actionBoxMenuTitleNameElement.textContent = this.user_.isOwner ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;

      this.actionBoxMenuTitleEmailElement.hidden =
          this.user_.legacySupervisedUser;
    },

    customizeUserPodPerUserType: function() {
      if (this.user_.childUser && !this.user_.isDesktopUser) {
        this.setUserPodIconType('child');
      } else if (this.user_.legacySupervisedUser && !this.user_.isDesktopUser) {
        this.setUserPodIconType('legacySupervised');
        this.classList.add('legacy-supervised');
      } else if (this.multiProfilesPolicyApplied) {
        // Mark user pod as not focusable which in addition to the grayed out
        // filter makes it look in disabled state.
        this.classList.add('multiprofiles-policy-applied');
        this.setUserPodIconType('policy');

        if (this.user.multiProfilesPolicy ==
            MULTI_PROFILE_USER_BEHAVIOR.PRIMARY_ONLY) {
          this.querySelector('.mp-policy-primary-only-msg').hidden = false;
        } else if (this.user.multiProfilesPolicy ==
            MULTI_PROFILE_USER_BEHAVIOR.OWNER_PRIMARY_ONLY) {
          this.querySelector('.mp-owner-primary-only-msg').hidden = false;
        } else {
          this.querySelector('.mp-policy-not-allowed-msg').hidden = false;
        }
      }
    },

    /**
     * Updates ARIA labels and sets hidden states. All updates related to ARIA
     * should go here.
     * @private
     */
    updateAriaLabels_: function() {
      this.setAttribute('aria-label', this.user_.displayName);
      this.querySelector('.password-container')
          .setAttribute(
              'aria-label',
              loadTimeData.getStringF(
                  'passwordFieldAccessibleName', this.user_.emailAddress));
      this.actionBoxAreaElement.setAttribute(
          'aria-label',
          loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label',
          loadTimeData.getString('podMenuRemoveItemAccessibleName'));
    },

    isPinReady: function() {
      return this.pinKeyboard && this.pinKeyboard.offsetHeight > 0;
    },

    set showError(visible) {
      if (this.submitButton)
        this.submitButton.classList.toggle('error-shown', visible);
    },

    updatePinClass_: function(element, enable) {
      element.classList.toggle('pin-enabled', enable);
      element.classList.toggle('pin-disabled', !enable);
    },

    setPinVisibility: function(visible) {
      if (this.isPinShown() == visible)
        return;

      // Do not show pin if virtual keyboard is there.
      if (visible && Oobe.getInstance().virtualKeyboardShown)
        return;

      // Do not show pin keyboard if the pod does not have pin enabled.
      if (visible && !this.pinEnabled)
        return;

      // Do not show pin keyboard if the pod is not in large style.
      if (visible && this.getPodStyle() != UserPod.Style.LARGE)
        return;

      var elements = this.getElementsByClassName('pin-tag');
      for (var i = 0; i < elements.length; ++i)
        this.updatePinClass_(elements[i], visible);
      this.updatePinClass_(this, visible);

      // Set the focus to the input element after showing/hiding pin keyboard.
      this.mainInput.focus();

      // Change the password placeholder based on pin keyboard visibility.
      this.passwordElement.placeholder = loadTimeData.getString(visible ?
          'pinKeyboardPlaceholderPinPassword' : 'passwordHint');

      // Adjust the vertical position based on the pin keyboard visibility.
      var podHeight = visible ? CROS_POD_HEIGHT_WITH_PIN : CROS_POD_HEIGHT;
      this.top = ($('pod-row').screenSize.height - podHeight) / 2;
    },

    isPinShown: function() {
      return this.classList.contains('pin-enabled');
    },

    setUserPodIconType: function(userTypeClass) {
      this.userTypeIconAreaElement.classList.add(userTypeClass);
      // TODO(wzang): Evaluate all icon types other than supervised user and
      // switch them to badges per the design spec.
      this.userTypeIconAreaElement.hidden = true;
    },

    isFingerprintIconShown: function() {
      return this.fingerprintIconElement && !this.fingerprintIconElement.hidden;
    },

    /**
     * The user that this pod represents.
     * @type {!Object}
     */
    user_: undefined,
    get user() {
      return this.user_;
    },
    set user(userDict) {
      this.user_ = userDict;
      this.update();
    },

    /**
     * Returns true if multi-profiles sign in is currently active and this
     * user pod is restricted per policy.
     * @type {boolean}
     */
    get multiProfilesPolicyApplied() {
      var isMultiProfilesUI =
        (Oobe.getInstance().displayType == DISPLAY_TYPE.USER_ADDING);
      return isMultiProfilesUI && !this.user_.isMultiProfilesAllowed;
    },

    /**
     * Gets main input element.
     * @type {(HTMLButtonElement|HTMLInputElement)}
     */
    get mainInput() {
      if (this.isAuthTypePassword) {
        return this.passwordElement;
      } else if (this.isAuthTypeOnlineSignIn) {
        return this;
      } else if (this.isAuthTypeUserClick) {
        return this.passwordLabelElement;
      }
    },

    /**
     * Whether action box button is in active state.
     * @type {boolean}
     */
    get isActionBoxMenuActive() {
      return this.actionBoxAreaElement.classList.contains('active');
    },
    set isActionBoxMenuActive(active) {
      if (active == this.isActionBoxMenuActive)
        return;

      if (active) {
        this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;
        this.actionBoxRemoveUserWarningElement.hidden = true;

        // Clear focus first if another pod is focused.
        if (!this.parentNode.isFocused(this)) {
          this.parentNode.focusPod(undefined, true);
          this.actionBoxAreaElement.focus();
        }

        // Hide user-type-bubble.
        this.userTypeBubbleElement.classList.remove('bubble-shown');

        this.actionBoxAreaElement.classList.add('active');

        // Invisible focus causes ChromeVox to read user name and email.
        this.actionBoxMenuTitleElement.tabIndex = UserPodTabOrder.POD_MENU_ITEM;
        this.actionBoxMenuTitleElement.focus();
      } else {
        this.actionBoxAreaElement.classList.remove('active');
        this.actionBoxAreaElement.classList.remove('menu-moved-up');
        this.actionBoxMenu.classList.remove('menu-moved-up');
      }
    },

    /**
     * Whether action box button is in hovered state.
     * @type {boolean}
     */
    get isActionBoxMenuHovered() {
      return this.actionBoxAreaElement.classList.contains('hovered');
    },
    set isActionBoxMenuHovered(hovered) {
      if (hovered == this.isActionBoxMenuHovered)
        return;

      if (hovered) {
        this.actionBoxAreaElement.classList.add('hovered');
        this.classList.add('hovered');
      } else {
        if (this.multiProfilesPolicyApplied)
          this.userTypeBubbleElement.classList.remove('bubble-shown');
        this.actionBoxAreaElement.classList.remove('hovered');
        this.classList.remove('hovered');
      }
    },

    /**
     * Set the authentication type for the pod.
     * @param {number} An auth type value defined in the AUTH_TYPE enum.
     * @param {string} authValue The initial value used for the auth type.
     */
    setAuthType: function(authType, authValue) {
      this.authType_ = authType;
      this.authValue_ = authValue;
      this.setAttribute('auth-type', AUTH_TYPE_NAMES[this.authType_]);
      this.update();
      this.reset(this.parentNode.isFocused(this));
    },

    /**
     * The auth type of the user pod. This value is one of the enum
     * values in AUTH_TYPE.
     * @type {number}
     */
    get authType() {
      return this.authType_;
    },

    /**
     * The initial value used for the pod's authentication type.
     * eg. a prepopulated password input when using password authentication.
     */
    get authValue() {
      return this.authValue_;
    },

    /**
     * True if the the user pod uses a password to authenticate.
     * @type {bool}
     */
    get isAuthTypePassword() {
      return this.authType_ == AUTH_TYPE.OFFLINE_PASSWORD ||
             this.authType_ == AUTH_TYPE.FORCE_OFFLINE_PASSWORD;
    },

    /**
     * True if the the user pod uses a user click to authenticate.
     * @type {bool}
     */
    get isAuthTypeUserClick() {
      return this.authType_ == AUTH_TYPE.USER_CLICK;
    },

    /**
     * True if the the user pod uses a online sign in to authenticate.
     * @type {bool}
     */
    get isAuthTypeOnlineSignIn() {
      return this.authType_ == AUTH_TYPE.ONLINE_SIGN_IN;
    },

    /**
     * Updates the image element of the user.
     */
    updateUserImage: function() {
      UserPod.userImageSalt_[this.user.username] = new Date().getTime();
      this.update();
    },

    /**
     * Focuses on input element.
     * @param {boolean?} opt_ensureFocus If true, keep trying to focus until a
     * focus change event is raised.
     */
    focusInput: function(opt_ensureFocus) {
      // If |opt_ensureFocus| is set, keep setting the focus until we get a
      // global focus change event. Sometimes focus requests are ignored while
      // loading the page. See crbug.com/725622.
      if (opt_ensureFocus) {
        var INTERVAL_REPEAT_MS = 10;
        var input = this.mainInput;
        var intervalId = setInterval(function() {
          input.focus();
        }, INTERVAL_REPEAT_MS);
        window.addEventListener('focus', function refocus() {
          if (document.activeElement != input)
            return;
          window.removeEventListener('focus', refocus);
          window.clearInterval(intervalId);
        }, true);
      }

      // Move tabIndex from the whole pod to the main input.
      // Note: the |mainInput| can be the pod itself.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /**
     * Activates the pod.
     * @param {Event} e Event object.
     * @return {boolean} True if activated successfully.
     */
    activate: function(e) {
      if (this.isAuthTypeOnlineSignIn) {
        this.showSigninUI();
      } else if (this.isAuthTypeUserClick) {
        Oobe.disableSigninUI();
        this.classList.toggle('signing-in', true);
        chrome.send('attemptUnlock', [this.user.username]);
      } else if (this.isAuthTypePassword) {
        if (this.fingerprintAuthenticated_) {
          this.fingerprintAuthenticated_ = false;
          return true;
        }
        var pinValue = this.pinKeyboard ? this.pinKeyboard.value : '';
        var password = this.passwordElement.value || pinValue;
        if (!password)
          return false;
        Oobe.disableSigninUI();
        chrome.send('authenticateUser', [
          this.user.username, password, this.isPinShown() && !isNaN(password)
        ]);
      } else {
        console.error('Activating user pod with invalid authentication type: ' +
            this.authType);
      }

      return true;
    },

    showSupervisedUserSigninWarning: function() {
      // Legacy supervised user token has been invalidated.
      // Make sure that pod is focused i.e. "Sign in" button is seen.
      this.parentNode.focusPod(this);

      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent =
          loadTimeData.getString('supervisedUserExpiredTokenWarning');
      error.appendChild(messageDiv);

      $('bubble').showContentForElement(
          this.reauthWarningElement,
          cr.ui.Bubble.Attachment.TOP,
          error,
          this.reauthWarningElement.offsetWidth / 2,
          4);
      // Move warning bubble up if it overlaps the shelf.
      var maxHeight =
          cr.ui.LoginUITools.getMaxHeightBeforeShelfOverlapping($('bubble'));
      if (maxHeight < $('bubble').offsetHeight) {
        $('bubble').showContentForElement(
            this.reauthWarningElement,
            cr.ui.Bubble.Attachment.BOTTOM,
            error,
            this.reauthWarningElement.offsetWidth / 2,
            4);
      }
    },

    /**
     * Shows signin UI for this user.
     */
    showSigninUI: function() {
      if (this.user.legacySupervisedUser && !this.user.isDesktopUser) {
        this.showSupervisedUserSigninWarning();
      } else {
        // Special case for multi-profiles sign in. We show users even if they
        // are not allowed per policy. Restrict those users from starting GAIA.
        if (this.multiProfilesPolicyApplied)
          return;

        this.parentNode.showSigninUI(this.user.emailAddress);
      }
    },

    /**
     * Returns the element that should be used as the anchor for error bubbles
     * associated with the pod.
     *
     * @return {HTMLElement} The anchor for error bubbles.
     * @private
     */
    getBubbleAnchor_: function() {
      var bubbleAnchor = this.getElementsByClassName('auth-container')[0];
      if (!bubbleAnchor) {
        console.error('auth-container not found!');
        bubbleAnchor = this.mainInput;
      }
      return bubbleAnchor;
    },

    /**
     * Shows a bubble under the auth-container of the user pod.
     * @param {HTMLElement} content Content to show in bubble.
     * @param {!{bubble: (HTMLElement|undefined),
     *           anchor: (HTMLElement|undefined),
     *           timeout: (number|undefined)}|undefined} opt_options The custom
     *     options describing how the bubble should be shown:
     *     <ul>
     *       <li>bubble: The element that hosts the bubble content.</li>
     *       <li>
     *           anchor: The element to which the bubble should be anchored.
     *       </li>
     *       <li>
     *           timeout: Amount of time in ms after which the bubble
     *           should be hidden. Note: this should only be used for
     *           {@code $('bubble')} bubble element. The timeout will get
     *           cleared if the bubble is shown again.
     *       </li>
     *     </ul>
     * @return {function()} Function that, when called, hides the shown bubble.
     */
    showBubble: function(content, opt_options) {
      /** @const */ var BUBBLE_OFFSET = 25;
      // -8 = 4(BUBBLE_POD_OFFSET) - 2(bubble margin)
      //      - 10(internal bubble adjustment)
      var bubblePositioningPadding = -8;

      var options = opt_options || {};
      var bubble = options.bubble || $('bubble');

      // Make sure bubble timeout is changed only for $('bubble') element.
      if (options.timeout && bubble != $('bubble')) {
        console.error('Timeout can be set only when showing #bubble element.');
        return;
      }

      var bubbleAnchor = options.anchor || this.getBubbleAnchor_();
      var attachment;
      if (this.pinContainer && this.pinContainer.style.visibility == 'visible')
        attachment = cr.ui.Bubble.Attachment.RIGHT;
      else
        attachment = cr.ui.Bubble.Attachment.BOTTOM;

      // Cannot use cr.ui.LoginUITools.get* on bubble until it is attached to
      // the element. getMaxHeight/Width rely on the correct up/left element
      // side positioning that doesn't happen until bubble is attached.
      var maxHeight =
          cr.ui.LoginUITools.getMaxHeightBeforeShelfOverlapping(bubbleAnchor) -
          bubbleAnchor.offsetHeight - BUBBLE_POD_OFFSET;
      var maxWidth = cr.ui.LoginUITools.getMaxWidthToFit(bubbleAnchor) -
          bubbleAnchor.offsetWidth - BUBBLE_POD_OFFSET;

      // Change bubble visibility temporary to calculate height.
      var bubbleVisibility = bubble.style.visibility;
      bubble.style.visibility = 'hidden';
      bubble.hidden = false;
      // Now we need the bubble to have the new content before calculating
      // size. Undefined |content| == reuse old content.
      if (content !== undefined)
        bubble.replaceContent(content);

      // Get bubble size.
      var bubbleOffsetHeight = parseInt(bubble.offsetHeight);
      var bubbleOffsetWidth = parseInt(bubble.offsetWidth);
      // Restore attributes.
      bubble.style.visibility = bubbleVisibility;
      bubble.hidden = true;

      if (attachment == cr.ui.Bubble.Attachment.BOTTOM) {
        // Move the bubble if it overlaps the shelf.
        if (maxHeight < bubbleOffsetHeight)
          attachment = cr.ui.Bubble.Attachment.TOP;
      } else {
        // Move the bubble if it doesn't fit screen.
        if (maxWidth < bubbleOffsetWidth) {
          bubblePositioningPadding = 2;
          attachment = cr.ui.Bubble.Attachment.LEFT;
        }
      }

      if (bubble == $('bubble'))
        this.clearBubbleHideTimeout_();

      var state = {shown: false, hidden: false};

      var showBubbleCallback = function() {
        this.removeEventListener('transitionend', showBubbleCallback);
        // If the bubble was requested to be hidden while the transition was in
        // progress, do not show the bubble.
        if (state.hidden)
          return;

        state.shown = true;

        bubble.showContentForElement(
            bubbleAnchor, attachment, content, BUBBLE_OFFSET,
            bubblePositioningPadding, true);

        if (options.timeout != undefined) {
          this.hideBubbleTimeout_ = setTimeout(() => {
            this.hideBubbleTimeout_ = undefined;
            bubble.hideForElement(bubbleAnchor);
          }, options.timeout);
        }
      }.bind(this);
      this.addEventListener('transitionend', showBubbleCallback);
      ensureTransitionEndEvent(this);

      return function() {
        if (state.hidden)
          return;

        state.hidden = true;
        if (state.shown)
          bubble.hideForElement(bubbleAnchor);
      };
    },

    /**
     * Clears the timeout to hide a bubble, if a bubble timeout was set.
     * @private
     */
    clearBubbleHideTimeout_: function() {
      if (this.hideBubbleTimeout_) {
        clearTimeout(this.hideBubbleTimeout_);
        this.hideBubbleTimeout_ = null;
      }
    },

    /**
     * Shows persistent bubble for detachable base change warning.
     * @param {HTMLElement} content The bubble contens.
     */
    showDetachableBaseWarningBubble: function(content) {
      var anchor = this.getBubbleAnchor_();
      if (!anchor)
        return;
      this.clearBubbleHideTimeout_();
      $('bubble').hideForElement(anchor);
      this.detachableBaseWarningBubbleHider_ = this.showBubble(
          content, {bubble: $('bubble-persistent'), anchor: anchor});
    },

    /**
     * If a peristent bubble for detachable base change warning is shown (and
     * anchored at this pod), hides the bubble.
     */
    hideDetachableBaseWarningBubble: function() {
      if (this.detachableBaseWarningBubbleHider_) {
        this.detachableBaseWarningBubbleHider_();
        this.detachableBaseWarningBubbleHider_ = null;
      }
    },

    /**
     * Whether a detachable base warning bubble is being shown for this pod.
     * @return {boolean}
     */
    showingDetachableBaseWarningBubble: function() {
      return this.detachableBaseWarningBubbleHider_ &&
          !$('bubble-persistent').hidden &&
          $('bubble-persistent').anchor == this.getBubbleAnchor_();
    },

    /**
     * Resets the input field and updates the tab order of pod controls.
     * @param {boolean} takeFocus If true, input field takes focus.
     */
    reset: function(takeFocus) {
      this.passwordElement.value = '';
      if (this.pinKeyboard)
        this.pinKeyboard.value = '';
      this.updateInput_();
      this.classList.toggle('signing-in', false);
      if (takeFocus) {
        if (!this.multiProfilesPolicyApplied) {
          this.focusInput(
              this.mainInput.tagName == 'INPUT' /*opt_ensureFocus*/);
        }
      }
      else
        this.resetTabOrder();
    },

    /**
     * Removes a user using the correct identifier based on user type.
     * @param {Object} user User to be removed.
     */
    removeUser: function(user) {
      chrome.send('removeUser',
                  [user.isDesktopUser ? user.profilePath : user.username]);
    },

    /**
     * Handles a click event on action area button.
     * @param {Event} e Click event.
     */
    handleActionAreaButtonClick_: function(e) {
      if (this.parentNode.disabled)
        return;
      this.isActionBoxMenuActive = !this.isActionBoxMenuActive;
      // Action area menu and error bubbles shouldn't appear together.
      Oobe.clearErrors();
      e.stopPropagation();
    },

    /**
     * Handles a keydown event on action area button.
     * @param {Event} e KeyDown event.
     */
    handleActionAreaButtonKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.key) {
        case 'Enter':
        case ' ':
          if (this.parentNode.focusedPod_ && !this.isActionBoxMenuActive) {
            this.isActionBoxMenuActive = true;
            Oobe.clearErrors();
          }
          e.stopPropagation();
          break;
        case 'ArrowUp':
        case 'ArrowDown':
          if (this.isActionBoxMenuActive) {
            this.actionBoxMenuRemoveElement.tabIndex =
                UserPodTabOrder.POD_MENU_ITEM;
            this.actionBoxMenuRemoveElement.focus();
          }
          e.stopPropagation();
          break;
        // Ignore these two, so ChromeVox hotkeys don't close the menu before
        // they can navigate through it.
        case 'Shift':
        case 'Meta':
          break;
        case 'Escape':
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        case 'Tab':
          if (!this.parentNode.alwaysFocusSinglePod)
            this.parentNode.focusPod();
        default:
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a keydown event on menu title.
     * @param {Event} e KeyDown event.
     */
    handleMenuTitleElementKeyDown_: function(e) {
      if (this.disabled)
        return;

      if (e.key != 'Tab') {
        this.handleActionAreaButtonKeyDown_(e);
        return;
      }

      if (e.shiftKey == false) {
        if (this.actionBoxMenuRemoveElement.hidden) {
          this.isActionBoxMenuActive = false;
        } else {
          this.actionBoxMenuRemoveElement.tabIndex =
              UserPodTabOrder.POD_MENU_ITEM;
          this.actionBoxMenuRemoveElement.focus();
          e.preventDefault();
        }
      } else {
        this.isActionBoxMenuActive = false;
        this.focusInput();
        e.preventDefault();
      }
    },

    /**
     * Handles a blur event on menu title.
     * @param {Event} e Blur event.
     */
    handleMenuTitleElementBlur_: function(e) {
      if (this.disabled)
        return;
      this.actionBoxMenuTitleElement.tabIndex = -1;
    },

    /**
     * Handles a click event on remove user command.
     * @param {Event} e Click event.
     */
    handleRemoveCommandClick_: function(e) {
      this.showRemoveWarning_();
    },

    /**
     * Move the action box menu up if needed.
     */
    moveActionMenuUpIfNeeded_: function() {
      // Skip checking (computationally expensive) if already moved up.
      if (this.actionBoxMenu.classList.contains('menu-moved-up'))
        return;

      // Move up the menu if it overlaps shelf.
      var maxHeight = cr.ui.LoginUITools.getMaxHeightBeforeShelfOverlapping(
          this.actionBoxMenu, true);
      var actualHeight = parseInt(
          window.getComputedStyle(this.actionBoxMenu).height);
      if (maxHeight < actualHeight) {
        this.actionBoxMenu.classList.add('menu-moved-up');
        this.actionBoxAreaElement.classList.add('menu-moved-up');
      }
    },

    /**
     * Shows remove user warning. Used for legacy supervised users
     * and non-device-owner on CrOS, and for all users on desktop.
     */
    showRemoveWarning_: function() {
      this.actionBoxMenuRemoveElement.hidden = true;
      this.actionBoxRemoveUserWarningElement.hidden = false;

      if (!this.user.isDesktopUser) {
        this.moveActionMenuUpIfNeeded_();
        if (!this.user.legacySupervisedUser) {
          this.querySelector(
              '.action-box-remove-user-warning-text').style.display = 'none';
          this.querySelector(
              '.action-box-remove-user-warning-table-nonsync').style.display
              = 'none';
          var message = loadTimeData.getString('removeNonOwnerUserWarningText');
          this.updateRemoveNonOwnerUserWarningMessage_(this.user.profilePath,
                                                       message);
        }
      } else {
        // Show extra statistics information for desktop users
        this.querySelector(
          '.action-box-remove-non-owner-user-warning-text').hidden = true;
        this.RemoveWarningDialogSetMessage_();
        // set a global handler for the callback
        window.updateRemoveWarningDialog =
            this.updateRemoveWarningDialog_.bind(this);
        var is_synced_user = this.user.emailAddress !== "";
        if (!is_synced_user) {
          chrome.send('removeUserWarningLoadStats', [this.user.profilePath]);
        }
      }
      chrome.send('logRemoveUserWarningShown');
    },

    /**
     * Refresh the statistics in the remove user warning dialog.
     * @param {string} profilePath The filepath of the URL (must be verified).
     * @param {Object} profileStats Statistics associated with profileURL.
     */
    updateRemoveWarningDialog_: function(profilePath, profileStats) {
      if (profilePath !== this.user.profilePath)
        return;

      var stats_elements = this.statsMapElements;
      // Update individual statistics
      for (var key in profileStats) {
        if (stats_elements.hasOwnProperty(key)) {
          stats_elements[key].textContent = profileStats[key].count;
        }
      }
    },

    /**
     * Set the new message in the dialog.
     */
    RemoveWarningDialogSetMessage_: function() {
      var is_synced_user = this.user.emailAddress !== "";
      message = loadTimeData.getString(
          is_synced_user ? 'removeUserWarningTextSync' :
                           'removeUserWarningTextNonSync');
      this.updateRemoveWarningDialogSetMessage_(this.user.profilePath,
                                                message);
    },

    /**
     * Refresh the message in the remove user warning dialog.
     * @param {string} profilePath The filepath of the URL (must be verified).
     * @param {string} message The message to be written.
     * @param {number|string=} count The number or string to replace $1 in
     * |message|. Can be omitted if $1 is not present in |message|.
     */
    updateRemoveWarningDialogSetMessage_: function(
        profilePath, message, count) {
      if (profilePath !== this.user.profilePath)
        return;
      // Add localized messages where $1 will be replaced with
      // <span class="total-count"></span> and $2 will be replaced with
      // <span class="email"></span>.
      var element = this.querySelector('.action-box-remove-user-warning-text');
      element.textContent = '';

      messageParts = message.split(/(\$[12])/);
      var numParts = messageParts.length;
      for (var j = 0; j < numParts; j++) {
        if (messageParts[j] === '$1') {
          var elementToAdd = document.createElement('span');
          elementToAdd.classList.add('total-count');
          elementToAdd.textContent = count;
          element.appendChild(elementToAdd);
        } else if (messageParts[j] === '$2') {
          var elementToAdd = document.createElement('span');
          elementToAdd.classList.add('email');
          elementToAdd.textContent = this.user.emailAddress;
          element.appendChild(elementToAdd);
        } else {
          element.appendChild(document.createTextNode(messageParts[j]));
        }
      }
      this.moveActionMenuUpIfNeeded_();
    },

    /**
     * Update the message in the "remove non-owner user warning" dialog on CrOS.
     * @param {string} profilePath The filepath of the URL (must be verified).
     * @param (string) message The message to be written.
     */
    updateRemoveNonOwnerUserWarningMessage_: function(profilePath, message) {
      if (profilePath !== this.user.profilePath)
        return;
      // Add localized messages where $1 will be replaced with
      // <span class="email"></span>.
      var element = this.querySelector(
          '.action-box-remove-non-owner-user-warning-text');
      element.textContent = '';

      messageParts = message.split(/(\$[1])/);
      var numParts = messageParts.length;
      for (var j = 0; j < numParts; j++) {
        if (messageParts[j] == '$1') {
          var elementToAdd = document.createElement('span');
          elementToAdd.classList.add('email');
          elementToAdd.textContent = this.user.emailAddress;
          element.appendChild(elementToAdd);
        } else {
          element.appendChild(document.createTextNode(messageParts[j]));
        }
      }
      this.moveActionMenuUpIfNeeded_();
    },

    /**
     * Handles a click event on remove user confirmation button.
     * @param {Event} e Click event.
     */
    handleRemoveUserConfirmationClick_: function(e) {
      if (this.isActionBoxMenuActive) {
        this.isActionBoxMenuActive = false;
        this.removeUser(this.user);
        e.stopPropagation();
      }
    },

    /**
     * Handles mouseover event on fingerprint icon.
     * @param {Event} e MouseOver event.
     */
    handleFingerprintIconMouseOver_: function(e) {
      var bubbleContent = document.createElement('div');
      bubbleContent.textContent =
          loadTimeData.getString('fingerprintIconMessage');
      this.passwordElement.placeholder =
          loadTimeData.getString('fingerprintHint');

      /** @const */ var BUBBLE_OFFSET = 25;
      /** @const */ var BUBBLE_PADDING = -8;
      var attachment = this.isPinShown() ? cr.ui.Bubble.Attachment.RIGHT :
                                           cr.ui.Bubble.Attachment.BOTTOM;
      var bubbleAnchor = this.getBubbleAnchorForFingerprintIcon_();
      $('bubble').showContentForElement(
          bubbleAnchor, attachment, bubbleContent, BUBBLE_OFFSET,
          BUBBLE_PADDING, true);
    },

    /**
     * Handles mouseout event on fingerprint icon.
     * @param {Event} e MouseOut event.
     */
    handleFingerprintIconMouseOut_: function(e) {
      var bubbleAnchor = this.getBubbleAnchorForFingerprintIcon_();
      $('bubble').hideForElement(bubbleAnchor);
      this.passwordElement.placeholder = loadTimeData.getString(
          this.isPinShown() ? 'pinKeyboardPlaceholderPinPassword' :
                              'passwordHint');
    },

    /**
     * Returns bubble anchor of the fingerprint icon.
     * @return {!HTMLElement} Anchor element of the bubble.
     */
    getBubbleAnchorForFingerprintIcon_: function() {
      var bubbleAnchor = this;
      if (this.isPinShown())
        bubbleAnchor = (this.getElementsByClassName('auth-container'))[0];
      return bubbleAnchor;
    },

    /**
     * Handles a keydown event on remove user confirmation button.
     * @param {Event} e KeyDown event.
     */
    handleRemoveUserConfirmationKeyDown_: function(e) {
      if (!this.isActionBoxMenuActive)
        return;

      // Only handle pressing 'Enter' or 'Space', and let all other events
      // bubble to the action box menu.
      if (e.key == 'Enter' || e.key == ' ') {
        this.isActionBoxMenuActive = false;
        this.removeUser(this.user);
        e.stopPropagation();
        // Prevent default so that we don't trigger a 'click' event.
        e.preventDefault();
      }
    },

    /**
     * Handles a keydown event on remove command.
     * @param {Event} e KeyDown event.
     */
    handleRemoveCommandKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.key) {
        case 'Enter':
          e.preventDefault();
          this.showRemoveWarning_();
          e.stopPropagation();
          break;
        case 'ArrowUp':
        case 'ArrowDown':
          e.stopPropagation();
          break;
        // Ignore these two, so ChromeVox hotkeys don't close the menu before
        // they can navigate through it.
        case 'Shift':
        case 'Meta':
          break;
        case 'Escape':
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        default:
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a blur event on remove command.
     * @param {Event} e Blur event.
     */
    handleRemoveCommandBlur_: function(e) {
      if (this.disabled)
        return;
      this.actionBoxMenuRemoveElement.tabIndex = -1;
    },

    /**
     * Handles mouse down event. It sets whether the user click auth will be
     * allowed on the next mouse click event. The auth is allowed iff the pod
     * was focused on the mouse down event starting the click.
     * @param {Event} e The mouse down event.
     */
    handlePodMouseDown_: function(e) {
      // Only large pods have mouse down event.
      if (this.getPodStyle() == UserPod.Style.LARGE)
        this.userClickAuthAllowed_ = this.parentNode.isFocused(this);
    },

    /**
     * Called when the input of the password element changes. Updates the submit
     * button color and state and hides the error popup bubble.
     */
    updateInput_: function() {
      var isEmpty = this.passwordElement.value.length == 0;
      if (this.submitButton) {
        this.submitButton.disabled = isEmpty;
        if (this.isFingerprintIconShown()) {
          this.submitButton.hidden = isEmpty;
        } else {
          this.submitButton.hidden = false;
        }
      }
      this.showError = false;
      $('bubble').hide();
      this.classList.toggle('input-present', !isEmpty);
    },

    /**
     * Handles input event on the password element.
     * @param {Event} e Input event.
     */
    handleInputChanged_: function(e) {
      this.updateInput_();
    },

    /**
     * Handles click event on a user pod.
     * @param {Event} e Click event.
     */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      // Click events on public session pods should only be handled by their
      // overriding handler.
      if (this.isPublicSessionPod)
        return;

      if (this.getPodStyle() != UserPod.Style.LARGE) {
        $('pod-row').switchMainPod(this);
        return;
      }
      Oobe.clearErrors();

      if (!this.isActionBoxMenuActive) {
        if (this.isAuthTypeOnlineSignIn) {
          this.showSigninUI();
        } else if (this.isAuthTypeUserClick && this.userClickAuthAllowed_) {
          // Note that this.userClickAuthAllowed_ is set in mouse down event
          // handler.
          this.parentNode.setActivatedPod(this);
        } else if (this.pinKeyboard &&
                   e.target == this.pinKeyboard.submitButton) {
          // Sets the pod as activated if the submit button is clicked so that
          // it simulates what the enter button does for the password/pin.
          this.parentNode.setActivatedPod(this);
        }

        if (this.multiProfilesPolicyApplied)
          this.userTypeBubbleElement.classList.add('bubble-shown');

        // Prevent default so that we don't trigger 'focus' event and
        // stop propagation so that the 'click' event does not bubble
        // up and accidentally closes the bubble tooltip.
        stopEventPropagation(e);
      }
    },

    /**
     * Handles keydown event for a user pod.
     * @param {Event} e Key event.
     */
    handlePodKeyDown_: function(e) {
      if (this.getPodStyle() != UserPod.Style.LARGE) {
        this.handleNonLargePodKeyDown_(e);
        return;
      }
      if (!this.isAuthTypeUserClick || this.disabled)
        return;
      switch (e.key) {
        case 'Enter':
        case ' ':
          if (this.parentNode.isFocused(this))
            this.parentNode.setActivatedPod(this);
          break;
      }
    },

    /**
     * Handles keydown event for a small or extra small user pod.
     * @param {Event} e Key event.
     */
    handleNonLargePodKeyDown_: function(e) {
      switch (e.key) {
        case 'Enter':
        case ' ':
          if ($('pod-row').isFocused(this))
            $('pod-row').switchMainPod(this);
          break;
      }
    }
  };

  /**
   * Creates a public account user pod.
   * @constructor
   * @extends {UserPod}
   */
  var PublicAccountUserPod = cr.ui.define(function() {
    var node = UserPod();

    var extras = $('public-account-user-pod-extras-template').children;
    for (var i = 0; i < extras.length; ++i) {
      var el = extras[i].cloneNode(true);
      node.appendChild(el);
    }

    return node;
  });

  PublicAccountUserPod.prototype = {
    __proto__: UserPod.prototype,

    /**
     * Keeps track of the pod's original position before it's expanded.
     * @type {Object}
     */
    lastPosition: {left: 'unset', top: 'unset'},

    /**
     * If true, the public session should be launched directly without showing
     * the expanded view when the pod is activated.
     * @type {boolean}
     */
    skipExpandedView: false,

    /**
     * If true, further attempts of entering public session should bail out.
     * @type {boolean}
     */
    isEnteringPublicSession_: false,

    /**
     * The Learn more dialog.
     * @type {HTMLDivElement}
     */
    learnMoreDialog_: undefined,

    /**
     * "Enter" button in expanded side pane.
     * @type {!HTMLButtonElement}
     */
    get enterButtonElement() {
      return this.querySelector('.enter-button');
    },

    /**
     * Boolean flag of whether the pod is showing the side pane. The flag
     * controls whether 'expanded' class is added to the pod's class list and
     * resets tab order because main input element changes when the 'expanded'
     * state changes.
     * @type {boolean}
     */
    get expanded() {
      return this.classList.contains('expanded');
    },

    set expanded(expanded) {
      if (this.getPodStyle() != UserPod.Style.LARGE) {
        console.error(
            'Attempt to expand a public session pod when it is not large.');
        return;
      }
      if (this.expanded == expanded)
        return;

      this.resetTabOrder();
      this.classList.toggle('expanded', expanded);
      if (expanded) {
        // Show the advanced expanded pod directly if there are at least two
        // recommended locales. This will be the case in multilingual
        // environments where users are likely to want to choose among locales.
        if (this.querySelector('.language-select').multipleRecommendedLocales)
          this.classList.add('advanced');
      } else {
        this.classList.remove('advanced');
      }
      this.parentNode.handlePublicPodExpansion(this, expanded);
    },

    get advanced() {
      return this.classList.contains('advanced');
    },

    /** @override */
    get mainInput() {
      if (this.expanded)
        return this.querySelector('.monitoring-learn-more');
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);

      this.classList.add('public-account');

      this.nameElement.addEventListener('keydown', (function(e) {
        if (e.key == 'Enter') {
          this.parentNode.setActivatedPod(this, e);
          // Stop this keydown event from bubbling up to PodRow handler.
          e.stopPropagation();
          // Prevent default so that we don't trigger a 'click' event on the
          // newly focused "Enter" button.
          e.preventDefault();
        }
      }).bind(this));

      var languageSelect = this.querySelector('.language-select');
      languageSelect.tabIndex = UserPodTabOrder.POD_INPUT;
      languageSelect.manuallyChanged = false;
      languageSelect.addEventListener(
          'change',
          function() {
            languageSelect.manuallyChanged = true;
            this.getPublicSessionKeyboardLayouts_();
          }.bind(this));

      var keyboardSelect = this.querySelector('.keyboard-select');
      keyboardSelect.tabIndex = UserPodTabOrder.POD_INPUT;
      keyboardSelect.loadedLocale = null;

      var languageAndInputLink = this.querySelector('.language-and-input');
      languageAndInputLink.tabIndex = UserPodTabOrder.POD_INPUT;
      languageAndInputLink.addEventListener(
          'click', this.transitionToAdvanced_.bind(this));

      var languageAndInputIcon =
          this.querySelector('.language-and-input-dropdown');
      languageAndInputIcon.addEventListener(
          'click', this.transitionToAdvanced_.bind(this));

      var monitoringLearnMore = this.querySelector('.monitoring-learn-more');
      monitoringLearnMore.tabIndex = UserPodTabOrder.POD_INPUT;
      monitoringLearnMore.addEventListener(
          'click', this.onLearnMoreClicked_.bind(this));

      this.enterButtonElement.tabIndex = UserPodTabOrder.POD_INPUT;
      this.enterButtonElement.addEventListener('click', (function(e) {
        this.enterButtonElement.disabled = true;
        this.enterPublicSession_();
      }).bind(this));
    },

    /** @override **/
    initialize: function() {
      UserPod.prototype.initialize.call(this);

      id = this.user.username + '-keyboard';
      this.querySelector('.keyboard-select-label').htmlFor = id;
      this.querySelector('.keyboard-select').setAttribute('id', id);

      var id = this.user.username + '-language';
      this.querySelector('.language-select-label').htmlFor = id;
      var languageSelect = this.querySelector('.language-select');
      languageSelect.setAttribute('id', id);
      this.populateLanguageSelect(this.user.initialLocales,
                                  this.user.initialLocale,
                                  this.user.initialMultipleRecommendedLocales);
    },

    /** @override **/
    update: function() {
      UserPod.prototype.update.call(this);
      this.querySelector('.info').textContent =
          loadTimeData.getStringF('publicAccountInfoFormat',
                                  this.user_.enterpriseDisplayDomain);
      if (this.querySelector('.full-name'))
        this.querySelector('.full-name').textContent = this.user_.displayName;
    },

    /** @override */
    focusInput: function() {
      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      if (!takeFocus)
        this.expanded = false;
      this.enterButtonElement.disabled = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function(e) {
      if (!this.expanded) {
        if (this.skipExpandedView) {
          this.enterPublicSession_();
        } else {
          this.expanded = true;
          this.focusInput();
        }
      }
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      if (this.getPodStyle() != UserPod.Style.LARGE) {
        $('pod-row').switchMainPod(this);
        return;
      }
      Oobe.clearErrors();

      this.parentNode.focusPod(this);
      this.parentNode.setActivatedPod(this, e);
      // Prevent default so that we don't trigger 'focus' event.
      e.preventDefault();
    },

    /**
     * Updates the display name shown on the pod.
     * @param {string} displayName The new display name
     */
    setDisplayName: function(displayName) {
      this.user_.displayName = displayName;
      this.update();
    },

    /**
     * Transition the expanded pod from the basic to the advanced view.
     */
    transitionToAdvanced_: function() {
      this.classList.add('advanced');
    },

    /**
     * Show a dialog when user clicks on Learn more button.
     */
    onLearnMoreClicked_: function() {
      // Ignore if the Learn more dialog is already open.
      if (this.learnMoreDialog_)
        return;

      var topContainer = document.querySelector('#scroll-container');
      var dialogContainer =
          topContainer.querySelector('.monitoring-dialog-container');
      if (!dialogContainer) {
        // Add a dummy parent element to enable different CSS settings.
        dialogContainer = document.createElement('div');
        dialogContainer.classList.add('monitoring-dialog-container');
        topContainer.appendChild(dialogContainer);
      }
      dialogContainer.classList.toggle(
          'advanced', this.classList.contains('advanced'));
      var html = '';
      var infoItems = ['publicAccountMonitoringInfoItem1',
                       'publicAccountMonitoringInfoItem2',
                       'publicAccountMonitoringInfoItem3',
                       'publicAccountMonitoringInfoItem4'];
      for (item of infoItems) {
        html += '<p class="cr-dialog-item">';
        html += loadTimeData.getString(item);
        html += '</p>';
      }
      var title = loadTimeData.getString('publicAccountMonitoringInfo');
      this.learnMoreDialog_ = new cr.ui.dialogs.BaseDialog(dialogContainer);
      this.learnMoreDialog_.showHtml(
          title, html, undefined, this.onLearnMoreDialogClosed_.bind(this));
      this.parentNode.disabled = true;
    },

    /**
     * Clean up after the Learn more dialog is closed.
     */
    onLearnMoreDialogClosed_: function() {
      this.parentNode.disabled = false;
      this.learnMoreDialog_ = undefined;
    },

    /**
     * Retrieves the list of keyboard layouts available for the currently
     * selected locale.
     */
    getPublicSessionKeyboardLayouts_: function() {
      var selectedLocale = this.querySelector('.language-select').value;
      if (selectedLocale ==
          this.querySelector('.keyboard-select').loadedLocale) {
        // If the list of keyboard layouts was loaded for the currently selected
        // locale, it is already up to date.
        return;
      }
      chrome.send('getPublicSessionKeyboardLayouts',
                  [this.user.username, selectedLocale]);
    },

    /**
     * Populates the keyboard layout "select" element with a list of layouts.
     * @param {string} locale The locale to which this list of keyboard layouts
     *     applies
     * @param {!Object} list List of available keyboard layouts
     */
    populateKeyboardSelect: function(locale, list) {
      if (locale != this.querySelector('.language-select').value) {
        // The selected locale has changed and the list of keyboard layouts is
        // not applicable. This method will be called again when a list of
        // keyboard layouts applicable to the selected locale is retrieved.
        return;
      }

      var keyboardSelect = this.querySelector('.keyboard-select');
      keyboardSelect.loadedLocale = locale;
      keyboardSelect.innerHTML = '';
      for (var i = 0; i < list.length; ++i) {
        var item = list[i];
        keyboardSelect.appendChild(
            new Option(item.title, item.value, item.selected, item.selected));
      }
    },

    /**
     * Populates the language "select" element with a list of locales.
     * @param {!Object} locales The list of available locales
     * @param {string} defaultLocale The locale to select by default
     * @param {boolean} multipleRecommendedLocales Whether |locales| contains
     *     two or more recommended locales
     */
    populateLanguageSelect: function(
        locales, defaultLocale, multipleRecommendedLocales) {
      var languageSelect = this.querySelector('.language-select');
      // If the user manually selected a locale, do not change the selection.
      // Otherwise, select the new |defaultLocale|.
      var selected =
          languageSelect.manuallyChanged ? languageSelect.value : defaultLocale;
      languageSelect.innerHTML = '';
      var group = languageSelect;
      for (var i = 0; i < locales.length; ++i) {
        var item = locales[i];
        if (item.optionGroupName) {
          group = document.createElement('optgroup');
          group.label = item.optionGroupName;
          languageSelect.appendChild(group);
        } else {
          group.appendChild(new Option(item.title,
                                       item.value,
                                       item.value == selected,
                                       item.value == selected));
        }
      }
      languageSelect.multipleRecommendedLocales = multipleRecommendedLocales;

      // Retrieve a list of keyboard layouts applicable to the locale that is
      // now selected.
      this.getPublicSessionKeyboardLayouts_();
    },

    /**
     * Launches the public session with the user-selected locale and keyboard
     * layout (if available).
     * @private
     */
    enterPublicSession_: function() {
      if (this.isEnteringPublicSession_)
        return;
      this.isEnteringPublicSession_ = true;
      var locale = this.querySelector('.language-select').value;
      var keyboardSelect = this.querySelector('.keyboard-select');
      // The contents of |keyboardSelect| is updated asynchronously. If its
      // locale does not match |locale|, it has not updated yet and the
      // currently selected keyboard layout may not be applicable to |locale|.
      // Do not return any keyboard layout in this case and let the backend
      // choose a suitable layout.
      var keyboardLayout =
          keyboardSelect.loadedLocale == locale ? keyboardSelect.value : '';
      chrome.send(
          'launchPublicSession', [this.user.username, locale, keyboardLayout]);
    }
  };

  /**
   * Creates a user pod to be used only in desktop chrome.
   * @constructor
   * @extends {UserPod}
   */
  var DesktopUserPod = cr.ui.define(function() {
    // Don't just instantiate a UserPod(), as this will call decorate() on the
    // parent object, and add duplicate event listeners.
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  DesktopUserPod.prototype = {
    __proto__: UserPod.prototype,

    /** @override */
    initialize: function() {
      if (this.user.needsSignin) {
        if (this.user.hasLocalCreds) {
          this.user.initialAuthType = AUTH_TYPE.OFFLINE_PASSWORD;
        } else {
          this.user.initialAuthType = AUTH_TYPE.ONLINE_SIGN_IN;
        }
      }
      UserPod.prototype.initialize.call(this);
    },

    /** @override */
    get mainInput() {
      if (this.user.needsSignin)
        return this.passwordElement;
      else
        return this.nameElement;
    },

    /** @override */
    update: function() {
      this.imageElement.src = this.user.userImage;
      this.smallPodImageElement.src = this.user.userImage;
      this.nameElement.textContent = this.user.displayName;
      this.smallPodNameElement.textContent = this.user.displayName;
      this.reauthNameHintElement.textContent = this.user.displayName;

      var isLockedUser = this.user.needsSignin;
      var isLegacySupervisedUser = this.user.legacySupervisedUser;
      var isChildUser = this.user.childUser;
      var isSyncedUser = this.user.emailAddress !== "";
      var isProfileLoaded = this.user.isProfileLoaded;
      this.classList.toggle('locked', isLockedUser);
      this.classList.toggle('legacy-supervised', isLegacySupervisedUser);
      this.classList.toggle('child', isChildUser);
      this.classList.toggle('synced', isSyncedUser);

      if (this.isAuthTypeUserClick)
        this.passwordLabelElement.textContent = this.authValue;

      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
        'passwordFieldAccessibleName', this.user_.emailAddress));

      UserPod.prototype.updateActionBoxArea.call(this);
    },

    /** @override */
    activate: function(e) {
      if (!this.user.needsSignin) {
        Oobe.launchUser(this.user.profilePath);
      } else if (this.user.hasLocalCreds && !this.passwordElement.value) {
        return false;
      } else {
        chrome.send('authenticatedLaunchUser',
                    [this.user.profilePath,
                     this.user.emailAddress,
                     this.passwordElement.value]);
      }
      this.passwordElement.value = '';
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      if (this.getPodStyle() != UserPod.Style.LARGE) {
        $('pod-row').switchMainPod(this);
        return;
      }

      Oobe.clearErrors();
      this.parentNode.lastFocusedPod_ = this;

      // If this is a locked pod and there are local credentials, show the
      // password field.  Otherwise call activate() which will open up a browser
      // window or show the reauth dialog, as needed.
      if (!(this.user.needsSignin && this.user.hasLocalCreds) &&
          !this.isActionBoxMenuActive) {
        this.activate(e);
      }

      if (this.isAuthTypeUserClick)
        chrome.send('attemptUnlock', [this.user.emailAddress]);
    },
  };

  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether this user pod row is shown for the first time.
    firstShown_: true,

    // True if inside focusPod().
    insideFocusPod_: false,

    // Focused pod.
    focusedPod_: undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    // Pod that was most recently focused, if any.
    lastFocusedPod_: undefined,

    // Pod that occupies the main spot.
    mainPod_: undefined,

    // Pods whose initial images haven't been loaded yet.
    podsWithPendingImages_: [],

    // Whether pod placement has been postponed.
    podPlacementPostponed_: false,

    // Standard user pod height/width.
    userPodHeight_: 0,
    userPodWidth_: 0,

    // Array of users that are shown (public/supervised/regular).
    users_: [],

    // If we're in tablet mode.
    tabletModeEnabled_: false,

    // If testing mode is enabled.
    testingModeEnabled_: false,

    // The color used by the scroll list when the user count exceeds
    // LANDSCAPE_MODE_LIMIT or PORTRAIT_MODE_LIMIT.
    overlayColors_: {maskColor: undefined, scrollColor: undefined},

    // Whether we should add background behind user pods.
    showPodBackground_: false,

    // Current UI state of the sign-in screen.
    signinUIState_: SIGNIN_UI_STATE.HIDDEN,

    /** @override */
    decorate: function() {
      // Event listeners that are installed for the time period during which
      // the element is visible.
      this.listeners_ = {
        focus: [this.handleFocus_.bind(this), true /* useCapture */],
        click: [this.handleClick_.bind(this), true],
        mousemove: [this.handleMouseMove_.bind(this), false],
        keydown: [this.handleKeyDown.bind(this), false]
      };

      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var isNewDesktopUserManager = Oobe.getInstance().newDesktopUserManager;
      this.userPodHeight_ = isDesktopUserManager ?
          isNewDesktopUserManager ? MD_DESKTOP_POD_HEIGHT :
                                    DESKTOP_POD_HEIGHT :
          CROS_POD_HEIGHT;
      this.userPodWidth_ = isDesktopUserManager ?
          isNewDesktopUserManager ? MD_DESKTOP_POD_WIDTH :
                                    DESKTOP_POD_WIDTH :
          CROS_POD_WIDTH;
    },

    /**
     * Returns all the pods in this pod row. Some pods may not be its direct
     * children, but the caller doesn't have to know this.
     * @type {NodeList}
     */
    get pods() {
      var powRowChildren = Array.prototype.slice.call(this.children);
      var containerChildren =
          Array.prototype.slice.call(this.smallPodsContainer.children);
      return powRowChildren.concat(containerChildren);
    },

    /**
     * Return true if user pod row has only single user pod in it, which should
     * always be focused except desktop mode.
     * @type {boolean}
     */
    get alwaysFocusSinglePod() {
      return Oobe.getInstance().displayType !=
          DISPLAY_TYPE.DESKTOP_USER_MANAGER &&
          this.pods.length == 1;
    },

    /**
     * Returns pod with the given username (null if there is no such pod).
     * @param {string} username Username to be matched.
     * @return {Object} Pod with the given username. null if pod hasn't been
     *     found.
     */
    getPodWithUsername_: function(username) {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.username == username)
          return pod;
      }
      return null;
    },

    /**
     * True if the the pod row is disabled (handles no user interaction).
     * @type {boolean}
     */
    disabled_: false,
    get disabled() {
      return this.disabled_;
    },
    set disabled(value) {
      this.disabled_ = value;
      this.pods.forEach(function(pod) {
        pod.disabled = value;
      });
    },

    /**
     * Creates a user pod from given email.
     * @param {!Object} user User info dictionary.
     */
    createUserPod: function(user) {
      var userPod;
      if (user.isDesktopUser)
        userPod = new DesktopUserPod({user: user});
      else if (user.publicAccount)
        userPod = new PublicAccountUserPod({user: user});
      else
        userPod = new UserPod({user: user});

      userPod.hidden = false;
      return userPod;
    },

    /**
     * Add an existing user pod to this pod row.
     * @param {!Object} user User info dictionary.
     */
    addUserPod: function(user) {
      var userPod = this.createUserPod(user);
      this.appendChild(userPod);
      userPod.initialize();
    },

    /**
     * Performs visual changes on the user pod if there is an error.
     * @param {boolean} visible Whether to show or hide the display.
     */
    setFocusedPodErrorDisplay: function(visible) {
      if (this.focusedPod_)
        this.focusedPod_.showError = visible;
    },

    /**
     * Shows or hides the pin keyboard for the current focused pod.
     * @param {boolean} visible
     */
    setFocusedPodPinVisibility: function(visible) {
      if (this.focusedPod_)
        this.focusedPod_.setPinVisibility(visible);
    },

    /**
     * Enables or disables the pin keyboard for the given user. A disabled pin
     * keyboard will never be displayed.
     *
     * If the user's pod is focused, then enabling the pin keyboard will display
     * it; disabling the pin keyboard will hide it.
     * @param {!string} username
     * @param {boolean} enabled
     */
    setPinEnabled: function(username, enabled) {
      var pod = this.getPodWithUsername_(username);
      if (!pod) {
        console.error('Attempt to enable/disable pin keyboard of missing pod.');
        return;
      }

      // Make sure to set |pinEnabled| before toggling visiblity to avoid
      // validation errors.
      pod.pinEnabled = enabled;

      if (this.focusedPod_ == pod) {
        if (enabled) {
          ensurePinKeyboardLoaded(
              this.setPinVisibility.bind(this, username, true));
        } else {
          this.setPinVisibility(username, false);
        }
      }
    },

    /**
     * Shows or hides the pin keyboard from the pod with the given |username|.
     * This is only a visibility change; the pin keyboard can be reshown.
     *
     * Use setPinEnabled if the pin keyboard should be disabled for the given
     * user.
     * @param {!user} username
     * @param {boolean} visible
     */
    setPinVisibility: function(username, visible) {
      var pod = this.getPodWithUsername_(username);
      if (!pod) {
        console.error('Attempt to show/hide pin keyboard of missing pod.');
        return;
      }
      if (visible && pod.pinEnabled === false) {
        console.error('Attempt to show disabled pin keyboard');
        return;
      }
      if (visible && this.focusedPod_ != pod) {
        console.error('Attempt to show pin keyboard on non-focused pod');
        return;
      }

      pod.setPinVisibility(visible);
    },

    /**
     * Removes user pod from pod row.
     * @param {!user} username
     */
    removeUserPod: function(username) {
      var podToRemove = this.getPodWithUsername_(username);
      if (podToRemove == null) {
        console.warn('Attempt to remove pod that does not exist');
        return;
      }
      // Its parent is not necessarily this pod row.
      podToRemove.parentNode.removeChild(podToRemove);
      this.mainPod_ = null;
      if (this.pods.length > 0) {
        // placePods_() will select a new main pod and re-append pods
        // to different parents if necessary.
        this.placePods_();
        this.maybePreselectPod();
      }
    },

    /**
     * Returns index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    indexOf_: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        if (pod == this.pods[i])
          return i;
      }
      return -1;
    },

    /**
     * Populates pod row with given existing users and start init animation.
     * @param {array} users Array of existing user emails.
     */
    loadPods: function(users) {
      this.users_ = users;

      this.rebuildPods();
    },

    /**
     * Current header bar UI / sign in state.
     *
     * @type {number} state Current state of the sign-in screen (see
     *       SIGNIN_UI_STATE).
     */
    get signinUIState() {
      return this.signinUIState_;
    },

    set signinUIState(state) {
      this.signinUIState_ = state;
      this.rebuildPods();
    },

    /**
     * Rebuilds pod row using users_ that were previously set or
     * updated.
     */
    rebuildPods: function() {
      var emptyPodRow = this.pods.length == 0;

      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;
      this.lastFocusedPod_ = undefined;
      this.mainPod_ = undefined;
      this.smallPodsContainer.innerHTML = '';
      this.topMask.innerHTML = '';
      this.bottomMask.innerHTML = '';

      // Switch off animation
      Oobe.getInstance().toggleClass('flying-pods', false);

      for (var i = 0; i < this.users_.length; ++i)
        this.addUserPod(this.users_[i]);

      for (var i = 0, pod; pod = this.pods[i]; ++i)
        this.podsWithPendingImages_.push(pod);

      // Make sure we eventually show the pod row, even if some image is stuck.
      setTimeout(function() {
        $('pod-row').classList.remove('images-loading');
        this.smallPodsContainer.classList.remove('images-loading');
        this.topMask.classList.remove('images-loading');
        this.bottomMask.classList.remove('images-loading');
      }.bind(this), POD_ROW_IMAGES_LOAD_TIMEOUT_MS);

      var isAccountPicker =
          this.signinUIState_ == SIGNIN_UI_STATE.ACCOUNT_PICKER;

      // Immediately recalculate pods layout only when current UI is account
      // picker. Otherwise postpone it.
      if (isAccountPicker) {
        this.placePods_();
        this.maybePreselectPod();

        // Without timeout changes in pods positions will be animated even
        // though it happened when 'flying-pods' class was disabled.
        setTimeout(function() {
          Oobe.getInstance().toggleClass('flying-pods', true);
        }, 0);
      } else {
        this.podPlacementPostponed_ = true;
      }
    },

    /**
     * Gets the container of small pods.
     * @type {!HTMLDivElement}
     */
    get smallPodsContainer() {
      return document.querySelector('.small-pod-container');
    },

    /**
     * Gets the gradient mask at the top.
     * @type {!HTMLDivElement}
     */
    get topMask() {
      return document.querySelector('.small-pod-container-mask');
    },

    /**
     * Gets the gradient mask at the bottom.
     * @type {!HTMLDivElement}
     */
    get bottomMask() {
      return document.querySelector('.small-pod-container-mask.rotate');
    },

    /**
     * Shows a custom icon on a user pod besides the input field.
     * @param {string} username Username of pod to add button
     * @param {!{id: !string,
     *           hardlockOnClick: boolean,
     *           isTrialRun: boolean,
     *           ariaLabel: string | undefined,
     *           tooltip: ({text: string, autoshow: boolean} | undefined)}} icon
     *     The icon parameters.
     */
    showUserPodCustomIcon: function(username, icon) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to show user pod button: user pod not found.');
        return;
      }

      if (!icon.id && !icon.tooltip)
        return;

      if (icon.id)
        pod.customIconElement.setIcon(icon.id);

      if (icon.isTrialRun) {
        pod.customIconElement.setInteractive(
            this.onDidClickLockIconDuringTrialRun_.bind(this, username));
      } else if (icon.hardlockOnClick) {
        pod.customIconElement.setInteractive(
            this.hardlockUserPod_.bind(this, username));
      } else {
        pod.customIconElement.setInteractive(null);
      }

      var ariaLabel = icon.ariaLabel || (icon.tooltip && icon.tooltip.text);
      if (ariaLabel)
        pod.customIconElement.setAriaLabel(ariaLabel);
      else
        console.warn('No ARIA label for user pod custom icon.');

      pod.customIconElement.show();

      // This has to be called after |show| in case the tooltip should be shown
      // immediatelly.
      pod.customIconElement.setTooltip(
          icon.tooltip || {text: '', autoshow: false});

      // Hide fingerprint icon when custom icon is shown.
      this.setUserPodFingerprintIcon(username, FINGERPRINT_STATES.HIDDEN);
    },

    /**
     * Hard-locks user pod for the user. If user pod is hard-locked, it can be
     * only unlocked using password, and the authentication type cannot be
     * changed.
     * @param {!string} username The user's username.
     * @private
     */
    hardlockUserPod_: function(username) {
      chrome.send('hardlockPod', [username]);
    },

    /**
     * Records a metric indicating that the user clicked on the lock icon during
     * the trial run for Easy Unlock.
     * @param {!string} username The user's username.
     * @private
     */
    onDidClickLockIconDuringTrialRun_: function(username) {
      chrome.send('recordClickOnLockIcon', [username]);
    },

    /**
     * Hides the custom icon in the user pod added by showUserPodCustomIcon().
     * @param {string} username Username of pod to remove button
     */
    hideUserPodCustomIcon: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to hide user pod button: user pod not found.');
        return;
      }

      // TODO(tengs): Allow option for a fading transition.
      pod.customIconElement.hide();

      // Show fingerprint icon if applicable.
      this.setUserPodFingerprintIcon(username, FINGERPRINT_STATES.DEFAULT);
    },

    /**
     * Set a fingerprint icon in the user pod of |username|.
     * @param {string} username Username of the selected user
     * @param {number} state Fingerprint unlock state
     */
    setUserPodFingerprintIcon: function(username, state) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error(
            'Unable to set user pod fingerprint icon: user pod not found.');
        return;
      }
      pod.fingerprintAuthenticated_ = false;
      if (!pod.fingerprintIconElement)
        return;
      if (!pod.user.allowFingerprint || state == FINGERPRINT_STATES.HIDDEN ||
          !pod.customIconElement.hidden) {
        pod.fingerprintIconElement.hidden = true;
        pod.submitButton.hidden = false;
        return;
      }

      FINGERPRINT_STATES_MAPPING.forEach(function(icon) {
          pod.fingerprintIconElement.classList.toggle(
              icon.class, state == icon.state);
      });
      pod.fingerprintIconElement.hidden = false;
      pod.submitButton.hidden = pod.passwordElement.value.length == 0;
      this.updatePasswordField_(pod, state);
      if (state == FINGERPRINT_STATES.DEFAULT)
        return;

      pod.fingerprintAuthenticated_ = true;
      this.setActivatedPod(pod);
      if (state == FINGERPRINT_STATES.FAILED) {
        /** @const */ var RESET_ICON_TIMEOUT_MS = 500;
        setTimeout(
            this.resetIconAndPasswordField_.bind(this, pod),
            RESET_ICON_TIMEOUT_MS);
      }
    },

    /**
     * Reset the fingerprint icon and password field.
     * @param {UserPod} pod Pod to reset.
     */
    resetIconAndPasswordField_: function(pod) {
      if (!pod.fingerprintIconElement)
        return;
      this.setUserPodFingerprintIcon(
          pod.user.username, FINGERPRINT_STATES.DEFAULT);
    },

    /**
     * Remove the fingerprint icon in the user pod.
     * @param {string} username Username of the selected user
     */
    removeUserPodFingerprintIcon: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('No user pod found (when removing fingerprint icon).');
        return;
      }
      this.resetIconAndPasswordField_(pod);
      if (pod.fingerprintIconElement) {
        pod.fingerprintIconElement.parentNode.removeChild(
            pod.fingerprintIconElement);
      }
      pod.submitButton.hidden = false;
    },

    /**
     * Updates the password field in the user pod.
     * @param {UserPod} pod Pod to update.
     * @param {number} state Fingerprint unlock state
     */
    updatePasswordField_: function(pod, state) {
      FINGERPRINT_STATES_MAPPING.forEach(function(item) {
        pod.passwordElement.classList.toggle(item.class, state == item.state);
      });
      var placeholderStr = loadTimeData.getString(
          pod.isPinShown() ? 'pinKeyboardPlaceholderPinPassword' :
                             'passwordHint');
      if (state == FINGERPRINT_STATES.SIGNIN) {
        placeholderStr = loadTimeData.getString('fingerprintSigningin');
      } else if (state == FINGERPRINT_STATES.FAILED) {
        placeholderStr = loadTimeData.getString('fingerprintSigninFailed');
      }
      pod.passwordElement.placeholder = placeholderStr;
    },

    /**
     * Sets the authentication type used to authenticate the user.
     * @param {string} username Username of selected user
     * @param {number} authType Authentication type, must be one of the
     *                          values listed in AUTH_TYPE enum.
     * @param {string} value The initial value to use for authentication.
     */
    setAuthType: function(username, authType, value) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to set auth type: user pod not found.');
        return;
      }
      pod.setAuthType(authType, value);
    },

    /**
     * Sets the state of tablet mode.
     * @param {boolean} isTabletModeEnabled
     */
    setTabletModeState: function(isTabletModeEnabled) {
      this.tabletModeEnabled_ = isTabletModeEnabled;
      this.pods.forEach(function(pod, index) {
        pod.actionBoxAreaElement.classList.toggle(
            'forced', isTabletModeEnabled);
        if (pod.isPublicSessionPod) {
          pod.querySelector('.button-container')
              .classList.toggle('forced', isTabletModeEnabled);
        }
      });
    },

    /**
     * Sets whether the device is in demo mode.
     * @param {boolean} isDeviceInDemoMode
     */
    setDemoModeState: function(isDeviceInDemoMode) {
      for (let pod of this.pods) {
        if (pod.isPublicSessionPod)
          pod.skipExpandedView = isDeviceInDemoMode;
      }
    },

    /**
     * Updates the display name shown on a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} displayName The new display name
     */
    setPublicSessionDisplayName: function(userID, displayName) {
      var pod = this.getPodWithUsername_(userID);
      if (pod != null)
        pod.setDisplayName(displayName);
    },

    /**
     * Updates the list of locales available for a public session.
     * @param {string} userID The user ID of the public session
     * @param {!Object} locales The list of available locales
     * @param {string} defaultLocale The locale to select by default
     * @param {boolean} multipleRecommendedLocales Whether |locales| contains
     *     two or more recommended locales
     */
    setPublicSessionLocales: function(
        userID, locales, defaultLocale, multipleRecommendedLocales) {
      var pod = this.getPodWithUsername_(userID);
      if (pod != null) {
        pod.populateLanguageSelect(locales,
                                   defaultLocale,
                                   multipleRecommendedLocales);
      }
    },

    /**
     * Updates the list of available keyboard layouts for a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} locale The locale to which this list of keyboard layouts
     *     applies
     * @param {!Object} list List of available keyboard layouts
     */
    setPublicSessionKeyboardLayouts: function(userID, locale, list) {
      var pod = this.getPodWithUsername_(userID);
      if (pod != null)
        pod.populateKeyboardSelect(locale, list);
    },

    /**
     * Called when window was resized. The two common use cases are changing
     * screen orientation and showing the virtual keyboard.
     */
    onWindowResize: function() {
      var isAccountPicker =
          this.signinUIState_ == SIGNIN_UI_STATE.ACCOUNT_PICKER;
      if (isAccountPicker) {
        // Redo pod placement if account picker is the current screen.
        this.placePods_();
      } else {
        // Postpone pod placement. |handleBeforeShow| will check this flag.
        this.podPlacementPostponed_ = true;
      }
    },

    /**
     * Places pods onto their positions in pod grid matching the new design.
     * @private
     */
    placePods_: function() {
      var pods = this.pods;
      if (pods.length == 0) {
        console.error('Attempt to place pods for an empty pod list.');
        return;
      }
      // Appends all pods to their proper parents. Small pods have parent other
      // than the pod row. The pods were all initialized with the pow row as a
      // temporary parent, which is intended to ensure that all event listeners
      // work properly. If the main pod already exists, it means we are in the
      // process of resizing the window, then there is no need to change parents
      // of any pod.
      if (!this.mainPod_) {
        this.mainPod_ = this.preselectedPod;
        this.appendPodsToParents_();
      }
      this.handleBeforePodPlacement_();

      if (this.isScreenShrinked_()) {
        // When virtual keyboard is shown, the account picker should occupy
        // all the remaining screen. Screen size was already updated to exclude
        // the virtual keyboard.
        this.parentNode.setPreferredSize(
            this.screenSize.width, this.screenSize.height);
      } else {
        // Make sure not to block the header bar when virtual keyboard is absent.
        this.parentNode.setPreferredSize(
          Oobe.getInstance().clientAreaSize.width,
          Oobe.getInstance().clientAreaSize.height);
      }

      if (pods.length == 1) {
        this.placeSinglePod_();
      } else if (pods.length == POD_ROW_LIMIT) {
        this.placePodsOnPodRow_();
      } else {
        this.placePodsOnContainer_();
      }
      Oobe.getInstance().updateScreenSize(this.parentNode);
      this.handleAfterPodPlacement_();
    },

    /**
     * Appends pods to proper parents. Called each time before pod placement.
     * @private
     */
    appendPodsToParents_: function() {
      var pods = this.pods;
      // Pod count may have changed, so the placement method may change
      // accordingly. Therefore, always remove all pods from their current
      // parents first.
      for (var pod of pods) {
        pod.parentNode.removeChild(pod);
      }
      if (pods.length <= POD_ROW_LIMIT) {
        for (var pod of pods) {
          this.appendChild(pod);
        }
      } else {
        // When the user count exceeds the limit (currently set to 2), only the
        // main pod still has pow row as parent, all other pods should be
        // appended to the container with scroll bar.
        for (var pod of pods) {
          if (pod == this.mainPod_)
            this.appendChild(pod);
          else
            this.smallPodsContainer.appendChild(pod);
        }
      }
    },

    /**
     * Makes the screen unscrollable and hides the empty area underneath when
     * virtual keyboard is shown.
     * @private
     */
    hideEmptyArea_: function() {
      // When virtual keyboard is shown, although the screen size is reduced
      // properly, the size of #outer-container remains the same in order to
      // make other screens (e.g. Add person) scrollable, but this shouldn't
      // apply to account picker.
      // This is a hacky solution: we can make #scroll-container hide the
      // overflow area and manully position #inner-container.
      // NOTE: The global states set here might need to be cleared in
      //   handleHide. Please update the code there when adding new stuff here.
      var isScreenShrinked = this.isScreenShrinked_();
      $('scroll-container')
          .classList.toggle('disable-scroll', isScreenShrinked);
      $('inner-container').classList.toggle('disable-scroll', isScreenShrinked);
      $('inner-container').style.top = isScreenShrinked ?
          cr.ui.toCssPx($('scroll-container').scrollTop) :
          'unset';
    },

    /**
     * Called when there is one user pod.
     * @private
     */
    placeSinglePod_: function() {
      this.mainPod_.setPodStyle(UserPod.Style.LARGE);
      this.centerPod_(this.mainPod_, CROS_POD_WIDTH, CROS_POD_HEIGHT);
    },

    /**
     * Called when the number of pods is within the POD_ROW_LIMIT.
     * @private
     */
    placePodsOnPodRow_: function() {
      // Both pods have large size and are placed adjacently.
      var secondPod =
          this.pods[0] == this.mainPod_ ? this.pods[1] : this.pods[0];
      this.mainPod_.setPodStyle(UserPod.Style.LARGE);
      secondPod.setPodStyle(UserPod.Style.LARGE);

      var DOUBLE_PODS_PADDING = this.isPortraitMode_() ? 32 : 118;
      var leftPadding =
          (this.screenSize.width - (CROS_POD_WIDTH * 2 + DOUBLE_PODS_PADDING)) /
          2;
      // Start actual positioning.
      this.mainPod_.left = leftPadding;
      this.mainPod_.top = (this.screenSize.height - CROS_POD_HEIGHT) / 2;
      secondPod.left = leftPadding + CROS_POD_WIDTH + DOUBLE_PODS_PADDING;
      secondPod.top = (this.screenSize.height - CROS_POD_HEIGHT) / 2;
    },

    /**
     * Called when the number of pods exceeds the POD_ROW_LIMIT.
     * @private
     */
    placePodsOnContainer_: function() {
      this.smallPodsContainer.hidden = false;
      var pods = this.pods;
      if ((pods.length > LANDSCAPE_MODE_LIMIT && !this.isPortraitMode_()) ||
          (pods.length > PORTRAIT_MODE_LIMIT && this.isPortraitMode_())) {
        // If the pod count exceeds limits, they should be in extra small size
        // and the container will become scrollable.
        this.placePodsOnScrollableContainer_();
        return;
      }
      this.mainPod_.setPodStyle(UserPod.Style.LARGE);
      for (var pod of pods) {
        if (pod != this.mainPod_) {
          // All pods except the main one must be set to the small style.
          pod.setPodStyle(UserPod.Style.SMALL);
        }
      }
      // The size of the smallPodsContainer must be updated to avoid overflow.
      this.smallPodsContainer.style.height =
          cr.ui.toCssPx(this.screenSize.height);
      this.smallPodsContainer.style.width = cr.ui.toCssPx(CROS_SMALL_POD_WIDTH);

      var LEFT_PADDING = this.isPortraitMode_() ? 0 : 98;
      var MIDDLE_PADDING = this.isPortraitMode_() ? 84 : 220;
      var contentsWidth = LEFT_PADDING +
          CROS_POD_WIDTH + MIDDLE_PADDING + CROS_SMALL_POD_WIDTH;
      var blankWidth = this.screenSize.width - contentsWidth;
      var actualLeftPadding = LEFT_PADDING;
      actualLeftPadding +=
          this.isPortraitMode_() ? blankWidth * 2 / 3 : blankWidth / 2;
      var SMALL_POD_PADDING = 54;
      var actualSmallPodPadding = SMALL_POD_PADDING;
      var smallPodsTotalHeight = (pods.length - 1) * CROS_SMALL_POD_HEIGHT +
          (pods.length - 2) * actualSmallPodPadding;

      // SCROLL_TOP_PADDING denotes the smallest top padding we can tolerate
      // before allowing the container to overflow and show the scroll bar.
      var SCROLL_TOP_PADDING = this.isPortraitMode_() ? 66 : 72;
      if (smallPodsTotalHeight + SCROLL_TOP_PADDING * 2 >
          this.screenSize.height) {
        // In case the contents overflow for any reason (it shouldn't if the
        // pod count is within limits), fall to the scrollable container case.
        // But before that we'll try a smaller top padding and recalculate the
        // total height if virtual keyboard is shown.
        if (this.isScreenShrinked_()) {
          actualSmallPodPadding = 32;
          smallPodsTotalHeight = (pods.length - 1) * CROS_SMALL_POD_HEIGHT +
              (pods.length - 2) * actualSmallPodPadding;
        }
        // If virtual keyboard is not shown, or the updated total height still
        // exceeds screen height, fall to the scrollable container case.
        if (smallPodsTotalHeight + SCROLL_TOP_PADDING * 2 >
            this.screenSize.height) {
          this.placePodsOnScrollableContainer_();
          return;
        }
      }

      // Start positioning of the main pod and the smallPodsContainer.
      this.mainPod_.left = actualLeftPadding;
      this.mainPod_.top = (this.screenSize.height - CROS_POD_HEIGHT) / 2;
      this.smallPodsContainer.style.left =
          cr.ui.toCssPx(actualLeftPadding + CROS_POD_WIDTH + MIDDLE_PADDING);
      this.smallPodsContainer.style.top = cr.ui.toCssPx(0);
      // Start positioning of the small pods inside the smallPodsContainer.
      var smallPodsTopPadding =
          (this.screenSize.height - smallPodsTotalHeight) / 2;
      for (var pod of pods) {
        if (pod == this.mainPod_) {
          continue;
        }
        pod.left = 0;
        pod.top = smallPodsTopPadding;
        smallPodsTopPadding += CROS_SMALL_POD_HEIGHT + actualSmallPodPadding;
      }
    },

    /**
     * Called when the LANDSCAPE_MODE_LIMIT or PORTRAIT_MODE_LIMIT is exceeded
     * and the scrollable container is shown.
     * @private
     */
    placePodsOnScrollableContainer_: function() {
      this.smallPodsContainer.hidden = false;
      // Add a dark overlay.
      this.smallPodsContainer.classList.add('scroll');
      if (this.overlayColors_.scrollColor) {
        this.smallPodsContainer.style.backgroundColor =
            this.overlayColors_.scrollColor;
      }
      var pods = this.pods;
      this.mainPod_.setPodStyle(UserPod.Style.LARGE);
      for (var pod of pods) {
        if (pod != this.mainPod_) {
          // All pods except the main one must be set to the extra small style.
          pod.setPodStyle(UserPod.Style.EXTRA_SMALL);
        }
      }

      var SCROLL_LEFT_PADDING = this.isPortraitMode_() ? 46 : 72;
      var SCROLL_RIGHT_PADDING = this.isPortraitMode_() ? 12 : 72;
      // The offsetWidth of the smallPodsContainer.
      var scrollAreaWidth = SCROLL_LEFT_PADDING + CROS_EXTRA_SMALL_POD_WIDTH +
          SCROLL_RIGHT_PADDING;
      var mainPodPadding = (this.screenSize.width -
                            scrollAreaWidth - CROS_POD_WIDTH) / 2;
      var SCROLL_TOP_PADDING = this.isPortraitMode_() ? 66 : 72;
      var EXTRA_SMALL_POD_PADDING = 32;
      // Start positioning of the main pod and the smallPodsContainer.
      this.mainPod_.left = mainPodPadding;
      this.mainPod_.top = (this.screenSize.height - CROS_POD_HEIGHT) / 2;
      this.smallPodsContainer.style.left =
          cr.ui.toCssPx(mainPodPadding * 2 + CROS_POD_WIDTH);
      this.smallPodsContainer.style.top = cr.ui.toCssPx(0);

      // Precalculate the total height of the scrollable container and check if
      // it indeed exceeds the screen height.
      var scrollHeight = 0;
      for (var pod of pods) {
        if (pod == this.mainPod_) {
          continue;
        }
        scrollHeight += CROS_EXTRA_SMALL_POD_HEIGHT + EXTRA_SMALL_POD_PADDING;
      }
      scrollHeight -= EXTRA_SMALL_POD_PADDING;
      // The smallPodsContainer should occupy the full screen vertically.
      this.smallPodsContainer.style.height =
          cr.ui.toCssPx(this.screenSize.height);
      this.smallPodsContainer.style.width = cr.ui.toCssPx(
          SCROLL_LEFT_PADDING + CROS_EXTRA_SMALL_POD_WIDTH +
          SCROLL_RIGHT_PADDING);

      // SCROLL_TOP_PADDING denotes the smallest top padding we can tolerate
      // before allowing the container to overflow and show the scroll bar.
      var actualTopPadding = SCROLL_TOP_PADDING;
      if ((this.screenSize.height - scrollHeight) / 2 > actualTopPadding) {
        // Edge case: the total height of the scrollable container does not
        // exceed the screen height (minus the neceesary padding), so the
        // scroll bar will not appear.
        // In this case, we still want to keep the extra small pod size and
        // the overlay, but the top and bottom padding should be adjusted
        // to ensure a symmetric layout.
        actualTopPadding = (this.screenSize.height - scrollHeight) / 2;
      } else if (!this.isScreenShrinked_()) {
        // The scroll bar will definitely be shown if we reach here. A gradient
        // mask is applied to avoid blocking the header bar if the virtual
        // keyboard is not shown. When the keyboard is shown, there's no need
        // to add the mask and the original top padding value should be kept.
        actualTopPadding = SCROLL_MASK_HEIGHT;
        this.showScrollMask_();
      }

      // Start positioning of the small pods inside the smallPodsContainer.
      var topPadding = actualTopPadding;
      var lastPod = undefined;
      for (var pod of pods) {
        if (pod == this.mainPod_) {
          continue;
        }
        pod.left = SCROLL_LEFT_PADDING;
        pod.top = topPadding;
        topPadding += CROS_EXTRA_SMALL_POD_HEIGHT + EXTRA_SMALL_POD_PADDING;
        lastPod = pod;
      }
      // Make sure the last pod has a proper bottom padding for a symmetric
      // layout.
      lastPod.style.paddingBottom = cr.ui.toCssPx(actualTopPadding);
    },

    /**
     * Called each time before pod placement to ensure we start with the
     * initial state, which is ready to place only one user pod. The styles
     * of elements necessary for other placement methods must be set
     * explicitly each time.
     * @private
     */
    handleBeforePodPlacement_: function() {
      this.smallPodsContainer.hidden = true;
      this.topMask.hidden = true;
      this.bottomMask.hidden = true;
      this.smallPodsContainer.classList.remove('scroll');
      if (this.overlayColors_.scrollColor)
        this.smallPodsContainer.style.backgroundColor = 'unset';
      var pods = this.pods;
      for (var pod of pods) {
        // There is a chance that one of the pods has a bottom padding, so
        // reset all of them to be safe. This is because if the pod was at
        // the last position in the scrollable container, a bottom padding
        // was added to ensure a symmetric layout.
        pod.style.paddingBottom = cr.ui.toCssPx(0);
      }
      this.hideEmptyArea_();
      // Clear error bubbles whenever pod placement is happening, i.e., after
      // orientation change, showing or hiding virtual keyboard, and user
      // removal.
      Oobe.clearErrors();
    },

    /**
     * Checks if the screen is in portrait mode.
     * @return {boolean} True if in portrait mode.
     */
    isPortraitMode_: function() {
      return this.screenSize.width < this.screenSize.height;
    },

    /**
     * Checks if the screen is shrinked, i.e., when showing virtual keyboard.
     * We used to check Oobe.getInstance().virtualKeyboardShown directly
     * but there were occasional bugs because that value may not be updated yet
     * during pod placement.
     * @return {boolean} True if the screen is shrinked.
     */
    isScreenShrinked_: function() {
      return this.screenSize.height <= Oobe.getInstance().clientAreaSize.height;
    },

    /**
     * Called when scroll bar is shown and we need a mask for the header bar.
     * @private
     */
    showScrollMask_: function() {
      this.topMask.hidden = false;
      this.topMask.style.left = this.smallPodsContainer.style.left;
      this.topMask.style.width = this.smallPodsContainer.style.width;
      this.bottomMask.hidden = false;
      this.bottomMask.style.left = this.smallPodsContainer.style.left;
      this.bottomMask.style.width = this.smallPodsContainer.style.width;
      // The bottom mask should overlap with the header bar, and its z-index
      // is chosen to ensure it does not block users from using the header bar.
      this.bottomMask.style.top =
          cr.ui.toCssPx(this.screenSize.height - SCROLL_MASK_HEIGHT);
      if (this.overlayColors_.maskColor) {
        var maskGradient = this.getMaskGradient_(this.overlayColors_.maskColor);
        this.topMask.style.background = maskGradient;
        this.bottomMask.style.background = maskGradient;
      }
    },

    /**
     * Called after pod placement and before showing the pod row. Updates
     * elements whose style may depend on the pod placement outcome.
     * @private
     */
    handleAfterPodPlacement_: function() {
      var pods = this.pods;
      for (var pod of pods) {
        if (pod.getPodStyle() != UserPod.Style.LARGE)
          continue;
        // Make sure that user name on each large pod is centered and extra
        // long names don't exceed maximum pod width.
        var nameArea = pod.querySelector('.name-container');
        var leftMargin = (CROS_POD_WIDTH - pod.nameElement.offsetWidth) / 2;
        if (leftMargin > 0) {
          nameArea.style.left = cr.ui.toCssPx(leftMargin);
          nameArea.style.right = 'auto';
        } else {
          pod.nameElement.style.width = cr.ui.toCssPx(CROS_POD_WIDTH);
          nameArea.style.left = cr.ui.toCssPx(0);
          nameArea.style.right = 'auto';
          // For public session pods whose names are cut off, add a banner
          // which shows the full name upon hovering.
          if (pod.isPublicSessionPod && !pod.querySelector('.full-name')) {
            var fullNameContainer = document.createElement('div');
            fullNameContainer.classList.add('full-name');
            fullNameContainer.textContent = pod.nameElement.textContent;
            nameArea.appendChild(fullNameContainer);
          }
        }

        // Update info container area for public session pods.
        if (pod.isPublicSessionPod) {
          var infoElement = pod.querySelector('.info');
          var infoIcon = pod.querySelector('.learn-more');
          var totalWidth = PUBLIC_SESSION_ICON_WIDTH + infoElement.offsetWidth;
          var infoLeftMargin = (CROS_POD_WIDTH - totalWidth) / 2;
          if (infoLeftMargin > 0) {
            infoIcon.style.left = cr.ui.toCssPx(infoLeftMargin);
            infoIcon.style.right = 'auto';
            infoElement.style.left =
                cr.ui.toCssPx(infoLeftMargin + PUBLIC_SESSION_ICON_WIDTH);
            infoElement.style.right = 'auto';
          } else {
            infoIcon.style.left = cr.ui.toCssPx(0);
            infoIcon.style.right = 'auto';
            infoElement.style.left = cr.ui.toCssPx(PUBLIC_SESSION_ICON_WIDTH);
            infoElement.style.right = 'auto';
            infoElement.style.width = cr.ui.toCssPx(
                CROS_POD_WIDTH - PUBLIC_SESSION_ICON_WIDTH -
                infoElement.style.paddingLeft);
          }
          // If the public session pod is already expanded, make sure it's
          // still centered.
          if (pod.expanded)
            this.centerPod_(pod, PUBLIC_EXPANDED_WIDTH, PUBLIC_EXPANDED_HEIGHT);
        }

        // Update action box menu position to ensure it doesn't overlap with
        // elements outside the pod.
        var actionBoxMenu = pod.querySelector('.action-box-menu');
        var actionBoxButton = pod.querySelector('.action-box-button');
        var MENU_TOP_PADDING = 7;
        if (this.isPortraitMode_() && pods.length > 1) {
          // Confine the menu inside the pod when it may overlap with outside
          // elements.
          actionBoxMenu.style.left = 'auto';
          actionBoxMenu.style.right = cr.ui.toCssPx(0);
          actionBoxMenu.style.top =
              cr.ui.toCssPx(actionBoxButton.offsetHeight + MENU_TOP_PADDING);
        } else if (!this.isPortraitMode_() && this.isScreenShrinked_()) {
          // Prevent the virtual keyboard from blocking the remove user button.
          actionBoxMenu.style.left = cr.ui.toCssPx(
              pod.nameElement.offsetWidth + actionBoxButton.offsetWidth);
          actionBoxMenu.style.right = 'auto';
          actionBoxMenu.style.top = cr.ui.toCssPx(0);
        } else {
          actionBoxMenu.style.left = cr.ui.toCssPx(
              pod.nameElement.offsetWidth + actionBoxButton.style.marginLeft);
          actionBoxMenu.style.right = 'auto';
          actionBoxMenu.style.top =
              cr.ui.toCssPx(actionBoxButton.offsetHeight + MENU_TOP_PADDING);
        }

        // Update password container width based on the visibility of the
        // custom icon container.
        pod.passwordEntryContainerElement.classList.toggle(
            'custom-icon-shown', !pod.customIconElement.hidden);
        // Add ripple animation.
        var actionBoxRippleEffect =
            pod.querySelector('.action-box-button.ripple-circle');
        actionBoxRippleEffect.style.left = cr.ui.toCssPx(
            pod.nameElement.offsetWidth + actionBoxButton.style.marginLeft);
        actionBoxRippleEffect.style.right = 'auto';
        actionBoxRippleEffect.style.top =
            cr.ui.toCssPx(actionBoxButton.style.marginTop);
        // Adjust the vertical position of the pod if pin keyboard is shown.
        if (pod.isPinShown() && !this.isScreenShrinked_())
          pod.top = (this.screenSize.height - CROS_POD_HEIGHT_WITH_PIN) / 2;

        // In the end, switch direction of the above elements if right-to-left
        // language is used.
        if (isRTL()) {
          switchDirection(nameArea);
          switchDirection(actionBoxMenu);
          switchDirection(actionBoxRippleEffect);
          if (pod.isPublicSessionPod) {
            switchDirection(pod.querySelector('.info'));
            switchDirection(pod.querySelector('.learn-more'));
          }
        }
      }
      this.updateSigninBannerPosition_();
      this.togglePodBackground(this.showPodBackground_);
    },

    /**
     * Updates the sign-in banner position if it's shown. Called each time
     * after message update or pod placement, because the position of the
     * banner dynamically depends on the pod positions.
     * @private
     */
    updateSigninBannerPosition_: function() {
      var bannerContainer = $('signin-banner-container1');
      if (bannerContainer.hidden)
        return;
      if ($('signin-banner').classList.contains('warning')) {
        bannerContainer.style.top =
            cr.ui.toCssPx(this.mainPod_.top + CROS_POD_WARNING_BANNER_OFFSET_Y);
      } else {
        bannerContainer.style.top = cr.ui.toCssPx(this.mainPod_.top / 2);
      }
      if (this.pods.length <= POD_ROW_LIMIT) {
        bannerContainer.style.left = cr.ui.toCssPx(
            (this.screenSize.width - bannerContainer.offsetWidth) / 2);
      } else {
        var leftPadding = this.mainPod_.left -
            (bannerContainer.offsetWidth - CROS_POD_WIDTH) / 2;
        bannerContainer.style.left = cr.ui.toCssPx(Math.max(leftPadding, 0));
      }
    },

    /**
     * Handles required UI changes when a public session pod toggles the
     * expanded state.
     * @param {UserPod} pod Public session pod.
     * @param {boolean} expanded Whether the pod is expanded or not.
     */
    handlePublicPodExpansion: function(pod, expanded) {
      // Hide all other elements in the account picker if the pod is expanded.
      this.parentNode.classList.toggle('public-account-expanded', expanded);
      if (expanded) {
        this.centerPod_(pod, PUBLIC_EXPANDED_WIDTH, PUBLIC_EXPANDED_HEIGHT);
      } else {
        // Return the pod to its last position.
        pod.left = pod.lastPosition.left;
        pod.top = pod.lastPosition.top;
        // Pod placement has already finished by this point, but we still need
        // to make sure that the styles of all the elements in the pod row are
        // updated before being shown.
        this.handleAfterPodPlacement_();
      }
    },

    /**
     * Called when a pod needs to be centered.
     * @param {UserPod} pod Pod to be centered.
     * @param {number} podWidth The pod width.
     * @param {number} podHeight The pod height.
     * @private
     */
    centerPod_: function(pod, podWidth, podHeight) {
      // The original position of a public session pod is recorded in case of
      // future need.
      if (pod.isPublicSessionPod)
        pod.lastPosition = {left: pod.left, top: pod.top};
      // Avoid using offsetWidth and offsetHeight in case the pod is not fully
      // loaded yet.
      pod.left = (this.screenSize.width - podWidth) / 2;
      pod.top = (this.screenSize.height - podHeight) / 2;
    },

    /**
     * Toggles the animation for switching between main pod and small pod.
     * @param {UserPod} pod Pod that needs to toggle the animation.
     * @param {boolean} enabled Whether the switch animation is needed.
     * @private
     */
    toggleSwitchAnimation_: function(pod, enabled) {
      pod.imageElement.classList.toggle('switch-image-animation', enabled);
      pod.animatedImageElement.classList.toggle(
          'switch-image-animation', enabled);
      pod.smallPodImageElement.classList.toggle(
          'switch-image-animation', enabled);
      pod.smallPodAnimatedImageElement.classList.toggle(
          'switch-image-animation', enabled);
    },

    /**
     * Called when a small or extra small pod is clicked to trigger the switch
     * with the main pod.
     * @param {UserPod} pod Pod to be switched with the main pod.
     */
    switchMainPod: function(pod) {
      if (this.disabled) {
        console.error('Cannot switch main pod while sign-in UI is disabled.');
        return;
      }
      if (!this.mainPod_) {
        console.error('Attempt to switch a non-existing main pod.');
        return;
      }
      // Find the index of the small pod.
      var insert = 0;
      var children = pod.parentNode.children;
      while (insert < children.length && children[insert] != pod)
        insert++;
      if (insert >= children.length) {
        console.error('Attempt to switch a non-existing small pod.');
        return;
      }
      // Switch style of the two pods.
      this.mainPod_.setPodStyle(pod.getPodStyle());
      pod.setPodStyle(UserPod.Style.LARGE);
      // Add switch animation.
      this.toggleSwitchAnimation_(this.mainPod_, true);
      this.toggleSwitchAnimation_(pod, true);
      setTimeout(function() {
        var pods = this.pods;
        for (var pod of pods)
          this.toggleSwitchAnimation_(pod, false);
      }.bind(this), POD_SWITCH_ANIMATION_DURATION_MS);

      // Switch parent and position of the two pods.
      var left = pod.left;
      var top = pod.top;
      // Edge case: paddingBottom should be switched too because there's a
      // chance that the small pod was at the end of the scrollable container
      // and had a non-zero paddingBottom.
      var paddingBottom = pod.style.paddingBottom;
      var parent = pod.parentNode;
      parent.removeChild(pod);
      this.appendChild(pod);
      pod.left = this.mainPod_.left;
      pod.top = this.mainPod_.top;
      pod.style.paddingBottom = cr.ui.toCssPx(0);

      this.removeChild(this.mainPod_);
      // It must have the same index with the original small pod, instead
      // of being appended as the last child, in order to maintain the tab
      // order.
      parent.insertBefore(this.mainPod_, children[insert]);
      this.mainPod_.left = left;
      this.mainPod_.top = top;
      this.mainPod_.style.paddingBottom = paddingBottom;
      this.mainPod_ = pod;
      // The new main pod should already be focused but we need a focus update
      // in order to focus on the input box.
      this.focusPod(this.mainPod_, true /* force */);
      this.handleAfterPodPlacement_();
    },

    /**
     * Returns dimensions of screen including the header bar.
     * @type {Object}
     */
    get screenSize() {
      var container = $('scroll-container');
      return {width: container.offsetWidth, height: container.offsetHeight};
    },

    /**
     * Displays a banner containing |message|. If the banner is already present
     * this function updates the message in the banner.
     * @param {string} message Text to be displayed or empty to hide the banner.
     * @param {boolean} isWarning True if the given message is a warning.
     */
    showBannerMessage: function(message, isWarning) {
      var banner = $('signin-banner');
      banner.textContent = message;
      banner.classList.toggle('message-set', !!message);
      banner.classList.toggle('warning', isWarning);
      $('signin-banner-container1').hidden = banner.textContent.length == 0;
      this.updateSigninBannerPosition_();
    },

    /**
     * Sets login screen overlay colors based on colors extracted from the
     * wallpaper.
     * @param {string} maskColor Color for the gradient mask.
     * @param {string} scrollColor Color for the small pods container.
     */
    setOverlayColors: function(maskColor, scrollColor) {
      if (this.smallPodsContainer.classList.contains('scroll'))
        this.smallPodsContainer.style.backgroundColor = scrollColor;
      if (!this.topMask.hidden) {
        var maskGradient = this.getMaskGradient_(maskColor);
        this.topMask.style.background = maskGradient;
        this.bottomMask.style.background = maskGradient;
      }
      // Save the colors because the scrollable container and the masks may
      // become visible later.
      this.overlayColors_.maskColor = maskColor;
      this.overlayColors_.scrollColor = scrollColor;
    },

    /**
     * Helper function to create a style string for the scroll masks.
     * @param {string} maskColor Color for the gradient mask.
     * @return {string} A CSS style.
     */
    getMaskGradient_: function(maskColor) {
      return 'linear-gradient(' + maskColor + ', transparent)';
    },

    /**
     * Toggles the background behind user pods.
     * @param {boolean} showPodBackground Whether to add background behind user
     *     pods.
     */
    togglePodBackground: function(showPodBackground) {
      this.showPodBackground_ = showPodBackground;
      var pods = this.pods;
      for (var pod of pods)
        pod.classList.toggle('show-pod-background', showPodBackground);

      var isShowingScrollList =
          this.smallPodsContainer.classList.contains('scroll');
      if (isShowingScrollList) {
        if (showPodBackground) {
          // The scroll list should use a fixed color to make sure the pods are
          // legible.
          this.smallPodsContainer.style.backgroundColor = 'rgba(0, 0, 0, 0.9)';
        } else if (this.overlayColors_.scrollColor) {
          // Change the background back to the color extracted from wallpaper.
          this.smallPodsContainer.style.backgroundColor =
              this.overlayColors_.scrollColor;
        }
      }
      // Edge case: when we add pod background, we also need to add extra
      // padding to the pods if they are not placed on top of the scroll list.
      // The padding may result in overflow, so we allow showing overflow here.
      // An alternative is to adjust the size of the small pods container, but
      // we want to avoid changing the pod placement for this edge case.
      this.smallPodsContainer.classList.toggle(
          'show-overflow', showPodBackground && !isShowingScrollList);
    },

    /**
     * Whether the pod is currently focused.
     * @param {UserPod} pod Pod to check for focus.
     * @return {boolean} Pod focus status.
     */
    isFocused: function(pod) {
      return this.focusedPod_ == pod;
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod=} podToFocus User pod to focus (undefined clears focus).
     * @param {boolean=} opt_force If true, forces focus update even when
     *     podToFocus is already focused.
     * @param {boolean=} opt_skipInputFocus If true, don't focus on the input
     *     box of user pod.
     */
    focusPod: function(podToFocus, opt_force, opt_skipInputFocus) {
      if (this.isFocused(podToFocus) && !opt_force) {
        // Calling focusPod w/o podToFocus means reset.
        if (!podToFocus)
          Oobe.clearErrors();
        return;
      }

      // Make sure there's only one focusPod operation happening at a time.
      if (this.insideFocusPod_) {
        return;
      }
      this.insideFocusPod_ = true;

      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (!this.alwaysFocusSinglePod) {
          pod.isActionBoxMenuActive = false;
        }
        if (pod != podToFocus) {
          pod.isActionBoxMenuHovered = false;
          pod.classList.remove('focused');
          pod.setPinVisibility(false);
          this.setUserPodFingerprintIcon(
              pod.user.username, FINGERPRINT_STATES.HIDDEN);
          // On Desktop, the faded style is not set correctly, so we should
          // manually fade out non-focused pods if there is a focused pod.
          if (pod.user.isDesktopUser && podToFocus)
            pod.classList.add('faded');
          else
            pod.classList.remove('faded');
          pod.reset(false);
        }
      }

      // Clear any error messages for previous pod.
      if (!this.isFocused(podToFocus))
        Oobe.clearErrors();

      this.focusedPod_ = podToFocus;
      if (podToFocus) {
        // Only show the keyboard if it is fully loaded.
        if (podToFocus.isPinReady())
          podToFocus.setPinVisibility(true);
        podToFocus.classList.remove('faded');
        podToFocus.classList.add('focused');
        if (!podToFocus.multiProfilesPolicyApplied) {
          podToFocus.classList.toggle('signing-in', false);
          if (!opt_skipInputFocus)
            podToFocus.focusInput();
        } else {
          podToFocus.userTypeBubbleElement.classList.add('bubble-shown');
          // Note it is not necessary to skip this focus request when
          // |opt_skipInputFocus| is true. When |multiProfilesPolicyApplied|
          // is false, it doesn't focus on the password input box by default.
          podToFocus.focus();
        }

        // Only updates wallpaper when the focused pod is in large style.
        chrome.send('focusPod', [
          podToFocus.user.username,
          podToFocus.getPodStyle() == UserPod.Style.LARGE
        ]);
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;
        this.setUserPodFingerprintIcon(
            podToFocus.user.username, FINGERPRINT_STATES.DEFAULT);
      } else {
        chrome.send('noPodFocused');
      }
      this.insideFocusPod_ = false;
    },

    /**
     * Returns the currently activated pod.
     * @type {UserPod}
     */
    get activatedPod() {
      return this.activatedPod_;
    },

    /**
     * Sets currently activated pod.
     * @param {UserPod} pod Pod to check for focus.
     * @param {Event} e Event object.
     */
    setActivatedPod: function(pod, e) {
      if (this.disabled) {
        console.error('Cannot activate pod while sign-in UI is disabled.');
        return;
      }
      // If testing mode is enabled and a positive integer was entered, abort
      // the activation process and start testing mode.
      if (pod && this.testingModeEnabled_) {
        var userCount = pod.passwordElement.value;
        if (parseInt(userCount) == userCount && userCount > 0) {
          this.showDummyUsersForTesting(userCount);
          return;
        }
      }
      if (pod && pod.activate(e))
        this.activatedPod_ = pod;
    },

    /**
     * Used for testing only. Create the specified number of dummy users and
     * conveniently test the behaviors under different number of pods.
     * @param {number} count The number of users we want to test for.
     */
    showDummyUsersForTesting: function(count) {
      if (!this.testingModeEnabled_) {
        console.error(
            'Attempt to create dummy users when testing mode is disabled.');
        return;
      }
      var pods = this.pods;
      for (var pod of pods)
        pod.parentNode.removeChild(pod);
      var sampleUser = this.users_[0];
      var users = [];
      for (var i = 0; i < count; i++)
        users.push(sampleUser);

      this.loadPods(users);
    },

    /**
     * The pod that is preselected on user pod row show.
     * @type {?UserPod}
     */
    get preselectedPod() {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      if (isDesktopUserManager) {
        // On desktop, don't pre-select a pod if it's the only one.
        if (this.pods.length == 1)
          return null;

        // The desktop User Manager can send an URI encoded profile path in the
        // url hash, that indicates a pod that should be initially focused.
        var focusedProfilePath =
            decodeURIComponent(window.location.hash.substr(1));
        for (var i = 0, pod; pod = this.pods[i]; ++i) {
          if (focusedProfilePath === pod.user.profilePath)
            return pod;
        }
        return null;
      }

      for (i = 0; pod = this.pods[i]; ++i) {
        if (!pod.multiProfilesPolicyApplied)
          return pod;
      }
      return this.pods[0];
    },

    /**
     * Resets input UI.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.disabled = false;
      if (this.activatedPod_)
        this.activatedPod_.reset(takeFocus);
    },

    /**
     * Restores input focus to current selected pod, if there is any.
     */
    refocusCurrentPod: function() {
      if (this.focusedPod_ && !this.focusedPod_.multiProfilesPolicyApplied) {
        this.focusedPod_.focusInput();
      }
    },

    /**
     * Clears focused pod password field.
     */
    clearFocusedPod: function() {
      if (!this.disabled && this.focusedPod_)
        this.focusedPod_.reset(true);
    },

    /**
     * Shows signin UI.
     * @param {string} email Email for signin UI.
     */
    showSigninUI: function(email) {
      // Clear any error messages that might still be around.
      Oobe.clearErrors();
      this.disabled = true;
      this.lastFocusedPod_ = this.getPodWithUsername_(email);
      Oobe.showSigninUI(email);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod)
        pod.updateUserImage();
    },

    /**
     * Handler of click event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      // Clear all menus if the click is outside pod menu and its
      // button area.
      if (!findAncestorByClass(e.target, 'action-box-menu') &&
          !findAncestorByClass(e.target, 'action-box-area')) {
        for (var i = 0, pod; pod = this.pods[i]; ++i)
          pod.isActionBoxMenuActive = false;
      }

      // Clears focus if not clicked on a pod and if there's more than one pod.
      var pod = findAncestorByClass(e.target, 'pod');
      if ((!pod || pod.parentNode != this) && !this.alwaysFocusSinglePod) {
        this.focusPod();
      }

      if (pod && pod.getPodStyle() == UserPod.Style.LARGE)
        pod.isActionBoxMenuHovered = true;

      // Return focus back to single pod.
      if (this.alwaysFocusSinglePod && !pod) {
        this.focusPod(this.focusedPod_, true /* force */);
        this.focusedPod_.userTypeBubbleElement.classList.remove('bubble-shown');
        this.focusedPod_.isActionBoxMenuHovered = false;
        // If the click is outside the public session pod, still focus on it
        // but do not expand it any more.
        if (this.focusedPod_.isPublicSessionPod)
          this.focusedPod_.expanded = false;
      }
    },

    /**
     * Handler of mouse move event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleMouseMove_: function(e) {
      if (this.disabled)
        return;
      if (e.movementX == 0 && e.movementY == 0)
        return;

      // Defocus (thus hide) action box, if it is focused on a user pod
      // and the pointer is not hovering over it.
      var pod = findAncestorByClass(e.target, 'pod');
      if (document.activeElement &&
          document.activeElement.parentNode != pod &&
          document.activeElement.classList.contains('action-box-area')) {
        document.activeElement.parentNode.focus();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Hide action boxes on other user pods.
      for (var i = 0, p; p = this.pods[i]; ++i)
        if (p != pod && !p.isActionBoxMenuActive)
          p.isActionBoxMenuHovered = false;
    },

    /**
     * Handles focus event.
     * @param {Event} e Focus Event object.
     * @private
     */
    handleFocus_: function(e) {
      if (this.disabled)
        return;
      if (e.target.parentNode == this) {
        // Handles focus event on a large pod.
        if (e.target.classList.contains('focused')) {
          // Edge case: prevent input box from receiving unncessary focus
          // (thus hiding virtual keyboard) when remove user is clicked.
          if (e.target.isActionBoxMenuActive)
            return;
          if (!e.target.multiProfilesPolicyApplied)
            e.target.focusInput();
          else
            e.target.userTypeBubbleElement.classList.add('bubble-shown');
        } else
          this.focusPod(e.target);
        return;
      }

      // Small pods do not have input box.
      if (e.target.parentNode == this.smallPodsContainer) {
        this.focusPod(e.target, false, true /* opt_skipInputFocus */);
        return;
      }

      var pod = findAncestorByClass(e.target, 'pod');
      if (pod && pod.parentNode == this) {
        // Focus on a control of a pod but not on the action area button.
        if (!pod.classList.contains('focused')) {
          if (e.target.classList.contains('action-box-area') ||
              e.target.classList.contains('remove-warning-button')) {
            // focusPod usually moves focus on the password input box which
            // triggers virtual keyboard to show up. But the focus may move to a
            // non text input element shortly by e.target.focus. Hence, a
            // virtual keyboard flicking might be observed. We need to manually
            // prevent focus on password input box to avoid virtual keyboard
            // flicking in this case. See crbug.com/396016 for details.
            this.focusPod(pod, false, true /* opt_skipInputFocus */);
          } else {
            this.focusPod(pod);
          }
          pod.userTypeBubbleElement.classList.remove('bubble-shown');
          e.target.focus();
        }
        return;
      }

      // Clears pod focus when we reach here. It means new focus is neither
      // on a pod nor on a button/input for a pod.
      // Do not "defocus" user pod when it is a single pod.
      // That means that 'focused' class will not be removed and
      // input field/button will always be visible.
      if (!this.alwaysFocusSinglePod)
        this.focusPod();
      else {
        // Hide user-type-bubble in case this is one pod and we lost focus of
        // it.
        this.focusedPod_.userTypeBubbleElement.classList.remove('bubble-shown');
      }
    },

    /**
     * Handler of keydown event.
     * @param {Event} e KeyDown Event object.
     */
    handleKeyDown: function(e) {
      if (this.disabled)
        return;
      var editing = e.target.tagName == 'INPUT' && e.target.value;
      switch (e.key) {
        case 'ArrowLeft':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'ArrowRight':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          // Keydown events on public session pods should only be handled by
          // their own handler.
          if (this.focusedPod_ && !this.focusedPod_.isPublicSessionPod) {
            var targetTag = e.target.tagName;
            if (e.target == this.focusedPod_.passwordElement ||
                (this.focusedPod_.pinKeyboard &&
                 e.target == this.focusedPod_.pinKeyboard.inputElement) ||
                (targetTag != 'INPUT' &&
                 targetTag != 'BUTTON' &&
                 targetTag != 'A')) {
              this.setActivatedPod(this.focusedPod_, e);
              e.stopPropagation();
            }
          }
          break;
        case 'Escape':
          if (!this.alwaysFocusSinglePod)
            this.focusPod();
          break;
      }
      if (this.isValidInPassword(e.key)) {
        if (!this.focusedPod_ && this.mainPod_) {
          // If no pod is being focused, and a valid password key is entered,
          // move focus to the input field of the main pod. The key will be
          // treated as the first character of the password. Please note: when
          // the focus is on the header bar (excluding the status tray area),
          // this will also move focus to the main pod.
          this.focusPod(this.mainPod_);
        } else if (this.focusedPod_) {
          // If there's a focused pod but its input field is not focused (e.g.
          // when dropdown menu is shown, or crbug.com/725622), move focus to
          // the input field which will treat the key as password. This is a
          // no-op for small pods, or if the input field is already focused.
          this.focusedPod_.focusInput();
        }
      }
    },

    /**
     * Returns true if the key can be used in a valid password.
     * @param {string} key The character to check.
     * @return {boolean}
     */
    isValidInPassword: function(key) {
      // Passwords can consist of any ASCII characters per the guideline:
      // https://support.google.com/a/answer/33386. However we'll limit to
      // only alphanumeric and some special characters that we are sure won't
      // conflict with other keyboard events.
      // TODO(wzang): This should ideally be kept in sync with the requirements
      // set by the backend.
      if (key.length != 1)
        return false;
      return key.charCodeAt(0) > 31 && key.charCodeAt(0) < 127;
    },

    /**
     * Called right after the pod row is shown.
     */
    handleAfterShow: function() {
      var focusedPod = this.focusedPod_;

      // Without timeout changes in pods positions will be animated even though
      // it happened when 'flying-pods' class was disabled.
      setTimeout(function() {
        Oobe.getInstance().toggleClass('flying-pods', true);
        if (focusedPod)
          ensureTransitionEndEvent(focusedPod);
      }, 0);

      // Force input focus for user pod on show and once transition ends.
      if (focusedPod) {
        var screen = this.parentNode;
        var self = this;
        focusedPod.addEventListener('transitionend', function f(e) {
          focusedPod.removeEventListener('transitionend', f);
          focusedPod.reset(true);
          // Notify screen that it is ready.
          screen.onShow();
        });
      }
    },

    /**
     * Called right before the pod row is shown.
     */
    handleBeforeShow: function() {
      Oobe.getInstance().toggleClass('flying-pods', false);
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      if (this.podPlacementPostponed_) {
        this.podPlacementPostponed_ = false;
        this.placePods_();
        this.maybePreselectPod();
      }

      this.handleAfterPodPlacement_();
      // This is a hack for https://crbug.com/875128.
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE)
        document.documentElement.removeAttribute('full-screen-dialog');
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }

      // Clear global states that should only applies to account picker.
      $('scroll-container').classList.remove('disable-scroll');
      $('inner-container').classList.remove('disable-scroll');
      $('inner-container').style.top = 'unset';
    },

    /**
     * Called when a pod's user image finishes loading.
     */
    handlePodImageLoad: function(pod) {
      var index = this.podsWithPendingImages_.indexOf(pod);
      if (index == -1) {
        return;
      }

      this.podsWithPendingImages_.splice(index, 1);
      if (this.podsWithPendingImages_.length == 0) {
        this.classList.remove('images-loading');
        this.smallPodsContainer.classList.remove('images-loading');
        this.topMask.classList.remove('images-loading');
        this.bottomMask.classList.remove('images-loading');
      }
    },

    /**
     * Preselects pod, if needed.
     */
    maybePreselectPod: function() {
      var pod = this.preselectedPod;
      // Force a focus update to ensure the correct wallpaper is loaded.
      this.focusPod(pod, true /* force */);

      // Hide user-type-bubble in case all user pods are disabled and we focus
      // first pod.
      if (pod && pod.multiProfilesPolicyApplied) {
        pod.userTypeBubbleElement.classList.remove('bubble-shown');
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
