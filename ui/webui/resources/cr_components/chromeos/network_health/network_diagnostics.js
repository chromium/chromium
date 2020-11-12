
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for interacting with Network Diagnostics.
 */

// Namespace to make using the mojom objects more readable.
const diagnosticsMojom = chromeos.networkDiagnostics.mojom;

/**
 * A routine response from the Network Diagnostics mojo service.
 * @typedef {{
 *   verdict: chromeos.networkDiagnostics.mojom.RoutineVerdict,
 * }}
 * RoutineResponse can optionally have a `problems` field, which is an array of
 * enums relevant to the routine run. Unfortunately the closure compiler cannot
 * handle optional object fields.
 */
let RoutineResponse;

/**
 * A network diagnostics routine. Holds descriptive information about the
 * routine, and it's transient state.
 * @typedef {{
 *   name: string,
 *   type: !RoutineType,
 *   running: boolean,
 *   resultMsg: string,
 *   result: ?RoutineResponse,
 * }}
 */
let Routine;

/**
 * Definition for all Network diagnostic routine types. This enum is intended
 * to be used as an index in an array of routines.
 * @enum {number}
 */
const RoutineType = {
  LAN_CONNECTIVITY: 0,
  SIGNAL_STRENGTH: 1,
  GATEWAY_PING: 2,
  SECURE_WIFI: 3,
  DNS_RESOLVER: 4,
  DNS_LATENCY: 5,
  DNS_RESOLUTION: 6,
  HTTP_FIREWALL: 7,
  HTTPS_FIREWALL: 8,
  HTTPS_LATENCY: 9,
};

/**
 * Helper function to create a routine object.
 * @param {string} name
 * @param {!RoutineType} type
 * @return {!Routine} Routine object
 */
function createRoutine(name, type) {
  return {
    name: name,
    type: type,
    running: false,
    resultMsg: '',
    result: null,
    ariaDescription: '',
  };
}

