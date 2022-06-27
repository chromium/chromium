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
