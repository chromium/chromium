// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-lite.js';
import '../../../cr_elements/cr_button/cr_button.m.js';
import './routine_group.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior} from '../../../js/i18n_behavior.m.js';

import {getNetworkDiagnosticsService} from './mojo_interface_provider.js';
import {Routine, RoutineGroup, RoutineResponse} from './network_diagnostics_types.js';

/**
 * @fileoverview Polymer element for interacting with Network Diagnostics.
 */

// Namespace to make using the mojom objects more readable.
const diagnosticsMojom = chromeos.networkDiagnostics.mojom;

/**
 * Helper function to create a routine object.
 * @param {string} name
 * @param {!diagnosticsMojom.RoutineType} type
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
  _template: html`{__html_template__}`,
  is: 'network-diagnostics',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @private */
    areArcNetworkingRoutinesEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableArcNetworkDiagnostics');
      }
    },

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
                type: diagnosticsMojom.RoutineType.kLanConnectivity,
                func: () => getNetworkDiagnosticsService().runLanConnectivity(),
              },
            ]
          },
          {
            group: RoutineGroup.WIFI,
            routines: [
              {
                name: 'NetworkDiagnosticsSignalStrength',
                type: diagnosticsMojom.RoutineType.kSignalStrength,
                func: () => getNetworkDiagnosticsService().runSignalStrength(),
              },
              {
                name: 'NetworkDiagnosticsHasSecureWiFiConnection',
                type: diagnosticsMojom.RoutineType.kHasSecureWiFiConnection,
                func: () =>
                    getNetworkDiagnosticsService().runHasSecureWiFiConnection(),
              },
            ]
          },
          {
            group: RoutineGroup.PORTAL,
            routines: [
              {
                name: 'NetworkDiagnosticsCaptivePortal',
                type: diagnosticsMojom.RoutineType.kCaptivePortal,
                func: () => getNetworkDiagnosticsService().runCaptivePortal(),
              },
            ]
          },
          {
            group: RoutineGroup.GATEWAY,
            routines: [
              {
                name: 'NetworkDiagnosticsGatewayCanBePinged',
                type: diagnosticsMojom.RoutineType.kGatewayCanBePinged,
                func: () =>
                    getNetworkDiagnosticsService().runGatewayCanBePinged(),
              },
            ]
          },
          {
            group: RoutineGroup.FIREWALL,
            routines: [
              {
                name: 'NetworkDiagnosticsHttpFirewall',
                type: diagnosticsMojom.RoutineType.kHttpFirewall,
                func: () => getNetworkDiagnosticsService().runHttpFirewall(),
              },
              {
                name: 'NetworkDiagnosticsHttpsFirewall',
                type: diagnosticsMojom.RoutineType.kHttpsFirewall,
                func: () => getNetworkDiagnosticsService().runHttpsFirewall(),

              },
              {
                name: 'NetworkDiagnosticsHttpsLatency',
                type: diagnosticsMojom.RoutineType.kHttpsLatency,
                func: () => getNetworkDiagnosticsService().runHttpsLatency(),
              },
            ]
          },
          {
            group: RoutineGroup.DNS,
            routines: [
              {
                name: 'NetworkDiagnosticsDnsResolverPresent',
                type: diagnosticsMojom.RoutineType.kDnsResolverPresent,
                func: () =>
                    getNetworkDiagnosticsService().runDnsResolverPresent(),
              },
              {
                name: 'NetworkDiagnosticsDnsLatency',
                type: diagnosticsMojom.RoutineType.kDnsLatency,
                func: () => getNetworkDiagnosticsService().runDnsLatency(),
              },
              {
                name: 'NetworkDiagnosticsDnsResolution',
                type: diagnosticsMojom.RoutineType.kDnsResolution,
                func: () => getNetworkDiagnosticsService().runDnsResolution(),
              },
            ]
          },
          {
            group: RoutineGroup.GOOGLE_SERVICES,
            routines: [
              {
                name: 'NetworkDiagnosticsVideoConferencing',
                type: diagnosticsMojom.RoutineType.kVideoConferencing,
                // A null stun_server_hostname will use the routine
                // default.
                func: () => getNetworkDiagnosticsService().runVideoConferencing(
                    /*stun_server_hostname=*/ null),
              },
            ]
          }
        ];
        if (this.areArcNetworkingRoutinesEnabled_) {
          routineGroups.push({
            group: RoutineGroup.ARC,
            routines: [
              {
                name: 'ArcNetworkDiagnosticsPing',
                type: diagnosticsMojom.RoutineType.kArcPing,
                func: () => getNetworkDiagnosticsService().runArcPing(),
              },
              {
                name: 'ArcNetworkDiagnosticsHttp',
                type: diagnosticsMojom.RoutineType.kArcHttp,
                func: () => getNetworkDiagnosticsService().runArcHttp(),
              },
              {
                name: 'ArcNetworkDiagnosticsDnsResolution',
                type: diagnosticsMojom.RoutineType.kArcDnsResolution,
                func: () =>
                    getNetworkDiagnosticsService().runArcDnsResolution(),
              },
            ]
          });
        }
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
   * Runs all supported network diagnostics routines.
   * @public
   */
  runAllRoutines() {
    for (const routine of this.routines_) {
      this.runRoutine_(routine.type);
    }
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
   * @param {!diagnosticsMojom.RoutineType} type
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
   * @param {!diagnosticsMojom.RoutineType} type
   * @param {!RoutineResponse} response
   * @private
   */
  evaluateRoutine_(type, response) {
    const routine = `routines_.${type}`;
    this.set(routine + '.running', false);
    this.set(routine + '.result', response.result);

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

    const problemStrings = this.getRoutineProblemsString_(
        routine.type, routine.result.problems, true);

    if (problemStrings.length) {
      return this.i18n(
          'NetworkDiagnosticsResultPlaceholder', verdict, ...problemStrings);
    } else if (routine.result) {
      return verdict;
    }

    return '';
  },

  /**
   * @param {!diagnosticsMojom.RoutineType} type The type of routine
   * @param {!diagnosticsMojom.RoutineProblems} problems The list of
   *     problems for the routine
   * @param {boolean} translate Flag to return a translated string
   * @return {!Array<string>} List of network diagnostic problem strings
   * @private
   */
  getRoutineProblemsString_(type, problems, translate) {
    const getString = s => translate ? this.i18n(s) : s;

    let problemStrings = [];
    switch (type) {
      case diagnosticsMojom.RoutineType.kSignalStrength:
        if (!problems.signalStrengthProblems) {
          break;
        }

        for (const problem of problems.signalStrengthProblems) {
          switch (problem) {
            case diagnosticsMojom.SignalStrengthProblem.kWeakSignal:
              problemStrings.push(getString('SignalStrengthProblem_Weak'));
              break;
          }
        }
        break;

      case diagnosticsMojom.RoutineType.kGatewayCanBePinged:
        if (!problems.gatewayCanBePingedProblems) {
          break;
        }

        for (const problem of problems.gatewayCanBePingedProblems) {
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
        }
        break;

      case diagnosticsMojom.RoutineType.kHasSecureWiFiConnection:
        if (!problems.hasSecureWifiConnectionProblems) {
          break;
        }

        for (const problem of problems.hasSecureWifiConnectionProblems) {
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
        }
        break;

      case diagnosticsMojom.RoutineType.kDnsResolverPresent:
        if (!problems.dnsResolverPresentProblems) {
          break;
        }

        for (const problem of problems.dnsResolverPresentProblems) {
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
          }
        }
        break;

      case diagnosticsMojom.RoutineType.kDnsLatency:
        if (!problems.dnsLatencyProblems) {
          break;
        }

        for (const problem of problems.dnsLatencyProblems) {
          switch (problem) {
            case diagnosticsMojom.DnsLatencyProblem.kHostResolutionFailure:
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
        }
        break;

      case diagnosticsMojom.RoutineType.kDnsResolution:
        if (!problems.dnsResolutionProblems) {
          break;
        }

        for (const problem of problems.dnsResolutionProblems) {
          switch (problem) {
            case diagnosticsMojom.DnsResolutionProblem.kFailedToResolveHost:
              problemStrings.push(
                  getString('DnsResolutionProblem_FailedResolve'));
              break;
          }
        }
        break;

      case diagnosticsMojom.RoutineType.kHttpFirewall:
        if (!problems.httpFirewallProblems) {
          break;
        }

        for (const problem of problems.httpFirewallProblems) {
          switch (problem) {
            case diagnosticsMojom.HttpFirewallProblem
                .kDnsResolutionFailuresAboveThreshold:
              problemStrings.push(
                  getString('FirewallProblem_DnsResolutionFailureRate'));
              break;
            case diagnosticsMojom.HttpFirewallProblem.kFirewallDetected:
              problemStrings.push(
                  getString('FirewallProblem_FirewallDetected'));
              break;
            case diagnosticsMojom.HttpFirewallProblem.kPotentialFirewall:
              problemStrings.push(
                  getString('FirewallProblem_FirewallSuspected'));
              break;
          }
        }
        break;

      case diagnosticsMojom.RoutineType.kHttpsFirewall:
        if (!problems.httpsFirewallProblems) {
          break;
        }

        for (const problem of problems.httpsFirewallProblems) {
          switch (problem) {
            case diagnosticsMojom.HttpsFirewallProblem
                .kHighDnsResolutionFailureRate:
              problemStrings.push(
                  getString('FirewallProblem_DnsResolutionFailureRate'));
              break;
            case diagnosticsMojom.HttpsFirewallProblem.kFirewallDetected:
              problemStrings.push(
                  getString('FirewallProblem_FirewallDetected'));
              break;
            case diagnosticsMojom.HttpsFirewallProblem.kPotentialFirewall:
              problemStrings.push(
                  getString('FirewallProblem_FirewallSuspected'));
              break;
          }
        }
        break;

      case diagnosticsMojom.RoutineType.kHttpsLatency:
        if (!problems.httpsLatencyProblems) {
          break;
        }

        for (const problem of problems.httpsLatencyProblems) {
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
        break;

      case diagnosticsMojom.RoutineType.kCaptivePortal:
        if (!problems.captivePortalProblems) {
          break;
        }

        for (const problem of problems.captivePortalProblems) {
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
        }
        break;

      case diagnosticsMojom.RoutineType.kVideoConferencing:
        if (!problems.videoConferencingProblems) {
          break;
        }

        for (const problem of problems.videoConferencingProblems) {
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
        break;

      case diagnosticsMojom.RoutineType.kArcPing:
        if (!problems.arcPingProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcPingProblemStringIds(problems.arcPingProblems)
                .map(getString));
        break;

      case diagnosticsMojom.RoutineType.kArcDnsResolution:
        if (!problems.arcDnsResolutionProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcDnsProblemStringIds(problems.arcDnsResolutionProblems)
                .map(getString));
        break;

      case diagnosticsMojom.RoutineType.kArcHttp:
        if (!problems.arcHttpProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcHttpProblemStringIds(problems.arcHttpProblems)
                .map(getString));
        break;
    }

    return problemStrings;
  },

  /**
   * Converts a collection ArcPingProblem into string identifiers for display.
   *
   * @param {!Array<diagnosticsMojom.ArcPingProblem>} problems A collection of
   *     ArcPingProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcPingProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case diagnosticsMojom.ArcPingProblem.kFailedToGetArcServiceManager:
        case diagnosticsMojom.ArcPingProblem
            .kGetManagedPropertiesTimeoutFailure:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case diagnosticsMojom.ArcPingProblem.kFailedToGetNetInstanceForPingTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case diagnosticsMojom.ArcPingProblem.kUnreachableGateway:
          problemStringIds.push('GatewayPingProblem_Unreachable');
          break;
        case diagnosticsMojom.ArcPingProblem.kFailedToPingDefaultNetwork:
          problemStringIds.push('GatewayPingProblem_NoDefaultPing');
          break;
        case diagnosticsMojom.ArcPingProblem
            .kDefaultNetworkAboveLatencyThreshold:
          problemStringIds.push('GatewayPingProblem_DefaultLatency');
          break;
        case diagnosticsMojom.ArcPingProblem
            .kUnsuccessfulNonDefaultNetworksPings:
          problemStringIds.push('GatewayPingProblem_NoNonDefaultPing');
          break;
        case diagnosticsMojom.ArcPingProblem
            .kNonDefaultNetworksAboveLatencyThreshold:
          problemStringIds.push('GatewayPingProblem_NonDefaultLatency');
      }
    }

    return problemStringIds;
  },

  /**
   * Converts a collection ArcDnsResolutionProblem into string identifiers for
   * display.
   *
   * @param {!Array<diagnosticsMojom.ArcDnsResolutionProblem>} problems A
   *     collection of ArcDnsResolutionProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcDnsProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case diagnosticsMojom.ArcDnsResolutionProblem
            .kFailedToGetArcServiceManager:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case diagnosticsMojom.ArcDnsResolutionProblem
            .kFailedToGetNetInstanceForDnsResolutionTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case diagnosticsMojom.ArcDnsResolutionProblem.kHighLatency:
          problemStringIds.push('DnsLatencyProblem_LatencySlightlyAbove');
          break;
        case diagnosticsMojom.ArcDnsResolutionProblem.kVeryHighLatency:
          problemStringIds.push('DnsLatencyProblem_LatencySignificantlyAbove');
          break;
        case diagnosticsMojom.ArcDnsResolutionProblem.kFailedDnsQueries:
          problemStringIds.push('DnsResolutionProblem_FailedResolve');
          break;
      }
    }

    return problemStringIds;
  },

  /**
   * Converts a collection ArcHttpProblem into string identifiers for display.
   *
   * @param {!Array<diagnosticsMojom.ArcHttpProblem>} problems A collection of
   *     ArcHttpProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcHttpProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case diagnosticsMojom.ArcHttpProblem.kFailedToGetArcServiceManager:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case diagnosticsMojom.ArcHttpProblem.kFailedToGetNetInstanceForHttpTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case diagnosticsMojom.ArcHttpProblem.kHighLatency:
          problemStringIds.push('ArcHttpProblem_HighLatency');
          break;
        case diagnosticsMojom.ArcHttpProblem.kVeryHighLatency:
          problemStringIds.push('ArcHttpProblem_VeryHighLatency');
          break;
        case diagnosticsMojom.ArcHttpProblem.kFailedHttpRequests:
          problemStringIds.push('ArcHttpProblem_FailedHttpRequests');
          break;
      }
    }

    return problemStringIds;
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
