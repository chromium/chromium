/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.xpc.CrossPageChannelTest');
goog.setTestOnly('goog.net.xpc.CrossPageChannelTest');

const CfgFields = goog.require('goog.net.xpc.CfgFields');
const ChannelStates = goog.require('goog.net.xpc.ChannelStates');
const CrossPageChannel = goog.require('goog.net.xpc.CrossPageChannel');
const CrossPageChannelRole = goog.require('goog.net.xpc.CrossPageChannelRole');
const Disposable = goog.require('goog.Disposable');
const GoogPromise = goog.require('goog.Promise');
const Level = goog.require('goog.log.Level');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Resolver = goog.require('goog.promise.Resolver');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const TransportTypes = goog.require('goog.net.xpc.TransportTypes');
const Uri = goog.require('goog.Uri');
const browser = goog.require('goog.labs.userAgent.browser');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const log = goog.require('goog.log');
const object = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const xpc = goog.require('goog.net.xpc');
/** @suppress {extraRequire} Needed for G_testRunner.log() */
goog.require('goog.testing.jsunit');


// Set this to false when working on this test.  It needs to be true for
// automated testing, as some browsers (eg IE8) choke on the large numbers of
// iframes this test would otherwise leave active.
/** @const */
const CLEAN_UP_IFRAMES = true;

/** @const */
const IFRAME_LOAD_WAIT_MS = 1000;
const stubs = new PropertyReplacer();
let uniqueId = 0;
let driver;
let accessCheckPromise = null;

