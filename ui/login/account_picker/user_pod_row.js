// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Number of displayed columns depending on user pod count.
   * @type {Array<number>}
   * @const
   */
  var COLUMNS = [0, 1, 2, 3, 4, 5, 4, 4, 4, 5, 5, 6, 6, 5, 5, 6, 6, 6, 6];

  /**
   * Mapping between number of columns in pod-row and margin between user pods
   * for such layout.
   * @type {Array<number>}
   * @const
   */
  var MARGIN_BY_COLUMNS = [undefined, 40, 40, 40, 40, 40, 12];

  /**
   * Mapping between number of columns in the desktop pod-row and margin
   * between user pods for such layout.
   * @type {Array<number>}
   * @const
   */
  var DESKTOP_MARGIN_BY_COLUMNS = [undefined, 32, 32, 32, 32, 32, 32];

  /**
   * Maximal number of columns currently supported by pod-row.
   * @type {number}
   * @const
   */
  var MAX_NUMBER_OF_COLUMNS = 6;

  /**
   * Maximal number of rows if sign-in banner is displayed alonside.
   * @type {number}
   * @const
   */
  var MAX_NUMBER_OF_ROWS_UNDER_SIGNIN_BANNER = 2;

  /**
   * Variables used for pod placement processing. Width and height should be
   * synced with computed CSS sizes of pods.
   */
  var CROS_POD_WIDTH = 180;
  var DESKTOP_POD_WIDTH = 180;
  var MD_DESKTOP_POD_WIDTH = 160;
  var PUBLIC_EXPANDED_BASIC_WIDTH = 500;
  var PUBLIC_EXPANDED_ADVANCED_WIDTH = 610;
  var CROS_POD_HEIGHT = 213;
  var DESKTOP_POD_HEIGHT = 226;
  var MD_DESKTOP_POD_HEIGHT = 200;
  var POD_ROW_PADDING = 10;
  var DESKTOP_ROW_PADDING = 32;
  var CUSTOM_ICON_CONTAINER_SIZE = 40;
  var CROS_PIN_POD_HEIGHT = 417;

  /**
   * Minimal padding between user pod and virtual keyboard.
   * @type {number}
   * @const
   */
  var USER_POD_KEYBOARD_MIN_PADDING = 20;

  /**
   * Maximum time for which the pod row remains hidden until all user images
   * have been loaded.
   * @type {number}
   * @const
   */
  var POD_ROW_IMAGES_LOAD_TIMEOUT_MS = 3000;

  /**
   * Tab order for user pods. Update these when adding new controls.
   * @enum {number}
   * @const
   */
  var UserPodTabOrder = {
    POD_INPUT: 1,        // Password input field, Action box menu button and
                         // the pod itself.
    PIN_KEYBOARD: 2,     // Pin keyboard below the password input field.
    POD_CUSTOM_ICON: 3,  // Pod custom icon next to password input field.
    HEADER_BAR: 4,       // Buttons on the header bar (Shutdown, Add User).
    POD_MENU_ITEM: 5     // User pad menu items (User info, Remove user).
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
  // (2) when a user pod is activated, its tab index is set to -1 and its
  // main input field gets focus and tab index 1;
  // (3) if user pod custom icon is interactive, it has tab index 2 so it
  // follows the input.
  // (4) buttons on the header bar have tab index 3 so they follow the custom
  // icon, or user pod if custom icon is not interactive;
  // (5) Action box buttons have tab index 4 and follow header bar buttons;
  // (6) lastly, focus jumps to the Status Area and back to user pods.
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
      this.hidden = validIcon ? false : true;
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
      var parentPod = this.parentNode;
      while (parentPod && !parentPod.classList.contains('pod'))
        parentPod = parentPod.parentNode;
      return parentPod && parentPod.parentNode.isFocused(parentPod);
    },

    /**
     * Depending on {@code this.tooltipState_}, it updates tooltip visibility
     * and text.
     * @private
     */
    updateTooltip_: function() {
      if (this.hidden || !this.isParentPodFocused_())
        return;

      if (!this.tooltipState_.active() || !this.tooltipState_.text) {
        this.hideTooltip_();
        return;
      }

      // Show the tooltip bubble.
      var bubbleContent = document.createElement('div');
      bubbleContent.textContent = this.tooltipState_.text;

      /** @const */ var BUBBLE_OFFSET = CUSTOM_ICON_CONTAINER_SIZE / 2;
      // TODO(tengs): Introduce a special reauth state for the account picker,
      // instead of showing the tooltip bubble here (crbug.com/409427).
      /** @const */ var BUBBLE_PADDING = 8 + (this.iconId_ ? 0 : 23);
      $('bubble').showContentForElement(this,
                                        cr.ui.Bubble.Attachment.LEFT,
                                        bubbleContent,
                                        BUBBLE_OFFSET,
                                        BUBBLE_PADDING);
    },

    /**
     * Hides the tooltip.
     * @private
     */
    hideTooltip_: function() {
      $('bubble').hideForElement(this);
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
      this.actionBoxAreaElement.addEventListener('focus', () => {
        this.isActionBoxMenuActive = false;
      });

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
     * Gets action box menu, remove user command item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuCommandElement() {
      return this.querySelector('.action-box-menu-remove-command');
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
     * Updates the user pod element.
     */
    update: function() {
      var animatedImageSrc = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];
      this.imageElement.src = animatedImageSrc + '&frame=0';
      this.animatedImageElement.src = animatedImageSrc;

      this.nameElement.textContent = this.user_.displayName;
      this.reauthNameHintElement.textContent = this.user_.displayName;
      this.classList.toggle('signed-in', this.user_.signedIn);

      if (this.isAuthTypeUserClick)
        this.passwordLabelElement.textContent = this.authValue;

      this.updateActionBoxArea();

      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
        'passwordFieldAccessibleName', this.user_.emailAddress));

      this.customizeUserPodPerUserType();
    },

    updateActionBoxArea: function() {
      if (this.user_.publicAccount) {
        this.actionBoxAreaElement.hidden = true;
        return;
      }

      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;

      this.actionBoxAreaElement.setAttribute(
          'aria-label', loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label', loadTimeData.getString(
               'podMenuRemoveItemAccessibleName'));
      this.actionBoxMenuTitleNameElement.textContent = this.user_.isOwner ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;

      this.actionBoxMenuTitleEmailElement.hidden =
          this.user_.legacySupervisedUser;

      this.actionBoxMenuCommandElement.textContent =
          loadTimeData.getString('removeUser');
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

      var elements = this.getElementsByClassName('pin-tag');
      for (var i = 0; i < elements.length; ++i)
        this.updatePinClass_(elements[i], visible);
      this.updatePinClass_(this, visible);

      // Set the focus to the input element after showing/hiding pin keyboard.
      this.mainInput.focus();

      // Change the password placeholder based on pin keyboard visibility.
      this.passwordElement.placeholder = loadTimeData.getString(visible ?
          'pinKeyboardPlaceholderPinPassword' : 'passwordHint');
    },

    isPinShown: function() {
      return this.classList.contains('pin-enabled');
    },

    setUserPodIconType: function(userTypeClass) {
      this.userTypeIconAreaElement.classList.add(userTypeClass);
      this.userTypeIconAreaElement.hidden = false;
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

        // If the user pod is on either edge of the screen, then the menu
        // could be displayed partially ofscreen.
        this.actionBoxMenu.classList.remove('left-edge-offset');
        this.actionBoxMenu.classList.remove('right-edge-offset');

        var offsetLeft =
            cr.ui.login.DisplayManager.getOffset(this.actionBoxMenu).left;
        var menuWidth = this.actionBoxMenu.offsetWidth;
        if (offsetLeft < 0)
          this.actionBoxMenu.classList.add('left-edge-offset');
        else if (offsetLeft + menuWidth > window.innerWidth)
          this.actionBoxMenu.classList.add('right-edge-offset');
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
     */
    focusInput: function() {
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
        if (!this.multiProfilesPolicyApplied)
          this.focusInput();  // This will set a custom tab order.
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
          if (this.parentNode.focusedPod_ && !this.isActionBoxMenuActive)
            this.isActionBoxMenuActive = true;
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
    updateRemoveWarningDialogSetMessage_: function(profilePath, message,
                                                   count) {
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
      this.userClickAuthAllowed_ = this.parentNode.isFocused(this);
    },

    /**
     * Called when the input of the password element changes. Updates the submit
     * button color and state and hides the error popup bubble.
     */
    updateInput_: function() {
      if (this.submitButton) {
        this.submitButton.disabled = this.passwordElement.value.length == 0;
        if (this.isFingerprintIconShown()) {
          this.submitButton.hidden = this.passwordElement.value.length == 0;
        } else {
          this.submitButton.hidden = false;
        }
      }
      this.showError = false;
      $('bubble').hide();
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
      if (!this.isAuthTypeUserClick || this.disabled)
        return;
      switch (e.key) {
        case 'Enter':
        case ' ':
          if (this.parentNode.isFocused(this))
            this.parentNode.setActivatedPod(this);
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
        this.usualLeft = this.left;
        this.makeSpaceForExpandedPod_();
      } else if (typeof(this.usualLeft) != 'undefined') {
        this.left = this.usualLeft;
      }

      var self = this;
      this.classList.add('animating');
      this.addEventListener('transitionend', function f(e) {
        self.removeEventListener('transitionend', f);
        self.classList.remove('animating');

        // Accessibility focus indicator does not move with the focused
        // element. Sends a 'focus' event on the currently focused element
        // so that accessibility focus indicator updates its location.
        if (document.activeElement)
          document.activeElement.dispatchEvent(new Event('focus'));
      });
      // Guard timer set to animation duration + 20ms.
      ensureTransitionEndEvent(this, 200);
    },

    get advanced() {
      return this.classList.contains('advanced');
    },

    /** @override */
    get mainInput() {
      if (this.expanded)
        return this.enterButtonElement;
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

      var languageAndInput = this.querySelector('.language-and-input');
      languageAndInput.tabIndex = UserPodTabOrder.POD_INPUT;
      languageAndInput.addEventListener('click',
                                        this.transitionToAdvanced_.bind(this));

      var monitoringLearnMore = this.querySelector('.monitoring-learn-more');
      monitoringLearnMore.tabIndex = UserPodTabOrder.POD_INPUT;
      monitoringLearnMore.addEventListener(
          'click', this.onMonitoringLearnMoreClicked_.bind(this));

      this.enterButtonElement.addEventListener('click', (function(e) {
        this.enterButtonElement.disabled = true;
        var locale = this.querySelector('.language-select').value;
        var keyboardSelect = this.querySelector('.keyboard-select');
        // The contents of |keyboardSelect| is updated asynchronously. If its
        // locale does not match |locale|, it has not updated yet and the
        // currently selected keyboard layout may not be applicable to |locale|.
        // Do not return any keyboard layout in this case and let the backend
        // choose a suitable layout.
        var keyboardLayout =
            keyboardSelect.loadedLocale == locale ? keyboardSelect.value : '';
        chrome.send('launchPublicSession',
                    [this.user.username, locale, keyboardLayout]);
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
      this.querySelector('.expanded-pane-name').textContent =
          this.user_.displayName;
      this.querySelector('.info').textContent =
          loadTimeData.getStringF('publicAccountInfoFormat',
                                  this.user_.enterpriseDisplayDomain);
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
        this.expanded = true;
        this.focusInput();
      }
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

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

    makeSpaceForExpandedPod_: function() {
      var width = this.classList.contains('advanced') ?
          PUBLIC_EXPANDED_ADVANCED_WIDTH : PUBLIC_EXPANDED_BASIC_WIDTH;
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                              POD_ROW_PADDING;
      if (this.left + width > $('pod-row').offsetWidth - rowPadding)
        this.left = $('pod-row').offsetWidth - rowPadding - width;
    },

    /**
     * Transition the expanded pod from the basic to the advanced view.
     */
    transitionToAdvanced_: function() {
      var pod = this;
      var languageAndInputSection =
          this.querySelector('.language-and-input-section');
      this.classList.add('transitioning-to-advanced');
      setTimeout(function() {
        pod.classList.add('advanced');
        pod.makeSpaceForExpandedPod_();
        languageAndInputSection.addEventListener('transitionend',
                                                 function observer() {
          languageAndInputSection.removeEventListener('transitionend',
                                                      observer);
          pod.classList.remove('transitioning-to-advanced');
          pod.querySelector('.language-select').focus();
        });
        // Guard timer set to animation duration + 20ms.
        ensureTransitionEndEvent(languageAndInputSection, 380);
      }, 0);
    },

    /**
     * Show a dialog when user clicks on learn more (monitoring) button.
     */
    onMonitoringLearnMoreClicked_: function() {
      if (!this.dialogContainer_) {
        this.dialogContainer_ = document.createElement('div');
        this.dialogContainer_.classList.add('monitoring-dialog-container');
        var topContainer = document.querySelector('#scroll-container');
        topContainer.appendChild(this.dialogContainer_);
      }
      // Public Session POD in advanced view has a different size so add a dummy
      // parent element to enable different CSS settings.
      this.dialogContainer_.classList.toggle(
          'advanced', this.classList.contains('advanced'))
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
      this.dialog_ = new cr.ui.dialogs.BaseDialog(this.dialogContainer_);
      this.dialog_.showHtml(title, html, undefined,
                            this.onMonitoringDialogClosed_.bind(this));
      this.parentNode.disabled = true;
    },

    /**
     * Cleanup after the monitoring warning dialog is closed.
     */
    onMonitoringDialogClosed_: function() {
      this.parentNode.disabled = false;
      this.dialog_ = undefined;
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
    populateLanguageSelect: function(locales,
                                     defaultLocale,
                                     multipleRecommendedLocales) {
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
      if (this.user.needsSignin && this.user.hasLocalCreds)
        return this.passwordElement;
      else
        return this.nameElement;
    },

    /** @override */
    update: function() {
      this.imageElement.src = this.user.userImage;
      this.animatedImageElement.src = this.user.userImage;
      this.nameElement.textContent = this.user.displayName;
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
     * Returns all the pods in this pod row.
     * @type {NodeList}
     */
    get pods() {
      return Array.prototype.slice.call(this.children);
    },

    /**
     * Return true if user pod row has only single user pod in it, which should
     * always be focused except desktop and tablet modes.
     * @type {boolean}
     */
    get alwaysFocusSinglePod() {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;

      return (isDesktopUserManager || this.tabletModeEnabled_) ?
          false :
          this.children.length == 1;
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
      this.removeChild(podToRemove);
      if (this.pods.length > 0)
        this.placePods_();
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
     * Scrolls focused user pod into view.
     */
    scrollFocusedPodIntoView: function() {
      var pod = this.focusedPod_;
      if (!pod)
        return;

      // First check whether focused pod is already fully visible.
      var visibleArea = $('scroll-container');
      // Visible area may not defined at user manager screen on all platforms.
      // Windows, Mac and Linux do not have visible area.
      if (!visibleArea)
        return;
      var scrollTop = visibleArea.scrollTop;
      var clientHeight = visibleArea.clientHeight;
      var podTop = $('oobe').offsetTop + pod.offsetTop;
      var padding = USER_POD_KEYBOARD_MIN_PADDING;
      if (podTop + pod.height + padding <= scrollTop + clientHeight &&
          podTop - padding >= scrollTop) {
        return;
      }

      // Scroll so that user pod is as centered as possible.
      visibleArea.scrollTop = podTop - (clientHeight - pod.offsetHeight) / 2;
    },

    /**
     * Rebuilds pod row using users_ that were previously set or updated.
     */
    rebuildPods: function() {
      var emptyPodRow = this.pods.length == 0;

      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;
      this.lastFocusedPod_ = undefined;

      // Switch off animation
      Oobe.getInstance().toggleClass('flying-pods', false);

      // Populate the pod row.
      for (var i = 0; i < this.users_.length; ++i)
        this.addUserPod(this.users_[i]);

      for (var i = 0, pod; pod = this.pods[i]; ++i)
        this.podsWithPendingImages_.push(pod);

      // Make sure we eventually show the pod row, even if some image is stuck.
      setTimeout(function() {
        $('pod-row').classList.remove('images-loading');
      }, POD_ROW_IMAGES_LOAD_TIMEOUT_MS);

      var isAccountPicker = $('login-header-bar').signinUIState ==
          SIGNIN_UI_STATE.ACCOUNT_PICKER;

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
     * @param {boolean} isTabletModeEnabled true if the mode is on.
     */
    setTabletModeState: function(isTabletModeEnabled) {
      this.tabletModeEnabled_ = isTabletModeEnabled;
      this.pods.forEach(function(pod, index) {
        pod.actionBoxAreaElement.classList.toggle(
            'forced', isTabletModeEnabled);
      });
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
    setPublicSessionLocales: function(userID,
                                      locales,
                                      defaultLocale,
                                      multipleRecommendedLocales) {
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
     * Called when window was resized.
     */
    onWindowResize: function() {
      var layout = this.calculateLayout_();
      if (layout.columns != this.columns || layout.rows != this.rows)
        this.placePods_();

      // Wrap this in a set timeout so the function is called after the pod is
      // finished transitioning so that we work with the final pod dimensions.
      // If there is no focused pod that may be transitioning when this function
      // is called, we can call scrollFocusedPodIntoView() right away.
      var timeOut = 0;
      if (this.focusedPod_) {
        var style = getComputedStyle(this.focusedPod_);
        timeOut = parseFloat(style.transitionDuration) * 1000;
      }

      setTimeout(function() {
        this.scrollFocusedPodIntoView();
      }.bind(this), timeOut);
    },

    /**
     * Returns width of podrow having |columns| number of columns.
     * @private
     */
    columnsToWidth_: function(columns) {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var margin = isDesktopUserManager ? DESKTOP_MARGIN_BY_COLUMNS[columns] :
                                          MARGIN_BY_COLUMNS[columns];
      var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                              POD_ROW_PADDING;
      return 2 * rowPadding + columns * this.userPodWidth_ +
          (columns - 1) * margin;
    },

    /**
     * Returns height of podrow having |rows| number of rows.
     * @private
     */
    rowsToHeight_: function(rows) {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                              POD_ROW_PADDING;
      return 2 * rowPadding + rows * this.userPodHeight_;
    },

    /**
     * Calculates number of columns and rows that podrow should have in order to
     * hold as much its pods as possible for current screen size. Also it tries
     * to choose layout that looks good.
     * @return {{columns: number, rows: number}}
     */
    calculateLayout_: function() {
      var preferredColumns = this.pods.length < COLUMNS.length ?
          COLUMNS[this.pods.length] : COLUMNS[COLUMNS.length - 1];
      var maxWidth = Oobe.getInstance().clientAreaSize.width;
      var columns = preferredColumns;
      while (maxWidth < this.columnsToWidth_(columns) && columns > 1)
        --columns;
      var rows = Math.floor((this.pods.length - 1) / columns) + 1;
      if (getComputedStyle(
          $('signin-banner'), null).getPropertyValue('display') != 'none') {
        rows = Math.min(rows, MAX_NUMBER_OF_ROWS_UNDER_SIGNIN_BANNER);
      }
      if (!Oobe.getInstance().newDesktopUserManager) {
        var maxHeigth = Oobe.getInstance().clientAreaSize.height;
        while (maxHeigth < this.rowsToHeight_(rows) && rows > 1)
         --rows;
      }
      // One more iteration if it's not enough cells to place all pods.
      while (maxWidth >= this.columnsToWidth_(columns + 1) &&
             columns * rows < this.pods.length &&
             columns < MAX_NUMBER_OF_COLUMNS) {
         ++columns;
      }
      return {columns: columns, rows: rows};
    },

    /**
     * Places pods onto their positions onto pod grid.
     * @private
     */
    placePods_: function() {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      if (isDesktopUserManager && !Oobe.getInstance().userPodsPageVisible)
        return;

      var layout = this.calculateLayout_();
      var columns = this.columns = layout.columns;
      var rows = this.rows = layout.rows;
      var maxPodsNumber = columns * rows;
      var margin = isDesktopUserManager ? DESKTOP_MARGIN_BY_COLUMNS[columns] :
                                          MARGIN_BY_COLUMNS[columns];
      this.parentNode.setPreferredSize(
          this.columnsToWidth_(columns), this.rowsToHeight_(rows));
      var height = this.userPodHeight_;
      var width = this.userPodWidth_;
      var pinPodLocation = { column: columns + 1, row: rows + 1 };
      if (this.focusedPod_ && this.focusedPod_.isPinShown())
        pinPodLocation = this.findPodLocation_(this.focusedPod_, columns, rows);

      this.pods.forEach(function(pod, index) {
        if (index >= maxPodsNumber) {
           pod.hidden = true;
           return;
        }
        pod.hidden = false;
        if (pod.offsetHeight != height &&
            pod.offsetHeight != CROS_PIN_POD_HEIGHT) {
          console.error('Pod offsetHeight (' + pod.offsetHeight +
              ') and POD_HEIGHT (' + height + ') are not equal.');
        }
        if (pod.offsetWidth != width) {
          console.error('Pod offsetWidth (' + pod.offsetWidth +
              ') and POD_WIDTH (' + width + ') are not equal.');
        }
        var column = index % columns;
        var row = Math.floor(index / columns);

        var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                                POD_ROW_PADDING;
        pod.left = rowPadding + column * (width + margin);

        // On desktop, we want the rows to always be equally spaced.
        pod.top = isDesktopUserManager ? row * (height + rowPadding) :
                                         row * height + rowPadding;
      });
      Oobe.getInstance().updateScreenSize(this.parentNode);
    },

    /**
     * Number of columns.
     * @type {?number}
     */
    set columns(columns) {
      // Cannot use 'columns' here.
      this.setAttribute('ncolumns', columns);
    },
    get columns() {
      return parseInt(this.getAttribute('ncolumns'));
    },

    /**
     * Number of rows.
     * @type {?number}
     */
    set rows(rows) {
      // Cannot use 'rows' here.
      this.setAttribute('nrows', rows);
    },
    get rows() {
      return parseInt(this.getAttribute('nrows'));
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

        chrome.send(
            'focusPod', [podToFocus.user.username, true /* loads wallpaper */]);
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;
        this.scrollFocusedPodIntoView();
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
      if (pod && pod.activate(e))
        this.activatedPod_ = pod;
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

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Return focus back to single pod.
      if (this.alwaysFocusSinglePod && !pod) {
        if ($('login-header-bar').contains(e.target))
          return;
        this.focusPod(this.focusedPod_, true /* force */);
        this.focusedPod_.userTypeBubbleElement.classList.remove('bubble-shown');
        this.focusedPod_.isActionBoxMenuHovered = false;
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
        // Focus on a pod
        if (e.target.classList.contains('focused')) {
          if (!e.target.multiProfilesPolicyApplied)
            e.target.focusInput();
          else
            e.target.userTypeBubbleElement.classList.add('bubble-shown');
        } else
          this.focusPod(e.target);
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
          if (this.focusedPod_) {
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
      $('login-header-bar').buttonsTabIndex = UserPodTabOrder.HEADER_BAR;

      if (this.podPlacementPostponed_) {
        this.podPlacementPostponed_ = false;
        this.placePods_();
        this.maybePreselectPod();
      }
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = 0;
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
      }
    },

    /**
     * Preselects pod, if needed.
     */
     maybePreselectPod: function() {
       var pod = this.preselectedPod;
       this.focusPod(pod);

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
