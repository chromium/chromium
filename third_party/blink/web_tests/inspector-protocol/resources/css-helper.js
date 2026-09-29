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

  async setContainerQueryConditionText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setContainerQueryConditionText(options);
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

  async setNavigationText(styleSheetId, expectError, options) {
    options.styleSheetId = styleSheetId;
    var message = await this._dp.CSS.setNavigationText(options);
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

    const startingStyles = rule.startingStyles || [];
    if (startingStyles.length) {
      this._indentLog(baseIndent, '@starting-style');
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

  rangeText(range) {
    if (!range) {
      return '[undefined-undefined]';
    }
    return `[${range.startLine}:${range.startColumn}-${range.endLine}:${
        range.endColumn}]`;
  }

  dumpRulesArray(rules, currentIndent = '') {
    if (!rules) {
      return;
    }
    for (let i = 0; i < rules.length; ++i) {
      this.dumpRule(rules[i], currentIndent);
    }
  }

  dumpRuleMatchesArray(matches, currentIndent = '') {
    if (!matches) {
      return;
    }
    for (let i = 0; i < matches.length; ++i) {
      this.dumpRule(matches[i].rule, currentIndent);
    }
  }

  dumpRule(rule, currentIndent = '') {
    function selectorRange() {
      const selectors = rule.selectorList.selectors;
      if (!selectors || !selectors[0].range) {
        return '';
      }
      const ranges = [];
      for (let i = 0; i < selectors.length; ++i) {
        const range = selectors[i].range;
        ranges.push(`${range.startLine}:${range.startColumn}-${range.endLine}:${
            range.endColumn}`);
      }
      return ', ' + ranges.join('; ');
    }

    if (!rule.type || rule.type === 'style') {
      this._testRunner.log(`${currentIndent}${rule.selectorList.text}: [${
          rule.origin}${selectorRange()}] {`);
      this.dumpStyleWithRanges(rule.style, currentIndent + '    ');
      this._testRunner.log(`${currentIndent}}`);
      return;
    }

    if (rule.type === 'media') {
      this._testRunner.log(`${currentIndent}@media ${rule.mediaText} {`);
      this.dumpRulesArray(rule.childRules, currentIndent + '    ');
      this._testRunner.log(`${currentIndent}}`);
    }
  }

  dumpStyleWithRanges(style, currentIndent = '') {
    if (!style) {
      this._testRunner.log(currentIndent + '[NO STYLE]');
      return;
    }
    for (let i = 0; i < style.cssProperties.length; ++i) {
      const property = style.cssProperties[i];
      if (!property.disabled) {
        this._testRunner.log(`${currentIndent}['${property.name}':'${
            property.value}'${property.important ? ' is-important' : ''}${
            ('parsedOk' in property) ?
                ' non-parsed' :
                ''}] @${this.rangeText(property.range)} `);
      } else {
        this._testRunner.log(
            `${currentIndent}[text='${property.text}'] disabled`);
      }
    }
  }

  async dumpSelectedNodeStyles(nodeId, omitLonghands = false,
                               styleSheetHeaders = new Map()) {
    const {result} = await this._dp.CSS.getMatchedStylesForNode({nodeId});
    const activeEffectiveProps = new Set();

    function trimMiddle(str, maxLength = 23) {
      if (str.length <= maxLength) {
        return str;
      }
      const half = Math.floor((maxLength - 1) / 2);
      return str.slice(0, half) + '…' +
          str.slice(str.length - (maxLength - 1 - half));
    }

    function formatRuleSubtitle(rule) {
      if (rule.origin === 'user-agent') {
        return 'user agent stylesheet';
      }
      const header = styleSheetHeaders.get(rule.style.styleSheetId);
      if (!header) {
        return '<style>';
      }
      if (!header.hasSourceURL &&
          (!header.sourceURL ||
           !header.isInline && header.sourceURL.endsWith('.html'))) {
        return '<style>';
      }
      const url = header.sourceURL;
      const fileName = url.startsWith('data:') ?
          url :
          url.substring(url.lastIndexOf('/') + 1);
      if (!fileName) {
        return '<style>';
      }
      const relLine = rule.style.range ? rule.style.range.startLine : 0;
      const relCol = rule.style.range ? rule.style.range.startColumn : 0;
      const startLine =
          (header.hasSourceURL ? 0 : (header.startLine || 0)) + relLine + 1;
      const startCol =
          (!header.hasSourceURL && relLine === 0 ? (header.startColumn || 0) :
                                                   0) +
          relCol + 1;
      const shortDisplay = trimMiddle(`${fileName}:${startLine}`, 23);
      return `${shortDisplay} -> ${fileName}:${startLine}:${startCol}`;
    }

    function cleanUserAgentPayload(payload) {
      for (const ruleMatch of payload) {
        const {matchingSelectors, rule} = ruleMatch;
        if (rule.origin === 'user-agent' && matchingSelectors.length) {
          rule.selectorList.selectors = rule.selectorList.selectors.filter(
              (_, i) => matchingSelectors.includes(i));
          rule.selectorList.text =
              rule.selectorList.selectors.map(item => item.text).join(', ');
          ruleMatch.matchingSelectors = matchingSelectors.map((_, i) => i);
        }
      }
      const cleanMatchedPayload = [];
      for (const ruleMatch of payload) {
        const lastMatch = cleanMatchedPayload[cleanMatchedPayload.length - 1];
        const mediaText = rm =>
            rm.rule.media ? rm.rule.media.map(m => m.text).join(', ') : null;
        if (!lastMatch || ruleMatch.rule.origin !== 'user-agent' ||
            lastMatch.rule.origin !== 'user-agent' ||
            ruleMatch.rule.selectorList.text !==
                lastMatch.rule.selectorList.text ||
            mediaText(ruleMatch) !== mediaText(lastMatch)) {
          cleanMatchedPayload.push(ruleMatch);
          continue;
        }
        const shorthands = new Map();
        const properties = new Map();
        for (const entry of lastMatch.rule.style.shorthandEntries) {
          shorthands.set(entry.name, entry.value);
        }
        for (const entry of lastMatch.rule.style.cssProperties) {
          properties.set(entry.name, entry.value);
        }
        for (const entry of ruleMatch.rule.style.shorthandEntries) {
          shorthands.set(entry.name, entry.value);
        }
        for (const entry of ruleMatch.rule.style.cssProperties) {
          properties.set(entry.name, entry.value);
        }
        lastMatch.rule.style.shorthandEntries =
            [...shorthands.entries()].map(([name, value]) => ({name, value}));
        lastMatch.rule.style.cssProperties =
            [...properties.entries()].map(([name, value]) => ({name, value}));
      }
      return cleanMatchedPayload;
    }

    function markWinningProperties(style) {
      if (!style || !style.cssProperties) {
        return [];
      }
      const statuses = [];
      const localLastIndex = new Map();
      for (let i = 0; i < style.cssProperties.length; ++i) {
        const p = style.cssProperties[i];
        if (p.disabled || p.parsedOk === false) {
          statuses.push('overloaded');
          continue;
        }
        if (p.range) {
          localLastIndex.set(p.name, i);
        }
      }
      for (let i = 0; i < style.cssProperties.length; ++i) {
        const p = style.cssProperties[i];
        if (p.disabled || p.parsedOk === false) {
          continue;
        }
        if (p.range && localLastIndex.get(p.name) !== i) {
          statuses[i] = 'overloaded';
          continue;
        }
        if (activeEffectiveProps.has(p.name)) {
          statuses[i] = 'overloaded';
        } else {
          statuses[i] = 'active';
          activeEffectiveProps.add(p.name);
        }
      }
      return statuses;
    }

    const dumpSectionStyle = (style, statuses) => {
      if (!style || !style.cssProperties) {
        return;
      }
      const longhandsByShorthand = new Map();
      for (const p of style.cssProperties) {
        if (!p.range && p.name) {
          for (const sp of style.cssProperties) {
            if (sp.range && !sp.disabled && sp.parsedOk !== false &&
                (p.name.startsWith(sp.name + '-') ||
                 (sp.name === 'background' &&
                  p.name.startsWith('background-')) ||
                 (sp.name === 'animation' &&
                  p.name.startsWith('animation-')))) {
              if (!longhandsByShorthand.has(sp.name)) {
                longhandsByShorthand.set(sp.name, []);
              }
              longhandsByShorthand.get(sp.name).push(p);
            }
          }
        }
      }

      for (let i = 0; i < style.cssProperties.length; ++i) {
        const p = style.cssProperties[i];
        if (!p.range && style.styleSheetId) {
          continue;
        }
        let prefix = '';
        if (p.disabled) {
          prefix = '/-- overloaded --/ /-- disabled --/ ';
        } else if (p.parsedOk === false || statuses[i] === 'overloaded') {
          prefix = '/-- overloaded --/ ';
        }
        const val = p.value +
            (p.important && !p.value.includes('!important') ? ' !important' :
                                                              '');
        const propText = `${p.name}: ${val};`;
        const rawText = p.disabled ? `/* ${propText} */` : propText;
        this._testRunner.log(`${prefix}    ${rawText}`);
        if (!omitLonghands && longhandsByShorthand.has(p.name)) {
          for (const lh of longhandsByShorthand.get(p.name)) {
            const lhVal =
                (lh.value && p.value !== 'inherit') ? ` ${lh.value}` : ' ';
            this._testRunner.log(`        ${lh.name}:${lhVal};`);
          }
        }
      }
    };

    const inlineStatuses = markWinningProperties(result.inlineStyle);
    this._testRunner.log('[expanded] ');
    this._testRunner.log('element.style { ()');
    dumpSectionStyle(result.inlineStyle, inlineStatuses);
    this._testRunner.log('');

    const matches =
        cleanUserAgentPayload(result.matchedCSSRules || []).slice().reverse();
    for (const match of matches) {
      const rule = match.rule;
      const statuses = markWinningProperties(rule.style);
      this._testRunner.log('[expanded] ');
      const selectorText =
          rule.selectorList.selectors.map(s => s.text).join(', ');
      const subtitle = formatRuleSubtitle(rule);
      this._testRunner.log(`${selectorText} { (${subtitle})`);
      dumpSectionStyle(rule.style, statuses);
      this._testRunner.log('');
    }

    for (const kfRule of result.cssKeyframesRules || []) {
      if (kfRule.animationName && kfRule.origin !== 'user-agent') {
        this._testRunner.log(
            `======== @keyframes ${kfRule.animationName.text} ========`);
        for (const kf of kfRule.keyframes) {
          this._testRunner.log('[expanded] ');
          const keyText = kf.keyText.text === 'from' ?
              '0%' :
              (kf.keyText.text === 'to' ? '100%' : kf.keyText.text);
          this._testRunner.log(`${keyText} { (<style>)`);
          const kfStatuses = kf.style.cssProperties.map(() => 'active');
          dumpSectionStyle(kf.style, kfStatuses);
          this._testRunner.log('');
        }
      }
    }
  }
});
