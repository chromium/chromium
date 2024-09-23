(class CSSHelper{
  constructor(testRunner, dp) {
    this._testRunner = testRunner;
    this._dp = dp;
  }

  _trimErrorMessage(error) {
    return error.replace(/at position \d+/, "<somewhere>");
  }

  async _logMessage(message, expectError, styleSheetId) {
    if (message.error && expectError) {
      this._testRunner.log('Expected protocol error: ' + message.error.message +
          (message.error.data ? ' (' + this._trimErrorMessage(message.error.data) + ')' : ''));
    } else if (message.error && !expectError) {
      this._testRunner.log('ERROR: ' + message.error.message);
    } else if (!message.error && expectError) {
      this._testRunner.die(
          'ERROR: protocol method call did not return expected error. Instead, the following message was received: ' +
          JSON.stringify(message));
    } else if (!message.error && !expectError) {
      var {result} =
          await this._dp.CSS.getStyleSheetText({styleSheetId: styleSheetId});
      this._testRunner.log('==== Style sheet text ====');
      this._testRunner.log(result.text);
    }
  }

  async setPropertyText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setPropertyText(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setRuleSelector(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setRuleSelector(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setMediaText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setMediaText(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setContainerQueryText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setContainerQueryText(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setSupportsText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setSupportsText(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setScopeText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setScopeText(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async addRule(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.addRule(options);
    await this._logMessage(message, expectError, styleSheetId);
  }

  async setStyleTexts(styleSheetId, expectError, edits) {
    var message = await this._dp.CSS.setStyleTexts({edits: edits});
    await this._logMessage(message, expectError, styleSheetId);
  }

  _indentLog(indent, string) {
    var indentString = Array(indent + 1).join(' ');
    this._testRunner.log(indentString + string);
  }

  dumpRuleMatch(ruleMatch) {
    var rule = ruleMatch.rule;
    var matchingSelectors = ruleMatch.matchingSelectors;
    var media = rule.media || [];
    var mediaLine = '';
    for (var i = 0; i < media.length; ++i)
      mediaLine += (i > 0 ? ' ' : '') + media[i].text;
    var baseIndent = 0;
    if (mediaLine.length) {
      this._indentLog(baseIndent, '@media ' + mediaLine);
      baseIndent += 4;
    }

    const containerQueries = rule.containerQueries || [];
    const containerQueriesLine = containerQueries.map(cq => {
      if (cq.name) {
        return `${cq.name} ${cq.text}`;
      }
      return cq.text;
    }).join(' ');
    if (containerQueriesLine.length) {
      this._indentLog(baseIndent, '@container ' + containerQueriesLine);
      baseIndent += 4;
    }

    const supports = rule.supports || [];
    const supportsLine = supports.map(s => s.text).join(' ');
    if (supportsLine.length) {
      this._indentLog(baseIndent, '@supports ' + supportsLine);
      baseIndent += 4;
    }

    const layers = rule.layers|| [];
    const layersLine = layers.map(s => s.text).join('.');
    if (layersLine.length) {
      this._indentLog(baseIndent, '@layer ' + layersLine);
      baseIndent += 4;
    }

    const scopes = rule.scopes || [];
    const scopesLine = scopes.map(s => s.text).join(' ');
    if (scopesLine.length) {
      this._indentLog(baseIndent, '@scope ' + scopesLine);
      baseIndent += 4;
    }

    var selectorLine = '';
    var selectors = rule.selectorList.selectors;
    for (var i = 0; i < selectors.length; ++i) {
      if (i > 0)
        selectorLine += ', ';
      var matching = matchingSelectors.indexOf(i) !== -1;
      if (matching)
        selectorLine += '*';
      selectorLine += selectors[i].text;
      if (matching)
        selectorLine += '*';
    }
    selectorLine += ' {';
    selectorLine += '    ' + rule.origin;
    if (!rule.style.styleSheetId)
      selectorLine += '    readonly';
    this._indentLog(baseIndent, selectorLine);
    this.dumpStyle(rule.style, baseIndent);
    this._indentLog(baseIndent, '}');
  }

  dumpStyle(style, baseIndent) {
    if (!style)
      return;
    var cssProperties = style.cssProperties;
    for (var i = 0; i < cssProperties.length; ++i) {
      var cssProperty = cssProperties[i];
      var range = cssProperty.range;
      var rangeText = range ? '[' + range.startLine + ':' + range.startColumn +
                                  '-' + range.endLine + ':' + range.endColumn + ']'
                            : '[undefined-undefined]';
      var propertyLine = cssProperty.name + ': ' + cssProperty.value + '; @' + rangeText;
      this._indentLog(baseIndent + 4, propertyLine);
    }
  }

  displayName(url) {
    return url.substr(url.lastIndexOf('/') + 1);
  }

  async loadAndDumpMatchingRulesForNode(nodeId, omitLog) {
    var {result} = await this._dp.CSS.getMatchedStylesForNode({'nodeId': nodeId});
    if (!omitLog)
      this._testRunner.log('Dumping matched rules: ');
    dumpRuleMatches.call(this, result.matchedCSSRules);
    if (!omitLog)
      this._testRunner.log('Dumping inherited rules: ');
    for (var inheritedEntry of result.inherited) {
      this.dumpStyle(inheritedEntry.inlineStyle, /*indent=*/0);
      dumpRuleMatches.call(this, inheritedEntry.matchedCSSRules);
    }

    function dumpRuleMatches(ruleMatches) {
      for (var ruleMatch of ruleMatches) {
        var origin = ruleMatch.rule.origin;
        if (origin !== 'inspector' && origin !== 'regular')
          continue;
        this.dumpRuleMatch(ruleMatch);
      }
    }
  }

  async loadAndDumpCSSPositionTryForNode(nodeId) {
    const {result} =
        await this._dp.CSS.getMatchedStylesForNode({'nodeId': nodeId});
    this._testRunner.log('Dumping CSS position-try rules: ');
    for (const cssPositionTryRule of result.cssPositionTryRules) {
      const status = Boolean(cssPositionTryRule.active) ? 'active' : 'inactive';
      this._testRunner.log(`@position-try ${cssPositionTryRule.name.text} (${status}) {`);
      this.dumpStyle(cssPositionTryRule.style, 0);
      this._testRunner.log('}');
    }
    this._testRunner.log('index of active position-try-fallback: ' + result.activePositionFallbackIndex);
  }

  async loadAndDumpCSSAnimationsForNode(nodeId) {
    var {result} =
        await this._dp.CSS.getMatchedStylesForNode({'nodeId': nodeId});
    this._testRunner.log('Dumping CSS keyframed animations: ');
    for (var keyframesRule of result.cssKeyframesRules) {
      this._testRunner.log(
          '@keyframes ' + keyframesRule.animationName.text + ' {');
      for (var keyframe of keyframesRule.keyframes) {
        this._indentLog(4, keyframe.keyText.text + ' {');
        this.dumpStyle(keyframe.style, 4);
        this._indentLog(4, '}');
      }
      this._testRunner.log('}');
    }
  }

  async loadAndDumpMatchingRules(documentNodeId, selector, omitLog) {
    var nodeId = await this.requestNodeId(documentNodeId, selector);
    await this.loadAndDumpMatchingRulesForNode(nodeId, omitLog);
  }

  async loadAndDumpInlineAndMatchingRules(documentNodeId, selector, omitLog) {
    var nodeId = await this.requestNodeId(documentNodeId, selector);
    var {result} =
        await this._dp.CSS.getInlineStylesForNode({'nodeId': nodeId});
    if (!omitLog)
      this._testRunner.log('Dumping inline style: ');
    this._testRunner.log('{');
    this.dumpStyle(result.inlineStyle, 0);
    this._testRunner.log('}');
    await this.loadAndDumpMatchingRulesForNode(nodeId, omitLog)
  }

  async requestDocumentNodeId() {
    var {result} = await this._dp.DOM.getDocument({});
    return result.root.nodeId;
  }

  async requestNodeId(nodeId, selector) {
    var response = await this._dp.DOM.querySelector({nodeId, selector});
    return response.result.nodeId;
  }

  async requestAllNodeIds(nodeId, selector) {
    var response = await this._dp.DOM.querySelectorAll({nodeId, selector});
    return response.result.nodeIds;
  }
});
