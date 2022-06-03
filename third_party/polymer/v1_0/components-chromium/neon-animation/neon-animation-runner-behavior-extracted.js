/**
   * `Polymer.NeonAnimationRunnerBehavior` adds a method to run animations.
   *
   * @polymerBehavior Polymer.NeonAnimationRunnerBehavior
   */
  Polymer.NeonAnimationRunnerBehaviorImpl = {

    _configureAnimations: function(configs) {
      var results = [];
      var resultsToPlay = [];

      if (configs.length > 0) {
        for (var config, index = 0; config = configs[index]; index++) {
          var neonAnimation = document.createElement(config.name);
          // is this element actually a neon animation?
          if (neonAnimation.isNeonAnimation) {
            var result = null;
            // Closure compiler does not work well with a try / catch here. .configure needs to be
            // explicitly defined
            if (!neonAnimation.configure) {
              /**
               * @param {Object} config
               * @return {AnimationEffect}
               */
              neonAnimation.configure = function(config) {
                return null;
              }
            }

            result = neonAnimation.configure(config);
            resultsToPlay.push({ result: result, config: config });
          } else {
            console.warn(this.is + ':', config.name, 'not found!');
          }
        }
      }

      for (var i = 0; i < resultsToPlay.length; i++) {
        var result = resultsToPlay[i].result;
        var config = resultsToPlay[i].config;
        // configuration or play could fail if polyfills aren't loaded
        try {
          // Check if we have an Effect rather than an Animation
          if (typeof result.cancel != 'function') {
            result = document.timeline.play(result);
          }
        } catch (e) {
          result = null;
          console.warn('Couldnt play', '(', config.name, ').', e);
        }

        if (result) {
          results.push({
            neonAnimation: neonAnimation,
            config: config,
            animation: result,
          });
        }
      }

      return results;
    },

    _shouldComplete: function(activeEntries) {
      var finished = true;
      for (var i = 0; i < activeEntries.length; i++) {
        if (activeEntries[i].animation.playState != 'finished') {
          finished = false;
          break;
        }
      }
      return finished;
    },

    _complete: function(activeEntries) {
      for (var i = 0; i < activeEntries.length; i++) {
        activeEntries[i].neonAnimation.complete(activeEntries[i].config);
      }
      for (var i = 0; i < activeEntries.length; i++) {
        activeEntries[i].animation.cancel();
      }
    },

    /**
     * Plays an animation with an optional `type`.
     * @param {string=} type
     * @param {!Object=} cookie
     */
    playAnimation: function(type, cookie) {
      var configs = this.getAnimationConfig(type);
      if (!configs) {
        return;
      }
      this._active = this._active || {};
      if (this._active[type]) {
        this._complete(this._active[type]);
        delete this._active[type];
      }

      var activeEntries = this._configureAnimations(configs);

      if (activeEntries.length == 0) {
        this.fire('neon-animation-finish', cookie, {bubbles: false});
        return;
      }

      this._active[type] = activeEntries;

      for (var i = 0; i < activeEntries.length; i++) {
        activeEntries[i].animation.onfinish = function() {
          if (this._shouldComplete(activeEntries)) {
            this._complete(activeEntries);
            delete this._active[type];
            this.fire('neon-animation-finish', cookie, {bubbles: false});
          }
        }.bind(this);
      }
    },

    /**
     * Cancels the currently running animations.
     */
    cancelAnimation: function() {
      for (var k in this._active) {
        var entries = this._active[k]

        for (var j in entries) {
          entries[j].animation.cancel();
        }
      }

      this._active = {};
    }
  };

  /** @polymerBehavior Polymer.NeonAnimationRunnerBehavior */
  Polymer.NeonAnimationRunnerBehavior = [
    Polymer.NeonAnimatableBehavior,
    Polymer.NeonAnimationRunnerBehaviorImpl
  ];