// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {getESimManagerRemote} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

cr.define('cellular_setup', function() {
  /**
   * Fetches the EUICC's eSIM profiles with status 'Pending'.
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * @return {!Promise<!Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>>}
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

  /**
   * Returns the Euicc that should be used for eSim operations or null
   * if there is none available.
   * @return {!Promise<?chromeos.cellularSetup.mojom.EuiccRemote>}
   */
  /* #export */ async function getEuicc() {
    const eSimManagerRemote = cellular_setup.getESimManagerRemote();
    const response = await eSimManagerRemote.getAvailableEuiccs();
    if (!response || !response.euiccs) {
      return null;
    }
    // Onboard Euicc always appears at index 0. If useExternalEuicc flag
    // is set, use the next available Euicc.
    const euiccIndex = loadTimeData.getBoolean('useExternalEuicc') ? 1 : 0;
    if (euiccIndex >= response.euiccs.length) {
      return null;
    }
    return response.euiccs[euiccIndex];
  }

  /**
   * Returns the eSIM profile with iccid in the first EUICC and null if none
   * found.
   * @param {string} iccid
   * @return {!Promise<?chromeos.cellularSetup.mojom.ESimProfileRemote>}
   */
  /* #export */ async function getESimProfile(iccid) {
    if (!iccid) {
      return null;
    }
    const euicc = await cellular_setup.getEuicc();

    if (!euicc) {
      console.error('No Euiccs found');
      return null;
    }
    const esimProfilesRemotes = await euicc.getProfileList();

    for (const profileRemote of esimProfilesRemotes.profiles) {
      const profileProperties = await profileRemote.getProperties();

      if (profileProperties.properties.iccid === iccid) {
        return profileRemote;
      }
    }
    return null;
  }

  // #cr_define_end
  return {
    getEuicc,
    getESimProfile,
    getPendingESimProfiles,
  };
});
