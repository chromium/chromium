// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/shill_error.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ui {

namespace {

const ash::NetworkState* GetNetworkState(const std::string& network_id) {
  return ash::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

}  // namespace

namespace shill_error {

std::u16string GetShillErrorString(const std::string& error,
                                   const std::string& network_id) {
  if (error.empty())
    return std::u16string();
  if (error == shill::kErrorOutOfRange)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
  if (error == shill::kErrorPinMissing)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
  if (error == shill::kErrorDhcpFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
  if (error == shill::kErrorConnectFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error == shill::kErrorBadPassphrase ||
      error == shill::kErrorResultInvalidPassphrase) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  }
  if (error == shill::kErrorBadWEPKey)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
  if (error == shill::kErrorActivationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error == shill::kErrorNeedEvdo)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
  if (error == shill::kErrorNeedHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
  }
  if (error == shill::kErrorOtaspFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
  if (error == shill::kErrorAaaFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
  if (error == shill::kErrorInternal)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
  if (error == shill::kErrorDNSLookupFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
  }
  if (error == shill::kErrorHTTPGetFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  if (error == shill::kErrorIpsecPskAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
  }
  if (error == shill::kErrorIpsecCertAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
  }
  if (error == shill::kErrorEapAuthenticationFailed) {
    const ash::NetworkState* network =
        network_id.empty() ? nullptr : GetNetworkState(network_id);
    // TLS always requires a client certificate, so show a cert auth
    // failed message for TLS. Other EAP methods do not generally require
    // a client certicate.
    if (network && network->eap_method() == shill::kEapMethodTLS) {
      return l10n_util::GetStringUTF16(
          IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_CHROMEOS_NETWORK_ERROR_EAP_AUTH_FAILED);
    }
  }
  if (error == shill::kErrorEapLocalTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_LOCAL_TLS_FAILED);
  }
  if (error == shill::kErrorEapRemoteTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_REMOTE_TLS_FAILED);
  }
  if (error == shill::kErrorPppAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
  }
  if (error == shill::kErrorResultNotOnHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NOT_ON_HOME_NETWORK);
  }
  if (error == shill::kErrorNotAuthenticated) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  }
  if (error == shill::kErrorSimLocked) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_LOCKED);
  }
  if (error == shill::kErrorSimCarrierLocked) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_CARRIER_LOCKED);
  }
  if (error == shill::kErrorNotRegistered) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NOT_REGISTERED);
  }
  if (error == shill::kErrorResultWrongState) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_WRONG_STATE);
  }
  if (error == shill::kErrorResultWepNotSupported) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_WEP_NOT_SUPPORTED);
  }
  if (error == shill::kErrorTooManySTAs) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_NETWORK_TO_MANY_STAS_ERROR);
  }
  if (error == shill::kErrorResultInvalidApn) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INVALID_APN);
  }
  if (error == shill::kErrorSuspectInactiveSim) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_SUSPECT_INACTIVE_SIM);
  }
  if (error == shill::kErrorSuspectSubscriptionError) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_SUSPECT_SUBSCRIPTION_ERROR);
  }
  if (error == shill::kErrorSuspectModemDisallowed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_SUSPECT_MODEM_DISALLOWED);
  }

  if (base::ToLowerASCII(error) == base::ToLowerASCII(shill::kUnknownString)) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  return l10n_util::GetStringFUTF16(IDS_NETWORK_UNRECOGNIZED_ERROR,
                                    base::UTF8ToUTF16(error));
}

}  // namespace shill_error

}  // namespace ui
