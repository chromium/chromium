'use strict';

function parallelPromiseTest(func, description) {
  async_test((t) => {
    Promise.resolve(func(t)).then(() => t.done()).catch(t.step_func((e) => {
      throw e;
    }));
  }, description);
}

/** @enum {string} */
const BeaconDataType = {
  String: 'String',
  ArrayBuffer: 'ArrayBuffer',
  FormData: 'FormData',
  URLSearchParams: 'URLSearchParams',
};

// Creates beacon data of the given `dataType` from `data`.
// @param {string} data - A string representation of the beacon data. Note that
//     it cannot contain UTF-16 surrogates for all `BeaconDataType` except BLOB.
// @param {BeaconDataType} dataType - must be one of `BeaconDataType`.
function makeBeaconData(data, dataType) {
  switch (dataType) {
    case BeaconDataType.String:
      return data;
    case BeaconDataType.ArrayBuffer:
      return new TextEncoder().encode(data).buffer;
    case BeaconDataType.FormData:
      const formData = new FormData();
      formData.append('payload', data);
      return formData;
    case BeaconDataType.URLSearchParams:
      return new URLSearchParams(data);
    default:
      throw Error(`Unsupported beacon dataType: ${dataType}`);
  }
}

// Create a string of `end`-`begin` characters, with characters starting from
// UTF-16 code unit `begin` to `end`-1.
function generateSequentialData(begin, end, skip) {
  const codeUnits = Array(end - begin).fill().map((el, i) => i + begin);
  if (skip) {
    return String.fromCharCode(
        ...codeUnits.filter(c => !skip.includes(String.fromCharCode(c))));
  }
  return String.fromCharCode(...codeUnits);
}

function wait(ms) {
  return new Promise(resolve => step_timeout(resolve, ms));
}

async function getBeaconCount(uuid) {
  const res = await fetch(
      `resources/get_beacon_count.py?uuid=${uuid}`, {cache: 'no-store'});
  const count = await res.json();
  return count;
}

async function getBeaconData(uuid) {
  const res = await fetch(
      `resources/get_beacon_data.py?uuid=${uuid}`, {cache: 'no-store'});
  const data = await res.text();
  return data;
}

// Retrieve beacon data for `uuid`, and perform percent-decoding.
async function getPercentDecodedBeaconData(uuid) {
  let data = await getBeaconData(uuid);
  // application/x-www-form-urlencoded serializer encodes space as '+'
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
  data = data.replace(/\+/g, '%20');
  return decodeURIComponent(data);
}

function beaconSendDataTest(
    method, dataType, testData, expectEmpty, description) {
  parallelPromiseTest(async t => {
    const uuid = token();
    const baseUrl = `${location.protocol}//${location.host}`;
    const url = `${
        baseUrl}/wpt_internal/pending_beacon/resources/set_beacon_data.py?uuid=${
        uuid}`;
    let beacon = new PendingBeacon(url, {method: method});

    beacon.setData(makeBeaconData(testData, dataType));
    beacon.sendNow();
    // Wait for the beacon to have sent.
    await wait(1000);

    const sentData = dataType === BeaconDataType.URLSearchParams ?
        await getPercentDecodedBeaconData(uuid) :
        await getBeaconData(uuid);
    assert_equals(sentData, expectEmpty ? '<NO-DATA>' : testData);
  }, `Beacon data of type "${dataType}": ${description}`);
}