testSuite({

  setUpPage() {
    // This test is insanely slow on IE8 and Safari for some reason.
    TestCase.getActiveTestCase().promiseTimeout = 40 * 1000;

    // Show debug log
    const debugDiv = dom.getElement('debugDiv');
    const logger = log.getLogger('goog.net.xpc');
    log.setLevel(logger, Level.ALL);
    log.addHandler(logger, function(logRecord) {
      const msgElm = dom.createDom(TagName.DIV);
      msgElm.innerHTML = logRecord.getMessage();
      dom.appendChild(debugDiv, msgElm);
    });

    accessCheckPromise = new GoogPromise(function(resolve, reject) {
      const accessCheckIframes = [];

      accessCheckIframes.push(
          create1x1Iframe('nonexistent', 'testdata/i_am_non_existent.html'));
      window.setTimeout(function() {
        accessCheckIframes.push(
            create1x1Iframe('existent', 'testdata/access_checker.html'));
      }, 10);

      // Called from testdata/access_checker.html
      window['sameDomainIframeAccessComplete'] = function() {
        for (let i = 0; i < accessCheckIframes.length; i++) {
          document.body.removeChild(accessCheckIframes[i]);
        }
        resolve();
      };
    });
  },


  setUp() {
    driver = new Driver();
    // Expose driver on the window object, since inner_peer.html uses it to
    // communicate.
    window['driver'] = driver;

    // Ensure that the access check is complete before starting each test.
    return accessCheckPromise;
  },


  tearDown() {
    stubs.reset();
    driver.dispose();
  },


  testCreateIframeSpecifyId() {
    driver.createPeerIframe('new_iframe');

    return Timer.promise(IFRAME_LOAD_WAIT_MS).then(function() {
      driver.checkPeerIframe();
    });
  },


  testCreateIframeRandomId() {
    driver.createPeerIframe();

    return Timer.promise(IFRAME_LOAD_WAIT_MS).then(function() {
      driver.checkPeerIframe();
    });
  },


  testGetRole() {
    const cfg = {};
    cfg[CfgFields.ROLE] = CrossPageChannelRole.OUTER;
    const channel = new CrossPageChannel(cfg);
    // If the configured role is ignored, this will cause the dynamicly
    // determined role to become INNER.
    /** @suppress {visibility} suppression added to enable type checking */
    channel.peerWindowObject_ = window.parent;
    assertEquals(
        'Channel should use role from the config.', CrossPageChannelRole.OUTER,
        channel.getRole());
    channel.dispose();
  },


  // The following batch of tests:
  // * Establishes a peer iframe
  // * Connects an XPC channel between the frames
  // * From the connection callback in each frame, sends an 'echo' request, and
  //   expects a 'response' response.
  // * Reconnects the inner frame, sends an 'echo', expects a 'response'.
  // * Optionally, reconnects the outer frame, sends an 'echo', expects a
  //   'response'.
  // * Optionally, reconnects the inner frame, but first reconfigures it to the
  //   alternate protocol version, simulating an inner frame navigation that
  //   picks up a new/old version.
  //
  // Every valid combination of protocol versions is tested, with both single
  // and double ended handshakes.  Two timing scenarios are tested per
  // combination, which is what the 'reverse' parameter distinguishes.
  //
  // Where single sided handshake is in use, reconnection by the outer frame is
  // not supported, and therefore is not tested.
  //
  // The only known issue migrating to V2 is that once two V2 peers have
  // connected, replacing either peer with a V1 peer will not work.  Upgrading
  // V1 peers to v2 is supported, as is replacing the only v2 peer in a
  // connection with a v1.


  testLifeCycle_v1_v1() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v1_v1_rev() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v1_v1_onesided() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v1_v1_onesided_rev() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v1_v2() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v1_v2_rev() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v1_v2_onesided() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v1_v2_onesided_rev() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 1 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v2_v1() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v2_v1_rev() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v2_v1_onesided() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, false /* reverse */);
  },

  testLifeCycle_v2_v1_onesided_rev() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        1 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        true /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v2_v2() {
    // Test flakes on IE 10+ and Chrome: see b/22873770 and b/18595666.
    if ((browser.isIE() && browser.isVersionOrHigher(10)) ||
        browser.isChrome()) {
      return;
    }

    return checkLifeCycle(
        false /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        false /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v2_v2_rev() {
    return checkLifeCycle(
        false /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, true /* outerFrameReconnectSupported */,
        false /* innerFrameMigrationSupported */, true /* reverse */);
  },


  testLifeCycle_v2_v2_onesided() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        false /* innerFrameMigrationSupported */, false /* reverse */);
  },


  testLifeCycle_v2_v2_onesided_rev() {
    return checkLifeCycle(
        true /* oneSidedHandshake */, 2 /* innerProtocolVersion */,
        2 /* outerProtocolVersion */, false /* outerFrameReconnectSupported */,
        false /* innerFrameMigrationSupported */, true /* reverse */);
  },


  // testConnectMismatchedNames have been flaky on IEs.
  // Flakiness is tracked in http://b/18595666
  // For now, not running these tests on IE.

  testConnectMismatchedNames_v1_v1() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        1 /* innerProtocolVersion */, 1 /* outerProtocolVersion */,
        false /* reverse */);
  },


  testConnectMismatchedNames_v1_v1_rev() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        1 /* innerProtocolVersion */, 1 /* outerProtocolVersion */,
        true /* reverse */);
  },


  testConnectMismatchedNames_v1_v2() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        1 /* innerProtocolVersion */, 2 /* outerProtocolVersion */,
        false /* reverse */);
  },


  testConnectMismatchedNames_v1_v2_rev() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        1 /* innerProtocolVersion */, 2 /* outerProtocolVersion */,
        true /* reverse */);
  },


  testConnectMismatchedNames_v2_v1() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        2 /* innerProtocolVersion */, 1 /* outerProtocolVersion */,
        false /* reverse */);
  },


  testConnectMismatchedNames_v2_v1_rev() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        2 /* innerProtocolVersion */, 1 /* outerProtocolVersion */,
        true /* reverse */);
  },


  testConnectMismatchedNames_v2_v2() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        2 /* innerProtocolVersion */, 2 /* outerProtocolVersion */,
        false /* reverse */);
  },


  testConnectMismatchedNames_v2_v2_rev() {
    if (browser.isIE()) {
      return;
    }

    return checkConnectMismatchedNames(
        2 /* innerProtocolVersion */, 2 /* outerProtocolVersion */,
        true /* reverse */);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEscapeServiceName() {
    /** @suppress {visibility} suppression added to enable type checking */
    const escape = CrossPageChannel.prototype.escapeServiceName_;
    assertEquals(
        'Shouldn\'t escape alphanumeric name', 'fooBar123',
        escape('fooBar123'));
    assertEquals(
        'Shouldn\'t escape most non-alphanumeric characters',
        '`~!@#$^&*()_-=+ []{}\'";,<.>/?\\',
        escape('`~!@#$^&*()_-=+ []{}\'";,<.>/?\\'));
    assertEquals(
        'Should escape %, |, and :', 'foo%3ABar%7C123%25',
        escape('foo:Bar|123%'));
    assertEquals('Should escape tp', '%25tp', escape('tp'));
    assertEquals('Should escape %tp', '%25%25tp', escape('%tp'));
    assertEquals('Should not escape stp', 'stp', escape('stp'));
    assertEquals('Should not escape s%tp', 's%25tp', escape('s%tp'));
  },


  testSameDomainCheck_noMessageOrigin() {
    const channel = new CrossPageChannel(
        object.create(CfgFields.PEER_HOSTNAME, 'http://foo.com'));
    assertTrue(channel.isMessageOriginAcceptable(undefined));
  },


  testSameDomainCheck_noPeerHostname() {
    const channel = new CrossPageChannel({});
    assertTrue(channel.isMessageOriginAcceptable('http://foo.com'));
  },


  testSameDomainCheck_unconfigured() {
    const channel = new CrossPageChannel({});
    assertTrue(channel.isMessageOriginAcceptable(undefined));
  },


  testSameDomainCheck_originsMatch() {
    const channel = new CrossPageChannel(
        object.create(CfgFields.PEER_HOSTNAME, 'http://foo.com'));
    assertTrue(channel.isMessageOriginAcceptable('http://foo.com'));
  },


  testSameDomainCheck_originsMismatch() {
    const channel = new CrossPageChannel(
        object.create(CfgFields.PEER_HOSTNAME, 'http://foo.com'));
    assertFalse(channel.isMessageOriginAcceptable('http://nasty.com'));
  },


  /** @suppress {checkTypes} suppression added to enable type checking */
  testUnescapeServiceName() {
    /** @suppress {visibility} suppression added to enable type checking */
    const unescape = CrossPageChannel.prototype.unescapeServiceName_;
    assertEquals(
        'Shouldn\'t modify alphanumeric name', 'fooBar123',
        unescape('fooBar123'));
    assertEquals(
        'Shouldn\'t modify most non-alphanumeric characters',
        '`~!@#$^&*()_-=+ []{}\'";,<.>/?\\',
        unescape('`~!@#$^&*()_-=+ []{}\'";,<.>/?\\'));
    assertEquals(
        'Should unescape URL-escapes', 'foo:Bar|123%',
        unescape('foo%3ABar%7C123%25'));
    assertEquals('Should unescape tp', 'tp', unescape('%25tp'));
    assertEquals('Should unescape %tp', '%tp', unescape('%25%25tp'));
    assertEquals('Should not escape stp', 'stp', unescape('stp'));
    assertEquals('Should not escape s%tp', 's%tp', unescape('s%25tp'));
  },


  async testDisposeImmediate() {
    // Given
    driver.createPeerIframe(
        'new_iframe',
        /* oneSidedHandshake= */ false,
        /* innerProtocolVersion= */ 2,
        /* outerProtocolVersion= */ 2,
        /* opt_randomChannelNames= */ true);

    assertEquals(driver.getChannel().state_, ChannelStates.NOT_CONNECTED);
    assertNull(driver.getChannel().transport_);

    // When
    driver.getChannel().dispose();

    // Then
    assertTrue(driver.getChannel().isDisposed());
    // Let any errors caused by erroneous retries happen.
    await Timer.promise(2000);
  },

  async testDisposeBeforePeerNotification() {
    // Given
    driver.createPeerIframe(
        'new_iframe',
        /* oneSidedHandshake= */ false,
        /* innerProtocolVersion= */ 2,
        /* outerProtocolVersion= */ 2,
        /* opt_randomChannelNames= */ true);

    await driver.connectAndWaitForPeer();

    assertEquals(driver.getChannel().state_, ChannelStates.NOT_CONNECTED);
    const transport = driver.getChannel().transport_;

    // When
    driver.getChannel().dispose();

    // Then
    assertNull(driver.getChannel().transport_);
    assertTrue(driver.getChannel().isDisposed());
    assertTrue(transport.isDisposed());
    // Let any errors caused by erroneous retries happen.
    await Timer.promise(2000);
  },



});


