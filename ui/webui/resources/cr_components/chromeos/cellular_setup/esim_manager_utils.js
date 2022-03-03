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
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   * @return {!Promise<!Array<!ash.cellularSetup.mojom.ESimProfileRemote>>}
   */
  /* #export */ function getPendingESimProfiles(euicc) {
    return euicc.getProfileList().then(response => {
      return filterByProfileProperties_(response.profiles, properties => {
        return properties.state ===
            ash.cellularSetup.mojom.ProfileState.kPending;
      });
    });
  }

  /**
   * Fetches the EUICC's eSIM profiles with status not 'Pending'.
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   * @return {!Promise<!Array<!ash.cellularSetup.mojom.ESimProfileRemote>>}
   */
  /* #export */ function getNonPendingESimProfiles(euicc) {
    return euicc.getProfileList().then(response => {
      return filterByProfileProperties_(response.profiles, properties => {
        return properties.state !==
            ash.cellularSetup.mojom.ProfileState.kPending;
      });
    });
  }

  /**
   * Filters each profile in profiles by callback, which is given the profile's
   * properties as an argument and returns true or false. Does not guarantee
   * that profiles retains the same order.
   * @private
   * @param {!Array<!ash.cellularSetup.mojom.ESimProfileRemote>} profiles
   * @param {function(ash.cellularSetup.mojom.ESimProfileProperties)}
   *     callback
   * @return {!Promise<Array<!ash.cellularSetup.mojom.ESimProfileRemote>>}
   */
  function filterByProfileProperties_(profiles, callback) {
    const profilePromises = profiles.map(profile => {
      return profile.getProperties().then(response => {
        if (!callback(response.properties)) {
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
   * @return {!Promise<number>}
   */
  /* #export */ function getNumESimProfiles() {
    return getEuicc()
        .then(euicc => {
          return euicc.getProfileList();
        })
        .then(response => {
          return response.profiles.length;
        });
  }

  /**
   * Returns the Euicc that should be used for eSim operations or null
   * if there is none available.
   * @return {!Promise<?ash.cellularSetup.mojom.EuiccRemote>}
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
   * @param {string} iccid
   * @return {!Promise<?{
   *       profileRemote: ash.cellularSetup.mojom.ESimProfileRemote,
   *       profileProperties: ash.cellularSetup.mojom.ESimProfileProperties
   *     }>} Returns a eSIM profile remote and profile properties for given
   *         |iccid|.
   */
  async function getESimProfileDetails(iccid) {
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
      const profilePropertiesResponse = await profileRemote.getProperties();
      if (!profilePropertiesResponse || !profilePropertiesResponse.properties) {
        return null;
      }

      const profileProperties = profilePropertiesResponse.properties;
      if (profileProperties.iccid === iccid) {
        return {profileRemote, profileProperties};
      }
    }
    return null;
  }

  /**
   * Returns the eSIM profile with iccid in the first EUICC or null if none
   * is found.
   * @param {string} iccid
   * @return {!Promise<?ash.cellularSetup.mojom.ESimProfileRemote>}
   */
  /* #export */ async function getESimProfile(iccid) {
    const details = await getESimProfileDetails(iccid);
    if (!details) {
      return null;
    }
    return details.profileRemote;
  }

  /**
   * Returns properties for eSIM profile with iccid in the first EUICC or null
   * if none is found.
   * @param {string} iccid
   * @return {!Promise<?ash.cellularSetup.mojom.ESimProfileProperties>}
   */
  /* #export */ async function getESimProfileProperties(iccid) {
    const details = await getESimProfileDetails(iccid);
    if (!details) {
      return null;
    }
    return details.profileProperties;
  }

  // #cr_define_end
  return {
    getEuicc,
    getESimProfile,
    getESimProfileProperties,
    getPendingESimProfiles,
    getNonPendingESimProfiles,
    getNumESimProfiles,
  };
});
