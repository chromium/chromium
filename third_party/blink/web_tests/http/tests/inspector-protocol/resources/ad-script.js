// This script is marked as an ad-script in the test.
window.insertAdEntry = async () => {
  await navigation.navigate('#ad-entry', {history: 'replace'}).committed;
  await navigation.navigate('#ad-pushed').committed;
};