/**
 * @param {string} iframeId
 * @param {string} src
 * @return {!HTMLIFrameElement}
 */
function create1x1Iframe(iframeId, src) {
  const iframeAccessChecker = dom.createElement(TagName.IFRAME);
  iframeAccessChecker.id = iframeAccessChecker.name = iframeId;
  iframeAccessChecker.style.width = iframeAccessChecker.style.height = '1px';
  iframeAccessChecker.src = src;
  document.body.insertBefore(iframeAccessChecker, document.body.firstChild);
  return iframeAccessChecker;
}

/**
 * @param {boolean} oneSidedHandshake,
 * @param {number} innerProtocolVersion
 * @param {number} outerProtocolVersion
 * @param {boolean} outerFrameReconnectSupported
 * @param {boolean} innerFrameMigrationSupported
 * @param {boolean} reverse
 * @return {!GoogPromise<undefined>}
 */
function checkLifeCycle(
    oneSidedHandshake, innerProtocolVersion, outerProtocolVersion,
    outerFrameReconnectSupported, innerFrameMigrationSupported, reverse) {
  driver.createPeerIframe(
      'new_iframe', oneSidedHandshake, innerProtocolVersion,
      outerProtocolVersion);
  return driver.connect(
      true /* fullLifeCycleTest */, outerFrameReconnectSupported,
      innerFrameMigrationSupported, reverse);
}