Polymer({
  is: 'network-diagnostics',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * List of Diagnostics Routines
     * @private {!Array<!Routine>}
     */
    routines_: {
      type: Array,
      value: function() {
        const routines = [];
        routines[RoutineType.LAN_CONNECTIVITY] = createRoutine(
            'NetworkDiagnosticsLanConnectivity', RoutineType.LAN_CONNECTIVITY);
        routines[RoutineType.SIGNAL_STRENGTH] = createRoutine(
            'NetworkDiagnosticsSignalStrength', RoutineType.SIGNAL_STRENGTH);
        routines[RoutineType.GATEWAY_PING] = createRoutine(
            'NetworkDiagnosticsGatewayCanBePinged', RoutineType.GATEWAY_PING);
        routines[RoutineType.SECURE_WIFI] = createRoutine(
            'NetworkDiagnosticsHasSecureWiFiConnection',
            RoutineType.SECURE_WIFI);
        routines[RoutineType.DNS_RESOLVER] = createRoutine(
            'NetworkDiagnosticsDnsResolverPresent', RoutineType.DNS_RESOLVER);
        routines[RoutineType.DNS_LATENCY] = createRoutine(
            'NetworkDiagnosticsDnsLatency', RoutineType.DNS_LATENCY);
        routines[RoutineType.DNS_RESOLUTION] = createRoutine(
            'NetworkDiagnosticsDnsResolution', RoutineType.DNS_RESOLUTION);
        routines[RoutineType.HTTP_FIREWALL] = createRoutine(
            'NetworkDiagnosticsHttpFirewall', RoutineType.HTTP_FIREWALL);
        routines[RoutineType.HTTPS_FIREWALL] = createRoutine(
            'NetworkDiagnosticsHttpsFirewall', RoutineType.HTTPS_FIREWALL);
        routines[RoutineType.HTTPS_LATENCY] = createRoutine(
            'NetworkDiagnosticsHttpsLatency', RoutineType.HTTPS_LATENCY);
        return routines;
      }
    }
  },

  /**
   * Network Diagnostics mojo remote.
   * @private {
   *     ?chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutinesRemote}
   */
  networkDiagnostics_: null,

  /** @override */
  created() {
    this.networkDiagnostics_ =
        diagnosticsMojom.NetworkDiagnosticsRoutines.getRemote();
  },

  /**
   * Runs all supported network diagnostics routines.
   * @public
   */
  runAllRoutines() {
    for (const routine of this.routines_) {
      this.runRoutine_(routine.type);
    }
  },

  /**
   * Gets the network diagnostics routine results and organizes them into a
   * stringified object that is returned.
   * @return {!string} The network diagnostics routine results
   * @public
   */
  getResults() {
    const results = {};
    for (const routine of this.routines_) {
      if (routine.result) {
        const name = routine.name.replace('NetworkDiagnostics', '');
        const result = {};
        result['verdict'] =
            this.getRoutineVerdictRawString_(routine.result.verdict);
        if (routine.result.problems && routine.result.problems.length > 0) {
          result['problems'] = this.getRoutineProblemsString_(
              routine.type, routine.result.problems, false);
        }

        results[name] = result;
      }
    }
    return JSON.stringify(results, undefined, 2);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRunRoutineClick_(event) {
    this.runRoutine_(event.model.index);
  },

  /**
   * @param {!RoutineType} type
   * @private
   */
  runRoutine_(type) {
    this.set(`routines_.${type}.running`, true);
    this.set(`routines_.${type}.resultMsg`, '');
    this.set(`routines_.${type}.result`, null);
    this.set(
        `routines_.${type}.ariaDescription`,
        this.i18n('NetworkDiagnosticsRunning'));

    switch (type) {
      case RoutineType.LAN_CONNECTIVITY:
        this.networkDiagnostics_.lanConnectivity().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.SIGNAL_STRENGTH:
        this.networkDiagnostics_.signalStrength().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.GATEWAY_PING:
        this.networkDiagnostics_.gatewayCanBePinged().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.SECURE_WIFI:
        this.networkDiagnostics_.hasSecureWiFiConnection().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_RESOLVER:
        this.networkDiagnostics_.dnsResolverPresent().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_LATENCY:
        this.networkDiagnostics_.dnsLatency().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.DNS_RESOLUTION:
        this.networkDiagnostics_.dnsResolution().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.HTTP_FIREWALL:
        this.networkDiagnostics_.httpFirewall().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.HTTPS_FIREWALL:
        this.networkDiagnostics_.httpsFirewall().then(
            result => this.evaluateRoutine_(type, result));
        break;
      case RoutineType.HTTPS_LATENCY:
        this.networkDiagnostics_.httpsLatency().then(
            result => this.evaluateRoutine_(type, result));
        break;
    }
  },

  /**
   * @param {!RoutineType} type
   * @param {!RoutineResponse} result
   * @private
   */
  evaluateRoutine_(type, result) {
    const routine = `routines_.${type}`;
    this.set(routine + '.running', false);
    this.set(routine + '.result', result);

    const resultMsg = this.getRoutineResult_(this.routines_[type]);
    this.set(routine + '.resultMsg', resultMsg);
    this.set(routine + '.ariaDescription', resultMsg);
  },

  /**
   * Helper function to get the icon for a routine based on the result.
   * @param {RoutineResponse} result
   * @return {string}
   * @private
   */
  getRoutineIcon_(result) {
    if (!result) {
      return 'test_not_run.png';
    }

    switch (result.verdict) {
      case diagnosticsMojom.RoutineVerdict.kNoProblem:
        return 'test_passed.png';
      case diagnosticsMojom.RoutineVerdict.kProblem:
        return 'test_failed.png';
      case diagnosticsMojom.RoutineVerdict.kNotRun:
        return 'test_canceled.png';
    }

    return '';
  },

  /**
   * Helper function to generate the routine result string.
   * @param {Routine} routine
   * @return {string}
   * @private
   */
  getRoutineResult_(routine) {
    let verdict = '';
    switch (routine.result.verdict) {
      case diagnosticsMojom.RoutineVerdict.kNoProblem:
        verdict = this.i18n('NetworkDiagnosticsPassed');
        break;
      case diagnosticsMojom.RoutineVerdict.kProblem:
        verdict = this.i18n('NetworkDiagnosticsFailed');
        break;
      case diagnosticsMojom.RoutineVerdict.kNotRun:
        verdict = this.i18n('NetworkDiagnosticsNotRun');
        break;
    }

    if (routine.result && routine.result.problems &&
        routine.result.problems.length) {
      const problemStrings = this.getRoutineProblemsString_(
          routine.type, routine.result.problems, true);
      return this.i18n(
          'NetworkDiagnosticsResultPlaceholder', verdict, ...problemStrings);
    } else if (routine.result) {
      return verdict;
    }

    return '';
  },

  /**
   *
   * @param {!RoutineType} type The type of routine
   * @param {!Array<number>} problems The list of problems for the routine
   * @param {boolean} translate Flag to return a translated string
   * @return {!Array<string>} List of network diagnostic problem strings
   * @private
   */
  getRoutineProblemsString_(type, problems, translate) {
    const getString = s => translate ? this.i18n(s) : s;

    const problemStrings = [];
    for (const problem of problems) {
      switch (type) {
        case RoutineType.SIGNAL_STRENGTH:
          switch (problem) {
            case diagnosticsMojom.SignalStrengthProblem.kSignalNotFound:
              problemStrings.push(getString('SignalStrengthProblem_NotFound'));
              break;
            case diagnosticsMojom.SignalStrengthProblem.kWeakSignal:
              problemStrings.push(getString('SignalStrengthProblem_Weak'));
              break;
          }
          break;

        case RoutineType.GATEWAY_PING:
          switch (problem) {
            case diagnosticsMojom.GatewayCanBePingedProblem.kUnreachableGateway:
              problemStrings.push(getString('GatewayPingProblem_Unreachable'));
              break;
            case diagnosticsMojom.GatewayCanBePingedProblem
                .kFailedToPingDefaultNetwork:
              problemStrings.push(
                  getString('GatewayPingProblem_NoDefaultPing'));
              break;
            case diagnosticsMojom.GatewayCanBePingedProblem
                .kDefaultNetworkAboveLatencyThreshold:
              problemStrings.push(
                  getString('GatewayPingProblem_DefaultLatency'));
              break;
            case diagnosticsMojom.GatewayCanBePingedProblem
                .kUnsuccessfulNonDefaultNetworksPings:
              problemStrings.push(
                  getString('GatewayPingProblem_NoNonDefaultPing'));
              break;
            case diagnosticsMojom.GatewayCanBePingedProblem
                .kNonDefaultNetworksAboveLatencyThreshold:
              problemStrings.push(
                  getString('GatewayPingProblem_NonDefaultLatency'));
              break;
          }
          break;

        case RoutineType.SECURE_WIFI:
          switch (problem) {
            case diagnosticsMojom.HasSecureWiFiConnectionProblem
                .kSecurityTypeNone:
              problemStrings.push(getString('SecureWifiProblem_None'));
              break;
            case diagnosticsMojom.HasSecureWiFiConnectionProblem
                .kSecurityTypeWep8021x:
              problemStrings.push(getString('SecureWifiProblem_8021x'));
              break;
            case diagnosticsMojom.HasSecureWiFiConnectionProblem
                .kSecurityTypeWepPsk:
              problemStrings.push(getString('SecureWifiProblem_PSK'));
              break;
            case diagnosticsMojom.HasSecureWiFiConnectionProblem
                .kUnknownSecurityType:
              problemStrings.push(getString('SecureWifiProblem_Unknown'));
              break;
          }
          break;

        case RoutineType.DNS_RESOLVER:
          switch (problem) {
            case diagnosticsMojom.DnsResolverPresentProblem.kNoNameServersFound:
              problemStrings.push(
                  getString('DnsResolverProblem_NoNameServers'));
              break;
            case diagnosticsMojom.DnsResolverPresentProblem
                .kMalformedNameServers:
              problemStrings.push(
                  getString('DnsResolverProblem_MalformedNameServers'));
              break;
            case diagnosticsMojom.DnsResolverPresentProblem.kEmptyNameServers:
              problemStrings.push(
                  getString('DnsResolverProblem_EmptyNameServers'));
              break;
          }
          break;

        case RoutineType.DNS_LATENCY:
          switch (problem) {
            case diagnosticsMojom.DnsLatencyProblem.kFailedToResolveAllHosts:
              problemStrings.push(
                  getString('DnsLatencyProblem_FailedResolveHosts'));
              break;
            case diagnosticsMojom.DnsLatencyProblem.kSlightlyAboveThreshold:
              problemStrings.push(
                  getString('DnsLatencyProblem_LatencySlightlyAbove'));
              break;
            case diagnosticsMojom.DnsLatencyProblem
                .kSignificantlyAboveThreshold:
              problemStrings.push(
                  getString('DnsLatencyProblem_LatencySignificantlyAbove'));
              break;
          }
          break;

        case RoutineType.DNS_RESOLUTION:
          switch (problem) {
            case diagnosticsMojom.DnsResolutionProblem.kFailedToResolveHost:
              problemStrings.push(
                  getString('DnsResolutionProblem_FailedResolve'));
              break;
          }
          break;

        case RoutineType.HTTP_FIREWALL:
        case RoutineType.HTTPS_FIREWALL:
          switch (problem) {
            case diagnosticsMojom.HttpFirewallProblem
                .kDnsResolutionFailuresAboveThreshold:
            case diagnosticsMojom.HttpsFirewallProblem.kFailedDnsResolutions:
              problemStrings.push(
                  getString('FirewallProblem_DnsResolutionFailureRate'));
              break;
            case diagnosticsMojom.HttpFirewallProblem.kFirewallDetected:
            case diagnosticsMojom.HttpsFirewallProblem.kFirewallDetected:
              problemStrings.push(
                  getString('FirewallProblem_FirewallDetected'));
              break;
            case diagnosticsMojom.HttpFirewallProblem.kPotentialFirewall:
            case diagnosticsMojom.HttpsFirewallProblem.kPotentialFirewall:
              problemStrings.push(
                  getString('FirewallProblem_FirewallSuspected'));
              break;
          }

        case RoutineType.HTTPS_LATENCY:
          switch (problem) {
            case diagnosticsMojom.HttpsLatencyProblem.kFailedDnsResolutions:
              problemStrings.push(
                  getString('HttpsLatencyProblem_FailedDnsResolution'));
              break;
            case diagnosticsMojom.HttpsLatencyProblem.kFailedHttpsRequests:
              problemStrings.push(
                  getString('HttpsLatencyProblem_FailedHttpsRequests'));
              break;
            case diagnosticsMojom.HttpsLatencyProblem.kHighLatency:
              problemStrings.push(getString('HttpsLatencyProblem_HighLatency'));
              break;
            case diagnosticsMojom.HttpsLatencyProblem.kVeryHighLatency:
              problemStrings.push(
                  getString('HttpsLatencyProblem_VeryHighLatency'));
              break;
          }
      }
    }

    return problemStrings;
  },

  /**
   * @param {!chromeos.networkDiagnostics.mojom.RoutineVerdict} verdict
   * @return {string} Untranslated string for a network diagnostic verdict
   * @private
   */
  getRoutineVerdictRawString_(verdict) {
    switch (verdict) {
      case diagnosticsMojom.RoutineVerdict.kNoProblem:
        return 'Passed';
      case diagnosticsMojom.RoutineVerdict.kNotRun:
        return 'Not Run';
      case diagnosticsMojom.RoutineVerdict.kProblem:
        return 'Failed';
    }
    return 'Unknown';
  },
});
