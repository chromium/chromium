/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.AesTest');
goog.setTestOnly();

const Aes = goog.require('goog.crypt.Aes');
const crypt = goog.require('goog.crypt');
const testSuite = goog.require('goog.testing.testSuite');

/** Override define value */
Aes['ENABLE_TEST_MODE'] = true;

/*
 * Unit test for goog.crypt.Aes using the test vectors from the spec:
 * http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */

let testData = null;

function doTest(key, input, values, dir) {
  testData = values;
  const keyArray = crypt.hexToByteArray(key);
  const aes = new Aes(keyArray);

  let testKeySchedule = onTestKeySchedule;
  let testStartRound = onTestStartRound;
  let testAfterSubBytes = onTestAfterSubBytes;
  let testAfterShiftRows = onTestAfterShiftRows;
  let testAfterMixColumns = onTestAfterMixColumns;
  let testAfterAddRoundKey = onTestAfterAddRoundKey;

  const inputArr = crypt.hexToByteArray(input);
  const keyArr = crypt.hexToByteArray(key);
  let outputArr = [];

  if (dir) {
    outputArr = aes.encrypt(inputArr);
  } else {
    outputArr = aes.decrypt(inputArr);
  }

  assertEquals(
      'Incorrect output for test ' + testData.name,
      testData[testData.length - 1].output, encodeHex(outputArr));
}

function onTestKeySchedule(roundNum, keySchedule, keyScheduleIndex) {
  assertNotNull(keySchedule);
  assertEquals(
      `Incorrect key for round ${roundNum}`, testData[roundNum].k_sch,
      encodeKey(keySchedule, keyScheduleIndex));
}

function onTestStartRound(roundNum, state) {
  assertEquals(
      'Incorrect state for test ' + testData.name + ' at start round ' +
          roundNum,
      testData[roundNum].start, encodeState(state));
}

function onTestAfterSubBytes(roundNum, state) {
  assertEquals(
      'Incorrect state for test ' + testData.name +
          ' after sub bytes in round ' + roundNum,
      testData[roundNum].s_box, encodeState(state));
}

function onTestAfterShiftRows(roundNum, state) {
  assertEquals(
      'Incorrect state for test ' + testData.name +
          ' after shift rows in round ' + roundNum,
      testData[roundNum].s_row, encodeState(state));
}

function onTestAfterMixColumns(roundNum, state) {
  assertEquals(
      'Incorrect state for test ' + testData.name +
          ' after mix columns in round ' + roundNum,
      testData[roundNum].m_col, encodeState(state));
}

function onTestAfterAddRoundKey(roundNum, state) {
  assertEquals(
      'Incorrect state for test ' + testData.name +
          ' after adding round key in round ' + roundNum,
      testData[roundNum].k_add, encodeState(state));
}

function encodeHex(arr) {
  const str = [];

  for (let i = 0; i < arr.length; i++) {
    str.push(encodeByte(arr[i]));
  }

  return str.join('');
}

function encodeState(state) {
  const s = [];

  for (let c = 0; c < 4; c++) {
    for (let r = 0; r < 4; r++) {
      s.push(encodeByte(state[r][c]));
    }
  }

  return s.join('');
}

function encodeKey(key, round) {
  const s = [];

  for (let r = round * 4; r < (round * 4 + 4); r++) {
    for (let c = 0; c < 4; c++) {
      s.push(encodeByte(key[r][c]));
    }
  }

  return s.join('');
}

function encodeByte(val) {
  val = Number(val).toString(16);

  if (val.length == 1) {
    val = `0${val}`;
  }

  return val;
}

