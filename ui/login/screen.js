// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for all login WebUI screens.
 */
cr.define('login', function() {
  /** @const */ var CALLBACK_USER_ACTED = 'userActed';

  function doNothing() {};

  function alwaysTruePredicate() { return true; }

  var querySelectorAll = HTMLDivElement.prototype.querySelectorAll;

  var Screen = function(sendPrefix) {
    this.sendPrefix_ = sendPrefix;
  };

  Screen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Prefix added to sent to Chrome messages' names.
     */
    sendPrefix_: null,

    /**
     * Called during screen initialization.
     */
    decorate: doNothing,

    /**
     * Returns minimal size that screen prefers to have. Default implementation
     * returns current screen size.
     * @return {{width: number, height: number}}
     */
    getPreferredSize: function() {
      return {width: this.offsetWidth, height: this.offsetHeight};
    },

    /**
     * Called for currently active screen when screen size changed.
     */
    onWindowResize: doNothing,

    /**
     * @final
     */
    initialize: function() {
      return this.initializeImpl_.apply(this, arguments);
    },

    /**
     * @final
     */
    send: function() {
      return this.sendImpl_.apply(this, arguments);
    },

    /**
     * @override
     * @final
     */
    querySelectorAll: function() {
      return this.querySelectorAllImpl_.apply(this, arguments);
    },

    /**
     * @private
     */
    initializeImpl_: function() {
      this.decorate();
    },

    /**
     * Sends message to Chrome, adding needed prefix to message name. All
     * arguments after |messageName| are packed into message parameters list.
     *
     * @param {string} messageName Name of message without a prefix.
     * @param {...*} varArgs parameters for message.
     * @private
     */
    sendImpl_: function(messageName, varArgs) {
      if (arguments.length == 0)
        throw Error('Message name is not provided.');
      var fullMessageName = this.sendPrefix_ + messageName;
      var payload = Array.prototype.slice.call(arguments, 1);
      chrome.send(fullMessageName, payload);
    },

    /**
     * Calls standart |querySelectorAll| method and returns its result converted
     * to Array.
     * @private
     */
    querySelectorAllImpl_: function(selector) {
      var list = querySelectorAll.call(this, selector);
      return Array.prototype.slice.call(list);
    },

    /**
     * If |value| is the value of some property of |this| returns property's
     * name. Otherwise returns empty string.
     * @private
     */
    getPropertyNameOf_: function(value) {
      for (var key in this)
        if (this[key] === value)
          return key;
      return '';
    }
  };

  Screen.CALLBACK_USER_ACTED = CALLBACK_USER_ACTED;

  return {
    Screen: Screen
  };
});

cr.define('login', function() {
  return {
    /**
     * Creates class and object for screen.
     * Methods specified in EXTERNAL_API array of prototype
     * will be available from C++ part.
     * Example:
     *     login.createScreen('ScreenName', 'screen-id', {
     *       foo: function() { console.log('foo'); },
     *       bar: function() { console.log('bar'); }
     *       EXTERNAL_API: ['foo'];
     *     });
     *     login.ScreenName.register();
     *     var screen = $('screen-id');
     *     screen.foo(); // valid
     *     login.ScreenName.foo(); // valid
     *     screen.bar(); // valid
     *     login.ScreenName.bar(); // invalid
     *
     * @param {string} name Name of created class.
     * @param {string} id Id of div representing screen.
     * @param {(function()|Object)} proto Prototype of object or function that
     *     returns prototype.
     */
    createScreen: function(name, id, template) {
      if (typeof template == 'function')
        template = template();

      var apiNames = template.EXTERNAL_API || [];
      for (var i = 0; i < apiNames.length; ++i) {
        var methodName = apiNames[i];
        if (typeof template[methodName] !== 'function')
          throw Error('External method "' + methodName + '" for screen "' +
              name + '" not a function or undefined.');
      }

      function checkPropertyAllowed(propertyName) {
        if (propertyName.charAt(propertyName.length - 1) === '_' &&
            (propertyName in login.Screen.prototype)) {
          throw Error('Property "' + propertyName + '" of "' + id + '" ' +
              'shadows private property of login.Screen prototype.');
        }
      };

      var Constructor = function() {
        login.Screen.call(this, 'login.' + name + '.');
      };
      Constructor.prototype = Object.create(login.Screen.prototype);
      var api = {};

      Object.getOwnPropertyNames(template).forEach(function(propertyName) {
        if (propertyName === 'EXTERNAL_API')
          return;

        checkPropertyAllowed(propertyName);

        var descriptor =
            Object.getOwnPropertyDescriptor(template, propertyName);
        Object.defineProperty(Constructor.prototype, propertyName, descriptor);

        if (apiNames.indexOf(propertyName) >= 0) {
          api[propertyName] = function() {
              var screen = $(id);
              return screen[propertyName].apply(screen, arguments);
          };
        }
      });

      Constructor.prototype.name = function() { return id; };

      api.register = function(opt_lazy_init) {
        var screen = $(id);
        screen.__proto__ = new Constructor();

        if (opt_lazy_init !== undefined && opt_lazy_init)
          screen.deferredInitialization = function() { screen.initialize(); }
        else
          screen.initialize();
        Oobe.getInstance().registerScreen(screen);
      };

      // See also c/b/r/chromeos/login/login_screen_behavior.js
      cr.define('login', function() {
        var result = {};
        result[name] = api;
        return result;
      });
    }
  };
});
