import {NonAssociatedWebTestControlHostRemote} from '/gen/content/web_test/common/web_test.mojom.m.js';
import {mojo} from '/gen/mojo/public/js/bindings.js';
import {ByteString} from '/gen/mojo/public/mojom/base/byte_string.mojom.m.js';
import {LCPCriticalPathPredictorNavigationTimeHint} from '/gen/third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.m.js';

export async function setupLCPTest(lcp_locator_protos) {
  if (window.location.search != '?start') {
    const hint = new LCPCriticalPathPredictorNavigationTimeHint();
    // All fields are non-nullable.
    hint.lcpElementLocators = [];
    hint.lcpInfluencerScripts = [];
    hint.fetchedFonts = [];
    hint.preconnectOrigins = [];

    let getLCPBytes = async function(proto_file) {
      const resp = await fetch(
          '/gen/third_party/blink/renderer/core/lcp_critical_path_predictor/test_proto/' +
          proto_file);
      console.assert(resp.status == 200, `${resp.url} is not found.`);
      const bytes = new ByteString;
      bytes.data = new Uint8Array(await resp.arrayBuffer());
      return bytes;
    };

    if (lcp_locator_protos) {
      for (let proto of lcp_locator_protos) {
        hint.lcpElementLocators.push(await getLCPBytes(proto));
      }
    }

    const web_test_control_host_remote =
        new NonAssociatedWebTestControlHostRemote();
    web_test_control_host_remote.$.bindNewPipeAndPassReceiver().bindInBrowser(
        'process');
    web_test_control_host_remote.setLCPPNavigationHint(hint);

    window.location.search = '?start';
  }
}