/**
 * @param {number} innerProtocolVersion
 * @param {number} outerProtocolVersion
 * @param {boolean} reverse
 * @return {!GoogPromise<undefined>}
 */
function checkConnectMismatchedNames(
    innerProtocolVersion, outerProtocolVersion, reverse) {
  driver.createPeerIframe(
      'new_iframe', false /* oneSidedHandshake */, innerProtocolVersion,
      outerProtocolVersion, true /* opt_randomChannelNames */);
  return driver.connect(
      false /* fullLifeCycleTest */, false /* outerFrameReconnectSupported */,
      false /* innerFrameMigrationSupported */, reverse /* reverse */);
}



/**
 * Driver for the tests for CrossPageChannel.
 * @unrestricted
 */
const Driver = class extends Disposable {
  constructor() {
    super();

    /**
     * The peer iframe.
     * @type {!Element}
     * @private
     * @suppress {checkTypes} suppression added to enable type checking
     */
    this.iframe_ = null;

    /**
     * The channel to use.
     * @type {?CrossPageChannel}
     * @private
     */
    this.channel_ = null;

    /**
     * Outer frame configuration object.
     * @type {?Object}
     * @private
     */
    this.outerFrameCfg_ = null;

    /**
     * The initial name of the outer channel.
     * @type {?string}
     * @private
     */
    this.initialOuterChannelName_ = null;

    /**
     * Inner frame configuration object.
     * @type {?Object}
     * @private
     */
    this.innerFrameCfg_ = null;

    /**
     * The contents of the payload of the 'echo' request sent by the inner
     * frame.
     * @type {?string}
     * @private
     */
    this.innerFrameEchoPayload_ = null;

    /**
     * The contents of the payload of the 'echo' request sent by the outer
     * frame.
     * @type {?string}
     * @private
     */
    this.outerFrameEchoPayload_ = null;

    /**
     * A resolver which fires its promise when the inner frame receives an echo.
     * @type {!Resolver}
     * @private
     */
    this.innerFrameResponseReceived_ = GoogPromise.withResolver();

    /**
     * A resolver which fires its promise when the outer frame receives an echo.
     * @type {!Resolver}
     * @private
     */
    this.outerFrameResponseReceived_ = GoogPromise.withResolver();
  }

  /** @override */
  disposeInternal() {
    // Required to make this test perform acceptably (and pass) on slow
    // browsers, esp IE8.
    if (CLEAN_UP_IFRAMES) {
      dom.removeNode(this.iframe_);
      delete this.iframe_;
    }
    dispose(this.channel_);
    this.innerFrameResponseReceived_.promise.cancel();
    this.outerFrameResponseReceived_.promise.cancel();
    super.disposeInternal();
  }

  /**
   * Returns the child peer's window object.
   * @return {!Window} Child peer's window.
   * @private
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  getInnerPeer_() {
    return this.iframe_.contentWindow;
  }

  /**
   * Sets up the configuration objects for the inner and outer frames.
   * @param {string=} opt_iframeId If present, the ID of the iframe to use,
   *     otherwise, tells the channel to generate an iframe ID.
   * @param {boolean=} opt_oneSidedHandshake Whether the one sided handshake
   *     config option should be set.
   * @param {string=} opt_channelName The name of the channel to use, or null
   *     to generate one.
   * @param {number=} opt_innerProtocolVersion The native transport protocol
   *     version used in the inner iframe.
   * @param {number=} opt_outerProtocolVersion The native transport protocol
   *     version used in the outer iframe.
   * @param {boolean=} opt_randomChannelNames Whether the different ends of the
   *     channel should be allowed to pick differing, random names.
   * @return {string} The name of the created channel.
   * @private
   * @suppress {missingReturn} suppression added to enable type checking
   */
  setConfiguration_(
      opt_iframeId, opt_oneSidedHandshake, opt_channelName,
      opt_innerProtocolVersion, opt_outerProtocolVersion,
      opt_randomChannelNames) {
    const cfg = {};
    if (opt_iframeId) {
      cfg[CfgFields.IFRAME_ID] = opt_iframeId;
    }
    cfg[CfgFields.PEER_URI] = 'testdata/inner_peer.html';
    if (!opt_randomChannelNames) {
      const channelName = opt_channelName || 'test_channel' + uniqueId++;
      cfg[CfgFields.CHANNEL_NAME] = channelName;
    }
    cfg[CfgFields.LOCAL_POLL_URI] = 'does-not-exist.html';
    cfg[CfgFields.PEER_POLL_URI] = 'does-not-exist.html';
    cfg[CfgFields.ONE_SIDED_HANDSHAKE] = !!opt_oneSidedHandshake;
    cfg[CfgFields.NATIVE_TRANSPORT_PROTOCOL_VERSION] = opt_outerProtocolVersion;
    function resolveUri(fieldName) {
      cfg[fieldName] =
          Uri.resolve(window.location.href, cfg[fieldName]).toString();
    }
    resolveUri(CfgFields.PEER_URI);
    resolveUri(CfgFields.LOCAL_POLL_URI);
    resolveUri(CfgFields.PEER_POLL_URI);
    this.outerFrameCfg_ = cfg;
    this.innerFrameCfg_ = object.clone(cfg);
    this.innerFrameCfg_[CfgFields.NATIVE_TRANSPORT_PROTOCOL_VERSION] =
        opt_innerProtocolVersion;
  }

  /**
   * Creates an outer frame channel object.
   * @return {string}
   * @private
   * @suppress {checkTypes} suppression added to enable type checking
   */
  createChannel_() {
    if (this.channel_) {
      this.channel_.dispose();
    }
    this.channel_ = new CrossPageChannel(this.outerFrameCfg_);
    this.channel_.registerService('echo', goog.bind(this.echoHandler_, this));
    this.channel_.registerService(
        'response', goog.bind(this.responseHandler_, this));

    return this.channel_.name;
  }

  /**
   * Checks the names of the inner and outer frames meet expectations.
   * @private
   * @suppress {undefinedVars} suppression added to enable type checking
   */
  checkChannelNames_() {
    const outerName = this.channel_.name;
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const innerName = this.getInnerPeer_().channel.name;
    const configName = this.innerFrameCfg_[CfgFields.CHANNEL_NAME] || null;

    // The outer channel never changes its name.
    assertEquals(this.initialOuterChannelName_, outerName);
    // The name should be as configured, if it was configured.
    if (configName) {
      assertEquals(configName, innerName);
    }
    // The names of both ends of the channel should match.
    assertEquals(innerName, outerName);
    G_testRunner.log('Channel name: ' + innerName);
  }

  /**
   * Returns the configuration of the xpc.
   * @return {?Object} The configuration of the xpc.
   */
  getInnerFrameConfiguration() {
    return this.innerFrameCfg_;
  }

  /**
   * Creates the peer iframe.
   * @param {string=} opt_iframeId If present, the ID of the iframe to create,
   *     otherwise, generates an iframe ID.
   * @param {boolean=} opt_oneSidedHandshake Whether a one sided handshake is
   *     specified.
   * @param {number=} opt_innerProtocolVersion The native transport protocol
   *     version used in the inner iframe.
   * @param {number=} opt_outerProtocolVersion The native transport protocol
   *     version used in the outer iframe.
   * @param {boolean=} opt_randomChannelNames Whether the ends of the channel
   *     should be allowed to pick differing, random names.
   * @return {!Array<string>} The id of the created iframe and the name of the
   *     created channel.
   * @suppress {missingReturn} suppression added to enable type checking
   */
  createPeerIframe(
      opt_iframeId, opt_oneSidedHandshake, opt_innerProtocolVersion,
      opt_outerProtocolVersion, opt_randomChannelNames) {
    let expectedIframeId;

    if (opt_iframeId) {
      expectedIframeId = opt_iframeId = opt_iframeId + uniqueId++;
    } else {
      // Have createPeerIframe() generate an ID
      stubs.set(xpc, 'getRandomString', function(length) {
        return '' + length;
      });
      expectedIframeId = 'xpcpeer4';
    }
    assertNull(
        'element[id=' + expectedIframeId + '] exists',
        dom.getElement(expectedIframeId));

    this.setConfiguration_(
        opt_iframeId, opt_oneSidedHandshake, undefined /* opt_channelName */,
        opt_innerProtocolVersion, opt_outerProtocolVersion,
        opt_randomChannelNames);
    const channelName = this.createChannel_();
    this.initialOuterChannelName_ = channelName;
    this.iframe_ = this.channel_.createPeerIframe(document.body);

    assertEquals(expectedIframeId, this.iframe_.id);
  }

  /**
   * Checks if the peer iframe has been created.
   */
  checkPeerIframe() {
    assertNotNull(this.iframe_);
    const peer = this.getInnerPeer_();
    assertNotNull(peer);
    assertNotNull(peer.document);
  }

  /**
   * Starts the connection. The connection happens asynchronously.
   * @param {boolean} fullLifeCycleTest
   * @param {boolean} outerFrameReconnectSupported
   * @param {boolean} innerFrameMigrationSupported
   * @param {boolean} reverse
   * @return {!GoogPromise<undefined>}
   * @suppress {checkTypes} suppression added to enable type checking
   */
  connect(
      fullLifeCycleTest, outerFrameReconnectSupported,
      innerFrameMigrationSupported, reverse) {
    if (!this.isTransportTestable_()) {
      return;
    }

    // Set the criteria for the initial handshake portion of the test.
    this.reinitializePromises_();

    this.innerFrameResponseReceived_.promise.then(
        this.checkChannelNames_, null, this);

    if (fullLifeCycleTest) {
      this.innerFrameResponseReceived_.promise.then(goog.bind(
          this.testReconnects_, this, outerFrameReconnectSupported,
          innerFrameMigrationSupported));
    }

    this.continueConnect_(reverse);
    return this.innerFrameResponseReceived_.promise;
  }

  /**
   * @param {boolean} reverse
   * @private
   * @suppress {missingProperties} suppression added to enable type checking
   */
  continueConnect_(reverse) {
    // Wait until the peer is fully established.  Establishment is sometimes
    // very slow indeed, especially on virtual machines, so a fixed timeout is
    // not suitable.  This wait is required because we want to take precise
    // control of the channel startup timing, and shouldn't be needed in
    // production use, where the inner frame's channel is typically not started
    // by a DOM call as it is here.
    if (!this.getInnerPeer_() || !this.getInnerPeer_().instantiateChannel) {
      window.setTimeout(goog.bind(this.continueConnect_, this, reverse), 100);
      return;
    }

    const connectFromOuterFrame = goog.bind(
        this.channel_.connect, this.channel_,
        goog.bind(this.outerFrameConnected_, this));
    const innerConfig = this.innerFrameCfg_;
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const connectFromInnerFrame = goog.bind(
        this.getInnerPeer_().instantiateChannel, this.getInnerPeer_(),
        innerConfig);

    // Take control of the timing and reverse of each frame's first SETUP call.
    // If these happen to fire right on top of each other, that tends to mask
    // problems that reliably occur when there is a short delay.
    window.setTimeout(connectFromOuterFrame, reverse ? 1 : 10);
    window.setTimeout(connectFromInnerFrame, reverse ? 10 : 1);
  }

  /**
   * Called by the outer frame connection callback.
   * @private
   */
  outerFrameConnected_() {
    const payload = this.outerFrameEchoPayload_ = xpc.getRandomString(10);
    this.channel_.send('echo', payload);
  }

  /**
   * Called by the inner frame connection callback in inner_peer.html.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  innerFrameConnected() {
    const payload = this.innerFrameEchoPayload_ = xpc.getRandomString(10);
    this.getInnerPeer_().sendEcho(payload);
  }

  /**
   * The handler function for incoming echo requests.
   * @param {string} payload The message payload.
   * @private
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  echoHandler_(payload) {
    assertTrue('outer frame should be connected', this.channel_.isConnected());
    const peer = this.getInnerPeer_();
    assertTrue('child should be connected', peer.isConnected());
    this.channel_.send('response', payload);
  }

  /**
   * The handler function for incoming echo responses.
   * @param {string} payload The message payload.
   * @private
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  responseHandler_(payload) {
    assertTrue('outer frame should be connected', this.channel_.isConnected());
    const peer = this.getInnerPeer_();
    assertTrue('child should be connected', peer.isConnected());
    assertEquals(this.outerFrameEchoPayload_, payload);
    this.outerFrameResponseReceived_.resolve(true);
  }

  /**
   * The handler function for incoming echo replies. Called from
   * inner_peer.html.
   * @param {string} payload The message payload.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  innerFrameGotResponse(payload) {
    assertTrue('outer frame should be connected', this.channel_.isConnected());
    const peer = this.getInnerPeer_();
    assertTrue('child should be connected', peer.isConnected());
    assertEquals(this.innerFrameEchoPayload_, payload);
    this.innerFrameResponseReceived_.resolve(true);
  }

  /**
   * The second phase of the standard test, where reconnections of both the
   * inner and outer frames are performed.
   * @param {boolean} outerFrameReconnectSupported Whether outer frame
   *     reconnects are supported, and should be tested.
   * @param {boolean} innerFrameMigrationSupported
   * @private
   */
  testReconnects_(outerFrameReconnectSupported, innerFrameMigrationSupported) {
    G_testRunner.log('Performing inner frame reconnect');
    this.reinitializePromises_();
    this.innerFrameResponseReceived_.promise.then(
        this.checkChannelNames_, null, this);

    if (outerFrameReconnectSupported) {
      this.innerFrameResponseReceived_.promise.then(goog.bind(
          this.performOuterFrameReconnect_, this,
          innerFrameMigrationSupported));
    } else if (innerFrameMigrationSupported) {
      this.innerFrameResponseReceived_.promise.then(
          this.migrateInnerFrame_, null, this);
    }

    this.performInnerFrameReconnect_();
  }

  /**
   * Initializes the promise resolvers and clears the echo payloads, ready for
   * another sub-test.
   * @private
   */
  reinitializePromises_() {
    this.innerFrameEchoPayload_ = null;
    this.outerFrameEchoPayload_ = null;
    this.innerFrameResponseReceived_.promise.cancel();
    this.innerFrameResponseReceived_ = GoogPromise.withResolver();
    this.outerFrameResponseReceived_.promise.cancel();
    this.outerFrameResponseReceived_ = GoogPromise.withResolver();
  }

  /**
   * Get the inner frame to reconnect, and repeat the echo test.
   * @private
   * @suppress {missingProperties} suppression added to enable type checking
   */
  performInnerFrameReconnect_() {
    const peer = this.getInnerPeer_();
    peer.instantiateChannel(this.innerFrameCfg_);
  }

  /**
   * Get the outer frame to reconnect, and repeat the echo test.
   * @private
   */
  performOuterFrameReconnect_(innerFrameMigrationSupported) {
    G_testRunner.log('Closing channel');
    this.channel_.close();

    // If there is another channel still open, the native transport's global
    // postMessage listener will still be active.  This will mean that messages
    // being sent to the now-closed channel will still be received and
    // delivered, such as transport service traffic from its previous
    // correspondent in the other frame.  Ensure these messages don't cause
    // exceptions.
    try {
      this.channel_.xpcDeliver(xpc.TRANSPORT_SERVICE, 'payload');
    } catch (e) {
      fail('Should not throw exception');
    }

    G_testRunner.log('Reconnecting outer frame');
    this.reinitializePromises_();
    this.innerFrameResponseReceived_.promise.then(
        this.checkChannelNames_, null, this);
    if (innerFrameMigrationSupported) {
      this.outerFrameResponseReceived_.promise.then(
          this.migrateInnerFrame_, null, this);
    }
    this.channel_.connect(goog.bind(this.outerFrameConnected_, this));
  }

  /**
   * Migrate the inner frame to the alternate protocol version and reconnect it.
   * @private
   */
  migrateInnerFrame_() {
    G_testRunner.log('Migrating inner frame');
    this.reinitializePromises_();
    const innerFrameProtoVersion =
        this.innerFrameCfg_[CfgFields.NATIVE_TRANSPORT_PROTOCOL_VERSION];
    this.innerFrameResponseReceived_.promise.then(
        this.checkChannelNames_, null, this);
    this.innerFrameCfg_[CfgFields.NATIVE_TRANSPORT_PROTOCOL_VERSION] =
        innerFrameProtoVersion == 1 ? 2 : 1;
    this.performInnerFrameReconnect_();
  }

  /**
   * Determines if the transport type for the channel is testable.
   * Some transports are misusing global state or making other
   * assumptions that cause connections to fail.
   * @return {boolean} Whether the transport is testable.
   * @private
   */
  isTransportTestable_() {
    let testable = false;

    /** @suppress {visibility} suppression added to enable type checking */
    const transportType = this.channel_.determineTransportType_();
    switch (transportType) {
      case TransportTypes.NATIVE_MESSAGING:
      case TransportTypes.DIRECT:
        testable = true;
        break;
    }

    return testable;
  }

  /** @return {?CrossPageChannel} */
  getChannel() {
    return this.channel_;
  }

  /**
   * Begin, but don't finish, connection to a peer.
   *
   * @return {!Promise<undefined>} A timing hook for the unstable period between
   *     the creation of the peer and the connection notification from that
   * peer.
   */
  connectAndWaitForPeer() {
    this.channel_.connect();
    // Set a listener for when the peer exists.
    return new Promise(
        /** @suppress {visibility} suppression added to enable type checking */
        (res) => this.channel_.peerWindowDeferred_.addCallback(res));
  }
};