const v128 = [];
(function v128_init() {
  for (let i = 0; i <= 10; i++) v128[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v128.name = '128';
  v128[0].input = '00112233445566778899aabbccddeeff';
  v128[0].k_sch = '000102030405060708090a0b0c0d0e0f';
  v128[1].start = '00102030405060708090a0b0c0d0e0f0';
  v128[1].s_box = '63cab7040953d051cd60e0e7ba70e18c';
  v128[1].s_row = '6353e08c0960e104cd70b751bacad0e7';
  v128[1].m_col = '5f72641557f5bc92f7be3b291db9f91a';
  v128[1].k_sch = 'd6aa74fdd2af72fadaa678f1d6ab76fe';
  v128[2].start = '89d810e8855ace682d1843d8cb128fe4';
  v128[2].s_box = 'a761ca9b97be8b45d8ad1a611fc97369';
  v128[2].s_row = 'a7be1a6997ad739bd8c9ca451f618b61';
  v128[2].m_col = 'ff87968431d86a51645151fa773ad009';
  v128[2].k_sch = 'b692cf0b643dbdf1be9bc5006830b3fe';
  v128[3].start = '4915598f55e5d7a0daca94fa1f0a63f7';
  v128[3].s_box = '3b59cb73fcd90ee05774222dc067fb68';
  v128[3].s_row = '3bd92268fc74fb735767cbe0c0590e2d';
  v128[3].m_col = '4c9c1e66f771f0762c3f868e534df256';
  v128[3].k_sch = 'b6ff744ed2c2c9bf6c590cbf0469bf41';
  v128[4].start = 'fa636a2825b339c940668a3157244d17';
  v128[4].s_box = '2dfb02343f6d12dd09337ec75b36e3f0';
  v128[4].s_row = '2d6d7ef03f33e334093602dd5bfb12c7';
  v128[4].m_col = '6385b79ffc538df997be478e7547d691';
  v128[4].k_sch = '47f7f7bc95353e03f96c32bcfd058dfd';
  v128[5].start = '247240236966b3fa6ed2753288425b6c';
  v128[5].s_box = '36400926f9336d2d9fb59d23c42c3950';
  v128[5].s_row = '36339d50f9b539269f2c092dc4406d23';
  v128[5].m_col = 'f4bcd45432e554d075f1d6c51dd03b3c';
  v128[5].k_sch = '3caaa3e8a99f9deb50f3af57adf622aa';
  v128[6].start = 'c81677bc9b7ac93b25027992b0261996';
  v128[6].s_box = 'e847f56514dadde23f77b64fe7f7d490';
  v128[6].s_row = 'e8dab6901477d4653ff7f5e2e747dd4f';
  v128[6].m_col = '9816ee7400f87f556b2c049c8e5ad036';
  v128[6].k_sch = '5e390f7df7a69296a7553dc10aa31f6b';
  v128[7].start = 'c62fe109f75eedc3cc79395d84f9cf5d';
  v128[7].s_box = 'b415f8016858552e4bb6124c5f998a4c';
  v128[7].s_row = 'b458124c68b68a014b99f82e5f15554c';
  v128[7].m_col = 'c57e1c159a9bd286f05f4be098c63439';
  v128[7].k_sch = '14f9701ae35fe28c440adf4d4ea9c026';
  v128[8].start = 'd1876c0f79c4300ab45594add66ff41f';
  v128[8].s_box = '3e175076b61c04678dfc2295f6a8bfc0';
  v128[8].s_row = '3e1c22c0b6fcbf768da85067f6170495';
  v128[8].m_col = 'baa03de7a1f9b56ed5512cba5f414d23';
  v128[8].k_sch = '47438735a41c65b9e016baf4aebf7ad2';
  v128[9].start = 'fde3bad205e5d0d73547964ef1fe37f1';
  v128[9].s_box = '5411f4b56bd9700e96a0902fa1bb9aa1';
  v128[9].s_row = '54d990a16ba09ab596bbf40ea111702f';
  v128[9].m_col = 'e9f74eec023020f61bf2ccf2353c21c7';
  v128[9].k_sch = '549932d1f08557681093ed9cbe2c974e';
  v128[10].start = 'bd6e7c3df2b5779e0b61216e8b10b689';
  v128[10].s_box = '7a9f102789d5f50b2beffd9f3dca4ea7';
  v128[10].s_row = '7ad5fda789ef4e272bca100b3d9ff59f';
  v128[10].k_sch = '13111d7fe3944a17f307a78b4d2b30c5';
  v128[10].output = '69c4e0d86a7b0430d8cdb78070b4c55a';
})();

const v128d = [];
(function v128d_init() {
  for (let i = 0; i <= 10; i++) v128d[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v128d.name = '128d';
  v128d[0].input = '69c4e0d86a7b0430d8cdb78070b4c55a';
  v128d[0].k_sch = '13111d7fe3944a17f307a78b4d2b30c5';
  v128d[1].start = '7ad5fda789ef4e272bca100b3d9ff59f';
  v128d[1].s_row = '7a9f102789d5f50b2beffd9f3dca4ea7';
  v128d[1].s_box = 'bd6e7c3df2b5779e0b61216e8b10b689';
  v128d[1].k_sch = '549932d1f08557681093ed9cbe2c974e';
  v128d[1].k_add = 'e9f74eec023020f61bf2ccf2353c21c7';
  v128d[2].start = '54d990a16ba09ab596bbf40ea111702f';
  v128d[2].s_row = '5411f4b56bd9700e96a0902fa1bb9aa1';
  v128d[2].s_box = 'fde3bad205e5d0d73547964ef1fe37f1';
  v128d[2].k_sch = '47438735a41c65b9e016baf4aebf7ad2';
  v128d[2].k_add = 'baa03de7a1f9b56ed5512cba5f414d23';
  v128d[3].start = '3e1c22c0b6fcbf768da85067f6170495';
  v128d[3].s_row = '3e175076b61c04678dfc2295f6a8bfc0';
  v128d[3].s_box = 'd1876c0f79c4300ab45594add66ff41f';
  v128d[3].k_sch = '14f9701ae35fe28c440adf4d4ea9c026';
  v128d[3].k_add = 'c57e1c159a9bd286f05f4be098c63439';
  v128d[4].start = 'b458124c68b68a014b99f82e5f15554c';
  v128d[4].s_row = 'b415f8016858552e4bb6124c5f998a4c';
  v128d[4].s_box = 'c62fe109f75eedc3cc79395d84f9cf5d';
  v128d[4].k_sch = '5e390f7df7a69296a7553dc10aa31f6b';
  v128d[4].k_add = '9816ee7400f87f556b2c049c8e5ad036';
  v128d[5].start = 'e8dab6901477d4653ff7f5e2e747dd4f';
  v128d[5].s_row = 'e847f56514dadde23f77b64fe7f7d490';
  v128d[5].s_box = 'c81677bc9b7ac93b25027992b0261996';
  v128d[5].k_sch = '3caaa3e8a99f9deb50f3af57adf622aa';
  v128d[5].k_add = 'f4bcd45432e554d075f1d6c51dd03b3c';
  v128d[6].start = '36339d50f9b539269f2c092dc4406d23';
  v128d[6].s_row = '36400926f9336d2d9fb59d23c42c3950';
  v128d[6].s_box = '247240236966b3fa6ed2753288425b6c';
  v128d[6].k_sch = '47f7f7bc95353e03f96c32bcfd058dfd';
  v128d[6].k_add = '6385b79ffc538df997be478e7547d691';
  v128d[7].start = '2d6d7ef03f33e334093602dd5bfb12c7';
  v128d[7].s_row = '2dfb02343f6d12dd09337ec75b36e3f0';
  v128d[7].s_box = 'fa636a2825b339c940668a3157244d17';
  v128d[7].k_sch = 'b6ff744ed2c2c9bf6c590cbf0469bf41';
  v128d[7].k_add = '4c9c1e66f771f0762c3f868e534df256';
  v128d[8].start = '3bd92268fc74fb735767cbe0c0590e2d';
  v128d[8].s_row = '3b59cb73fcd90ee05774222dc067fb68';
  v128d[8].s_box = '4915598f55e5d7a0daca94fa1f0a63f7';
  v128d[8].k_sch = 'b692cf0b643dbdf1be9bc5006830b3fe';
  v128d[8].k_add = 'ff87968431d86a51645151fa773ad009';
  v128d[9].start = 'a7be1a6997ad739bd8c9ca451f618b61';
  v128d[9].s_row = 'a761ca9b97be8b45d8ad1a611fc97369';
  v128d[9].s_box = '89d810e8855ace682d1843d8cb128fe4';
  v128d[9].k_sch = 'd6aa74fdd2af72fadaa678f1d6ab76fe';
  v128d[9].k_add = '5f72641557f5bc92f7be3b291db9f91a';
  v128d[10].start = '6353e08c0960e104cd70b751bacad0e7';
  v128d[10].s_row = '63cab7040953d051cd60e0e7ba70e18c';
  v128d[10].s_box = '00102030405060708090a0b0c0d0e0f0';
  v128d[10].k_sch = '000102030405060708090a0b0c0d0e0f';
  v128d[10].output = '00112233445566778899aabbccddeeff';
})();

const v192 = [];
(function v192_init() {
  for (let i = 0; i <= 12; i++) v192[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v192.name = '192';
  v192[0].input = '00112233445566778899aabbccddeeff';
  v192[0].k_sch = '000102030405060708090a0b0c0d0e0f';
  v192[1].start = '00102030405060708090a0b0c0d0e0f0';
  v192[1].s_box = '63cab7040953d051cd60e0e7ba70e18c';
  v192[1].s_row = '6353e08c0960e104cd70b751bacad0e7';
  v192[1].m_col = '5f72641557f5bc92f7be3b291db9f91a';
  v192[1].k_sch = '10111213141516175846f2f95c43f4fe';
  v192[2].start = '4f63760643e0aa85aff8c9d041fa0de4';
  v192[2].s_box = '84fb386f1ae1ac977941dd70832dd769';
  v192[2].s_row = '84e1dd691a41d76f792d389783fbac70';
  v192[2].m_col = '9f487f794f955f662afc86abd7f1ab29';
  v192[2].k_sch = '544afef55847f0fa4856e2e95c43f4fe';
  v192[3].start = 'cb02818c17d2af9c62aa64428bb25fd7';
  v192[3].s_box = '1f770c64f0b579deaaac432c3d37cf0e';
  v192[3].s_row = '1fb5430ef0accf64aa370cde3d77792c';
  v192[3].m_col = 'b7a53ecbbf9d75a0c40efc79b674cc11';
  v192[3].k_sch = '40f949b31cbabd4d48f043b810b7b342';
  v192[4].start = 'f75c7778a327c8ed8cfebfc1a6c37f53';
  v192[4].s_box = '684af5bc0acce85564bb0878242ed2ed';
  v192[4].s_row = '68cc08ed0abbd2bc642ef555244ae878';
  v192[4].m_col = '7a1e98bdacb6d1141a6944dd06eb2d3e';
  v192[4].k_sch = '58e151ab04a2a5557effb5416245080c';
  v192[5].start = '22ffc916a81474416496f19c64ae2532';
  v192[5].s_box = '9316dd47c2fa92834390a1de43e43f23';
  v192[5].s_row = '93faa123c2903f4743e4dd83431692de';
  v192[5].m_col = 'aaa755b34cffe57cef6f98e1f01c13e6';
  v192[5].k_sch = '2ab54bb43a02f8f662e3a95d66410c08';
  v192[6].start = '80121e0776fd1d8a8d8c31bc965d1fee';
  v192[6].s_box = 'cdc972c53854a47e5d64c765904cc028';
  v192[6].s_row = 'cd54c7283864c0c55d4c727e90c9a465';
  v192[6].m_col = '921f748fd96e937d622d7725ba8ba50c';
  v192[6].k_sch = 'f501857297448d7ebdf1c6ca87f33e3c';
  v192[7].start = '671ef1fd4e2a1e03dfdcb1ef3d789b30';
  v192[7].s_box = '8572a1542fe5727b9e86c8df27bc1404';
  v192[7].s_row = '85e5c8042f8614549ebca17b277272df';
  v192[7].m_col = 'e913e7b18f507d4b227ef652758acbcc';
  v192[7].k_sch = 'e510976183519b6934157c9ea351f1e0';
  v192[8].start = '0c0370d00c01e622166b8accd6db3a2c';
  v192[8].s_box = 'fe7b5170fe7c8e93477f7e4bf6b98071';
  v192[8].s_row = 'fe7c7e71fe7f807047b95193f67b8e4b';
  v192[8].m_col = '6cf5edf996eb0a069c4ef21cbfc25762';
  v192[8].k_sch = '1ea0372a995309167c439e77ff12051e';
  v192[9].start = '7255dad30fb80310e00d6c6b40d0527c';
  v192[9].s_box = '40fc5766766c7bcae1d7507f09700010';
  v192[9].s_row = '406c501076d70066e17057ca09fc7b7f';
  v192[9].m_col = '7478bcdce8a50b81d4327a9009188262';
  v192[9].k_sch = 'dd7e0e887e2fff68608fc842f9dcc154';
  v192[10].start = 'a906b254968af4e9b4bdb2d2f0c44336';
  v192[10].s_box = 'd36f3720907ebf1e8d7a37b58c1c1a05';
  v192[10].s_row = 'd37e3705907a1a208d1c371e8c6fbfb5';
  v192[10].m_col = '0d73cc2d8f6abe8b0cf2dd9bb83d422e';
  v192[10].k_sch = '859f5f237a8d5a3dc0c02952beefd63a';
  v192[11].start = '88ec930ef5e7e4b6cc32f4c906d29414';
  v192[11].s_box = 'c4cedcabe694694e4b23bfdd6fb522fa';
  v192[11].s_row = 'c494bffae62322ab4bb5dc4e6fce69dd';
  v192[11].m_col = '71d720933b6d677dc00b8f28238e0fb7';
  v192[11].k_sch = 'de601e7827bcdf2ca223800fd8aeda32';
  v192[12].start = 'afb73eeb1cd1b85162280f27fb20d585';
  v192[12].s_box = '79a9b2e99c3e6cd1aa3476cc0fb70397';
  v192[12].s_row = '793e76979c3403e9aab7b2d10fa96ccc';
  v192[12].k_sch = 'a4970a331a78dc09c418c271e3a41d5d';
  v192[12].output = 'dda97ca4864cdfe06eaf70a0ec0d7191';
})();

const v192d = [];
(function v192d_init() {
  for (let i = 0; i <= 12; i++) v192d[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v192d.name = '192d';
  v192d[0].input = 'dda97ca4864cdfe06eaf70a0ec0d7191';
  v192d[0].k_sch = 'a4970a331a78dc09c418c271e3a41d5d';
  v192d[1].start = '793e76979c3403e9aab7b2d10fa96ccc';
  v192d[1].s_row = '79a9b2e99c3e6cd1aa3476cc0fb70397';
  v192d[1].s_box = 'afb73eeb1cd1b85162280f27fb20d585';
  v192d[1].k_sch = 'de601e7827bcdf2ca223800fd8aeda32';
  v192d[1].k_add = '71d720933b6d677dc00b8f28238e0fb7';
  v192d[2].start = 'c494bffae62322ab4bb5dc4e6fce69dd';
  v192d[2].s_row = 'c4cedcabe694694e4b23bfdd6fb522fa';
  v192d[2].s_box = '88ec930ef5e7e4b6cc32f4c906d29414';
  v192d[2].k_sch = '859f5f237a8d5a3dc0c02952beefd63a';
  v192d[2].k_add = '0d73cc2d8f6abe8b0cf2dd9bb83d422e';
  v192d[3].start = 'd37e3705907a1a208d1c371e8c6fbfb5';
  v192d[3].s_row = 'd36f3720907ebf1e8d7a37b58c1c1a05';
  v192d[3].s_box = 'a906b254968af4e9b4bdb2d2f0c44336';
  v192d[3].k_sch = 'dd7e0e887e2fff68608fc842f9dcc154';
  v192d[3].k_add = '7478bcdce8a50b81d4327a9009188262';
  v192d[4].start = '406c501076d70066e17057ca09fc7b7f';
  v192d[4].s_row = '40fc5766766c7bcae1d7507f09700010';
  v192d[4].s_box = '7255dad30fb80310e00d6c6b40d0527c';
  v192d[4].k_sch = '1ea0372a995309167c439e77ff12051e';
  v192d[4].k_add = '6cf5edf996eb0a069c4ef21cbfc25762';
  v192d[5].start = 'fe7c7e71fe7f807047b95193f67b8e4b';
  v192d[5].s_row = 'fe7b5170fe7c8e93477f7e4bf6b98071';
  v192d[5].s_box = '0c0370d00c01e622166b8accd6db3a2c';
  v192d[5].k_sch = 'e510976183519b6934157c9ea351f1e0';
  v192d[5].k_add = 'e913e7b18f507d4b227ef652758acbcc';
  v192d[6].start = '85e5c8042f8614549ebca17b277272df';
  v192d[6].s_row = '8572a1542fe5727b9e86c8df27bc1404';
  v192d[6].s_box = '671ef1fd4e2a1e03dfdcb1ef3d789b30';
  v192d[6].k_sch = 'f501857297448d7ebdf1c6ca87f33e3c';
  v192d[6].k_add = '921f748fd96e937d622d7725ba8ba50c';
  v192d[7].start = 'cd54c7283864c0c55d4c727e90c9a465';
  v192d[7].s_row = 'cdc972c53854a47e5d64c765904cc028';
  v192d[7].s_box = '80121e0776fd1d8a8d8c31bc965d1fee';
  v192d[7].k_sch = '2ab54bb43a02f8f662e3a95d66410c08';
  v192d[7].k_add = 'aaa755b34cffe57cef6f98e1f01c13e6';
  v192d[8].start = '93faa123c2903f4743e4dd83431692de';
  v192d[8].s_row = '9316dd47c2fa92834390a1de43e43f23';
  v192d[8].s_box = '22ffc916a81474416496f19c64ae2532';
  v192d[8].k_sch = '58e151ab04a2a5557effb5416245080c';
  v192d[8].k_add = '7a1e98bdacb6d1141a6944dd06eb2d3e';
  v192d[9].start = '68cc08ed0abbd2bc642ef555244ae878';
  v192d[9].s_row = '684af5bc0acce85564bb0878242ed2ed';
  v192d[9].s_box = 'f75c7778a327c8ed8cfebfc1a6c37f53';
  v192d[9].k_sch = '40f949b31cbabd4d48f043b810b7b342';
  v192d[9].k_add = 'b7a53ecbbf9d75a0c40efc79b674cc11';
  v192d[10].start = '1fb5430ef0accf64aa370cde3d77792c';
  v192d[10].s_row = '1f770c64f0b579deaaac432c3d37cf0e';
  v192d[10].s_box = 'cb02818c17d2af9c62aa64428bb25fd7';
  v192d[10].k_sch = '544afef55847f0fa4856e2e95c43f4fe';
  v192d[10].k_add = '9f487f794f955f662afc86abd7f1ab29';
  v192d[11].start = '84e1dd691a41d76f792d389783fbac70';
  v192d[11].s_row = '84fb386f1ae1ac977941dd70832dd769';
  v192d[11].s_box = '4f63760643e0aa85aff8c9d041fa0de4';
  v192d[11].k_sch = '10111213141516175846f2f95c43f4fe';
  v192d[11].k_add = '5f72641557f5bc92f7be3b291db9f91a';
  v192d[12].start = '6353e08c0960e104cd70b751bacad0e7';
  v192d[12].s_row = '63cab7040953d051cd60e0e7ba70e18c';
  v192d[12].s_box = '00102030405060708090a0b0c0d0e0f0';
  v192d[12].k_sch = '000102030405060708090a0b0c0d0e0f';
  v192d[12].output = '00112233445566778899aabbccddeeff';
})();

const v256 = [];
(function v256_init() {
  for (let i = 0; i <= 14; i++) v256[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v256.name = '256';
  v256[0].input = '00112233445566778899aabbccddeeff';
  v256[0].k_sch = '000102030405060708090a0b0c0d0e0f';
  v256[1].start = '00102030405060708090a0b0c0d0e0f0';
  v256[1].s_box = '63cab7040953d051cd60e0e7ba70e18c';
  v256[1].s_row = '6353e08c0960e104cd70b751bacad0e7';
  v256[1].m_col = '5f72641557f5bc92f7be3b291db9f91a';
  v256[1].k_sch = '101112131415161718191a1b1c1d1e1f';
  v256[2].start = '4f63760643e0aa85efa7213201a4e705';
  v256[2].s_box = '84fb386f1ae1ac97df5cfd237c49946b';
  v256[2].s_row = '84e1fd6b1a5c946fdf4938977cfbac23';
  v256[2].m_col = 'bd2a395d2b6ac438d192443e615da195';
  v256[2].k_sch = 'a573c29fa176c498a97fce93a572c09c';
  v256[3].start = '1859fbc28a1c00a078ed8aadc42f6109';
  v256[3].s_box = 'adcb0f257e9c63e0bc557e951c15ef01';
  v256[3].s_row = 'ad9c7e017e55ef25bc150fe01ccb6395';
  v256[3].m_col = '810dce0cc9db8172b3678c1e88a1b5bd';
  v256[3].k_sch = '1651a8cd0244beda1a5da4c10640bade';
  v256[4].start = '975c66c1cb9f3fa8a93a28df8ee10f63';
  v256[4].s_box = '884a33781fdb75c2d380349e19f876fb';
  v256[4].s_row = '88db34fb1f807678d3f833c2194a759e';
  v256[4].m_col = 'b2822d81abe6fb275faf103a078c0033';
  v256[4].k_sch = 'ae87dff00ff11b68a68ed5fb03fc1567';
  v256[5].start = '1c05f271a417e04ff921c5c104701554';
  v256[5].s_box = '9c6b89a349f0e18499fda678f2515920';
  v256[5].s_row = '9cf0a62049fd59a399518984f26be178';
  v256[5].m_col = 'aeb65ba974e0f822d73f567bdb64c877';
  v256[5].k_sch = '6de1f1486fa54f9275f8eb5373b8518d';
  v256[6].start = 'c357aae11b45b7b0a2c7bd28a8dc99fa';
  v256[6].s_box = '2e5bacf8af6ea9e73ac67a34c286ee2d';
  v256[6].s_row = '2e6e7a2dafc6eef83a86ace7c25ba934';
  v256[6].m_col = 'b951c33c02e9bd29ae25cdb1efa08cc7';
  v256[6].k_sch = 'c656827fc9a799176f294cec6cd5598b';
  v256[7].start = '7f074143cb4e243ec10c815d8375d54c';
  v256[7].s_box = 'd2c5831a1f2f36b278fe0c4cec9d0329';
  v256[7].s_row = 'd22f0c291ffe031a789d83b2ecc5364c';
  v256[7].m_col = 'ebb19e1c3ee7c9e87d7535e9ed6b9144';
  v256[7].k_sch = '3de23a75524775e727bf9eb45407cf39';
  v256[8].start = 'd653a4696ca0bc0f5acaab5db96c5e7d';
  v256[8].s_box = 'f6ed49f950e06576be74624c565058ff';
  v256[8].s_row = 'f6e062ff507458f9be50497656ed654c';
  v256[8].m_col = '5174c8669da98435a8b3e62ca974a5ea';
  v256[8].k_sch = '0bdc905fc27b0948ad5245a4c1871c2f';
  v256[9].start = '5aa858395fd28d7d05e1a38868f3b9c5';
  v256[9].s_box = 'bec26a12cfb55dff6bf80ac4450d56a6';
  v256[9].s_row = 'beb50aa6cff856126b0d6aff45c25dc4';
  v256[9].m_col = '0f77ee31d2ccadc05430a83f4ef96ac3';
  v256[9].k_sch = '45f5a66017b2d387300d4d33640a820a';
  v256[10].start = '4a824851c57e7e47643de50c2af3e8c9';
  v256[10].s_box = 'd61352d1a6f3f3a04327d9fee50d9bdd';
  v256[10].s_row = 'd6f3d9dda6279bd1430d52a0e513f3fe';
  v256[10].m_col = 'bd86f0ea748fc4f4630f11c1e9331233';
  v256[10].k_sch = '7ccff71cbeb4fe5413e6bbf0d261a7df';
  v256[11].start = 'c14907f6ca3b3aa070e9aa313b52b5ec';
  v256[11].s_box = '783bc54274e280e0511eacc7e200d5ce';
  v256[11].s_row = '78e2acce741ed5425100c5e0e23b80c7';
  v256[11].m_col = 'af8690415d6e1dd387e5fbedd5c89013';
  v256[11].k_sch = 'f01afafee7a82979d7a5644ab3afe640';
  v256[12].start = '5f9c6abfbac634aa50409fa766677653';
  v256[12].s_box = 'cfde0208f4b418ac5309db5c338538ed';
  v256[12].s_row = 'cfb4dbedf4093808538502ac33de185c';
  v256[12].m_col = '7427fae4d8a695269ce83d315be0392b';
  v256[12].k_sch = '2541fe719bf500258813bbd55a721c0a';
  v256[13].start = '516604954353950314fb86e401922521';
  v256[13].s_box = 'd133f22a1aed2a7bfa0f44697c4f3ffd';
  v256[13].s_row = 'd1ed44fd1a0f3f2afa4ff27b7c332a69';
  v256[13].m_col = '2c21a820306f154ab712c75eee0da04f';
  v256[13].k_sch = '4e5a6699a9f24fe07e572baacdf8cdea';
  v256[14].start = '627bceb9999d5aaac945ecf423f56da5';
  v256[14].s_box = 'aa218b56ee5ebeacdd6ecebf26e63c06';
  v256[14].s_row = 'aa5ece06ee6e3c56dde68bac2621bebf';
  v256[14].k_sch = '24fc79ccbf0979e9371ac23c6d68de36';
  v256[14].output = '8ea2b7ca516745bfeafc49904b496089';
})();

const v256d = [];
(function v256d_init() {
  for (let i = 0; i <= 14; i++) v256d[i] = {};
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  v256d.name = '256d';
  v256d[0].input = '8ea2b7ca516745bfeafc49904b496089';
  v256d[0].k_sch = '24fc79ccbf0979e9371ac23c6d68de36';
  v256d[1].start = 'aa5ece06ee6e3c56dde68bac2621bebf';
  v256d[1].s_row = 'aa218b56ee5ebeacdd6ecebf26e63c06';
  v256d[1].s_box = '627bceb9999d5aaac945ecf423f56da5';
  v256d[1].k_sch = '4e5a6699a9f24fe07e572baacdf8cdea';
  v256d[1].k_add = '2c21a820306f154ab712c75eee0da04f';
  v256d[2].start = 'd1ed44fd1a0f3f2afa4ff27b7c332a69';
  v256d[2].s_row = 'd133f22a1aed2a7bfa0f44697c4f3ffd';
  v256d[2].s_box = '516604954353950314fb86e401922521';
  v256d[2].k_sch = '2541fe719bf500258813bbd55a721c0a';
  v256d[2].k_add = '7427fae4d8a695269ce83d315be0392b';
  v256d[3].start = 'cfb4dbedf4093808538502ac33de185c';
  v256d[3].s_row = 'cfde0208f4b418ac5309db5c338538ed';
  v256d[3].s_box = '5f9c6abfbac634aa50409fa766677653';
  v256d[3].k_sch = 'f01afafee7a82979d7a5644ab3afe640';
  v256d[3].k_add = 'af8690415d6e1dd387e5fbedd5c89013';
  v256d[4].start = '78e2acce741ed5425100c5e0e23b80c7';
  v256d[4].s_row = '783bc54274e280e0511eacc7e200d5ce';
  v256d[4].s_box = 'c14907f6ca3b3aa070e9aa313b52b5ec';
  v256d[4].k_sch = '7ccff71cbeb4fe5413e6bbf0d261a7df';
  v256d[4].k_add = 'bd86f0ea748fc4f4630f11c1e9331233';
  v256d[5].start = 'd6f3d9dda6279bd1430d52a0e513f3fe';
  v256d[5].s_row = 'd61352d1a6f3f3a04327d9fee50d9bdd';
  v256d[5].s_box = '4a824851c57e7e47643de50c2af3e8c9';
  v256d[5].k_sch = '45f5a66017b2d387300d4d33640a820a';
  v256d[5].k_add = '0f77ee31d2ccadc05430a83f4ef96ac3';
  v256d[6].start = 'beb50aa6cff856126b0d6aff45c25dc4';
  v256d[6].s_row = 'bec26a12cfb55dff6bf80ac4450d56a6';
  v256d[6].s_box = '5aa858395fd28d7d05e1a38868f3b9c5';
  v256d[6].k_sch = '0bdc905fc27b0948ad5245a4c1871c2f';
  v256d[6].k_add = '5174c8669da98435a8b3e62ca974a5ea';
  v256d[7].start = 'f6e062ff507458f9be50497656ed654c';
  v256d[7].s_row = 'f6ed49f950e06576be74624c565058ff';
  v256d[7].s_box = 'd653a4696ca0bc0f5acaab5db96c5e7d';
  v256d[7].k_sch = '3de23a75524775e727bf9eb45407cf39';
  v256d[7].k_add = 'ebb19e1c3ee7c9e87d7535e9ed6b9144';
  v256d[8].start = 'd22f0c291ffe031a789d83b2ecc5364c';
  v256d[8].s_row = 'd2c5831a1f2f36b278fe0c4cec9d0329';
  v256d[8].s_box = '7f074143cb4e243ec10c815d8375d54c';
  v256d[8].k_sch = 'c656827fc9a799176f294cec6cd5598b';
  v256d[8].k_add = 'b951c33c02e9bd29ae25cdb1efa08cc7';
  v256d[9].start = '2e6e7a2dafc6eef83a86ace7c25ba934';
  v256d[9].s_row = '2e5bacf8af6ea9e73ac67a34c286ee2d';
  v256d[9].s_box = 'c357aae11b45b7b0a2c7bd28a8dc99fa';
  v256d[9].k_sch = '6de1f1486fa54f9275f8eb5373b8518d';
  v256d[9].k_add = 'aeb65ba974e0f822d73f567bdb64c877';
  v256d[10].start = '9cf0a62049fd59a399518984f26be178';
  v256d[10].s_row = '9c6b89a349f0e18499fda678f2515920';
  v256d[10].s_box = '1c05f271a417e04ff921c5c104701554';
  v256d[10].k_sch = 'ae87dff00ff11b68a68ed5fb03fc1567';
  v256d[10].k_add = 'b2822d81abe6fb275faf103a078c0033';
  v256d[11].start = '88db34fb1f807678d3f833c2194a759e';
  v256d[11].s_row = '884a33781fdb75c2d380349e19f876fb';
  v256d[11].s_box = '975c66c1cb9f3fa8a93a28df8ee10f63';
  v256d[11].k_sch = '1651a8cd0244beda1a5da4c10640bade';
  v256d[11].k_add = '810dce0cc9db8172b3678c1e88a1b5bd';
  v256d[12].start = 'ad9c7e017e55ef25bc150fe01ccb6395';
  v256d[12].s_row = 'adcb0f257e9c63e0bc557e951c15ef01';
  v256d[12].s_box = '1859fbc28a1c00a078ed8aadc42f6109';
  v256d[12].k_sch = 'a573c29fa176c498a97fce93a572c09c';
  v256d[12].k_add = 'bd2a395d2b6ac438d192443e615da195';
  v256d[13].start = '84e1fd6b1a5c946fdf4938977cfbac23';
  v256d[13].s_row = '84fb386f1ae1ac97df5cfd237c49946b';
  v256d[13].s_box = '4f63760643e0aa85efa7213201a4e705';
  v256d[13].k_sch = '101112131415161718191a1b1c1d1e1f';
  v256d[13].k_add = '5f72641557f5bc92f7be3b291db9f91a';
  v256d[14].start = '6353e08c0960e104cd70b751bacad0e7';
  v256d[14].s_row = '63cab7040953d051cd60e0e7ba70e18c';
  v256d[14].s_box = '00102030405060708090a0b0c0d0e0f0';
  v256d[14].k_sch = '000102030405060708090a0b0c0d0e0f';
  v256d[14].output = '00112233445566778899aabbccddeeff';
})();
testSuite({
  test128() {
    doTest(
        '000102030405060708090a0b0c0d0e0f', '00112233445566778899aabbccddeeff',
        v128, true /* encrypt */);
  },

  test192() {
    doTest(
        '000102030405060708090a0b0c0d0e0f1011121314151617',
        '00112233445566778899aabbccddeeff', v192, true /* encrypt */);
  },

  test256() {
    doTest(
        '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
        '00112233445566778899aabbccddeeff', v256, true /* encrypt */);
  },

  test128d() {
    doTest(
        '000102030405060708090a0b0c0d0e0f', '69c4e0d86a7b0430d8cdb78070b4c55a',
        v128d, false /* decrypt */);
  },

  test192d() {
    doTest(
        '000102030405060708090a0b0c0d0e0f1011121314151617',
        'dda97ca4864cdfe06eaf70a0ec0d7191', v192d, false /* decrypt */);
  },

  test256d() {
    doTest(
        '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
        '8ea2b7ca516745bfeafc49904b496089', v256d, false /* decrypt */);
  },
});
