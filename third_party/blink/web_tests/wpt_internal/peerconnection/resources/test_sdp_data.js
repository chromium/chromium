// Hardcoded SDP Description and Candidates that are used for tests.
const LOCAL_ADDRESS_CANDIDATE = 'a=candidate:1 1 UDP 2130706431 127.0.0.1 12345 typ host generation 0';

const REMOTE_DESCRIPTION_WITH_LOCAL_ADDRESS = `v=0
o=- 3988109818200882900 2 IN IP4 127.0.0.1
s=-
t=0 0
a=msid-semantic: WMS
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=mid:0
a=ice-ufrag:someRemoteUfrag
a=ice-pwd:someRemotePasswordGeneratedString
a=sctpmap:5000 webrtc-datachannel 1024
a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00
${LOCAL_ADDRESS_CANDIDATE}
`;

const REMOTE_DESCRIPTION_WITH_NO_CANDIDATE = `v=0
o=- 3988109818200882900 2 IN IP4 127.0.0.1
s=-
t=0 0
a=msid-semantic: WMS
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=mid:0
a=ice-ufrag:someRemoteUfrag
a=ice-pwd:someRemotePasswordGeneratedString
a=sctpmap:5000 webrtc-datachannel 1024
a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00
`;

