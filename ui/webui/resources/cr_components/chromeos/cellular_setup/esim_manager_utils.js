// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {getESimManagerRemote} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// clang-format on

cr.define('cellular_setup', function() {
  /**
   * Fetches the EUICC's eSIM profiles with status 'Pending'.
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * @return {!Promise<Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>>}
   */
  /* #export */ function getPendingESimProfiles(euicc) {
    return euicc.getProfileList().then(response => {
      return filterForPendingProfiles_(response.profiles);
    });
  }

  /**
   * @private
   * @param {!Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>} profiles
   * @return {!Promise<Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>>}
   */
  function filterForPendingProfiles_(profiles) {
    const profilePromises = profiles.map(profile => {
      return profile.getProperties().then(response => {
        if (response.properties.state !==
            chromeos.cellularSetup.mojom.ProfileState.kPending) {
          return null;
        }
        return profile;
      });
    });
    return Promise.all(profilePromises).then(profiles => {
      return profiles.filter(profile => {
        return profile !== null;
      });
    });
  }

  // #cr_define_end
  return {
    getPendingESimProfiles,
  };
});
