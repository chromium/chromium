// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for interacting with Network Diagnostics.
 */

// Namespace to make using the mojom objects more readable.
const diagnosticsMojom = chromeos.networkDiagnostics.mojom;

/**
 * Helper function to create a routine object.
 * @param {string} name
 * @param {!RoutineType} type
 * @param {!RoutineGroup} group
 * @param {!function()} func
 * @return {!Routine} Routine object
 */
function createRoutine(name, type, group, func) {
  return {
    name: name,
    type: type,
    group: group,
    func: func,
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
        const routineGroups = [
          {
            group: RoutineGroup.CONNECTION,
            routines: [
              {
                name: 'NetworkDiagnosticsLanConnectivity',
                type: RoutineType.LAN_CONNECTIVITY,
                func: () => this.networkDiagnostics_.lanConnectivity(),
              },
            ]
          },
          {
            group: RoutineGroup.WIFI,
            routines: [
              {
                name: 'NetworkDiagnosticsSignalStrength',
                type: RoutineType.SIGNAL_STRENGTH,
                func: () => this.networkDiagnostics_.signalStrength(),
              },
              {
                name: 'NetworkDiagnosticsHasSecureWiFiConnection',
                type: RoutineType.SECURE_WIFI,
                func: () => this.networkDiagnostics_.hasSecureWiFiConnection(),
              },
            ]
          },
          {
            group: RoutineGroup.PORTAL,
            routines: [
              {
                name: 'NetworkDiagnosticsCaptivePortal',
                type: RoutineType.CAPTIVE_PORTAL,
                func: () => this.networkDiagnostics_.captivePortal(),
              },
            ]
          },
          {
            group: RoutineGroup.GATEWAY,
            routines: [
              {
                name: 'NetworkDiagnosticsGatewayCanBePinged',
                type: RoutineType.GATEWAY_PING,
                func: () => this.networkDiagnostics_.gatewayCanBePinged(),
              },
            ]
          },
          {
            group: RoutineGroup.FIREWALL,
            routines: [
              {
                name: 'NetworkDiagnosticsHttpFirewall',
                type: RoutineType.HTTP_FIREWALL,
                func: () => this.networkDiagnostics_.httpFirewall(),
              },
              {
                name: 'NetworkDiagnosticsHttpsFirewall',
                type: RoutineType.HTTPS_FIREWALL,
                func: () => this.networkDiagnostics_.httpsFirewall(),
              },
              {
                name: 'NetworkDiagnosticsHttpsLatency',
                type: RoutineType.HTTPS_LATENCY,
                func: () => this.networkDiagnostics_.httpsLatency(),
              },
            ]
          },
          {
            group: RoutineGroup.DNS,
            routines: [
              {
                name: 'NetworkDiagnosticsDnsResolverPresent',
                type: RoutineType.DNS_RESOLVER,
                func: () => this.networkDiagnostics_.dnsResolverPresent(),
              },
              {
                name: 'NetworkDiagnosticsDnsLatency',
                type: RoutineType.DNS_LATENCY,
                func: () => this.networkDiagnostics_.dnsLatency(),
              },
              {
                name: 'NetworkDiagnosticsDnsResolution',
                type: RoutineType.DNS_RESOLUTION,
                func: () => this.networkDiagnostics_.dnsResolution(),
              },
            ]
          },
          {
            group: RoutineGroup.GOOGLE_SERVICES,
            routines: [
              {
                name: 'NetworkDiagnosticsVideoConferencing',
                type: RoutineType.VIDEO_CONFERENCING,
                // A null stun_server_hostname will use the routine default.
                func: () => this.networkDiagnostics_.videoConferencing(
                    /*stun_server_hostname=*/ null),
              },
            ]
          },
        ];
        const routines = [];

        for (const group of routineGroups) {
          for (const routine of group.routines) {
            routines[routine.type] = createRoutine(
                routine.name, routine.type, group.group, routine.func);
          }
        }

        return routines;
      }
    },

    /**
     * Enum of Routine Groups
     * @private {Object}
     */
    RoutineGroup_: {
      type: Object,
      value: RoutineGroup,
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
   * Runs all supported network diagnostics routines.
   * @param {!PolymerDeepPropertyChange} routines
   * @param {Number} group
   * @return {!Array<!Routine>}
   * @private
   */
  getRoutineGroup_(routines, group) {
    return routines.base.filter(r => r.group === group);
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

    this.routines_[type].func().then(
        result => this.evaluateRoutine_(type, result));
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

        case RoutineType.CAPTIVE_PORTAL:
          switch (problem) {
            case diagnosticsMojom.CaptivePortalProblem.kNoActiveNetworks:
              problemStrings.push(
                  getString('CaptivePortalProblem_NoActiveNetworks'));
              break;
            case diagnosticsMojom.CaptivePortalProblem.kUnknownPortalState:
              problemStrings.push(
                  getString('CaptivePortalProblem_UnknownPortalState'));
              break;
            case diagnosticsMojom.CaptivePortalProblem.kPortalSuspected:
              problemStrings.push(
                  getString('CaptivePortalProblem_PortalSuspected'));
              break;
            case diagnosticsMojom.CaptivePortalProblem.kPortal:
              problemStrings.push(getString('CaptivePortalProblem_Portal'));
              break;
            case diagnosticsMojom.CaptivePortalProblem.kProxyAuthRequired:
              problemStrings.push(
                  getString('CaptivePortalProblem_ProxyAuthRequired'));
              break;
            case diagnosticsMojom.CaptivePortalProblem.kNoInternet:
              problemStrings.push(getString('CaptivePortalProblem_NoInternet'));
              break;
          }

        case RoutineType.VIDEO_CONFERENCING:
          switch (problem) {
            case diagnosticsMojom.VideoConferencingProblem.kUdpFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_UdpFailure'));
              break;
            case diagnosticsMojom.VideoConferencingProblem.kTcpFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_TcpFailure'));
              break;
            case diagnosticsMojom.VideoConferencingProblem.kMediaFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_MediaFailure'));
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
