# Authors: 
#   Trevor Perrin
#   Google - handling CertificateRequest.certificate_types
#   Google (adapted by Sam Rushing and Marcelo Fernandez) - NPN support
#   Dimitris Moraitis - Anon ciphersuites
#   Yngve Pettersen (ported by Paul Sokolovsky) - TLS 1.2
#
# See the LICENSE file for legal information regarding use of this file.

"""Classes representing TLS messages."""

from __future__ import division

from .utils.compat import *
from .utils.cryptomath import *
from .errors import *
from .utils.codec import *
from .constants import *
from .x509 import X509
from .x509certchain import X509CertChain
from .utils.tackwrapper import *

def parse_next_protos(b):
    protos = []
    while True:
        if len(b) == 0:
            break
        l = b[0]
        b = b[1:]
        if len(b) < l:
            raise BadNextProtos(len(b))
        protos.append(b[:l])
        b = b[l:]
    return protos

def next_protos_encoded(protocol_list):
    b = bytearray()
    for e in protocol_list:
        if len(e) > 255 or len(e) == 0:
            raise BadNextProtos(len(e))
        b += bytearray( [len(e)] ) + bytearray(e)
    return b

class RecordHeader3(object):
    def __init__(self):
        self.type = 0
        self.version = (0,0)
        self.length = 0
        self.ssl2 = False

    def create(self, version, type, length):
        self.type = type
        self.version = version
        self.length = length
        return self

    def write(self):
        w = Writer()
        w.add(self.type, 1)
        w.add(self.version[0], 1)
        w.add(self.version[1], 1)
        w.add(self.length, 2)
        return w.bytes

    def parse(self, p):
        self.type = p.get(1)
        self.version = (p.get(1), p.get(1))
        self.length = p.get(2)
        self.ssl2 = False
        return self

class RecordHeader2(object):
    def __init__(self):
        self.type = 0
        self.version = (0,0)
        self.length = 0
        self.ssl2 = True

    def parse(self, p):
        if p.get(1)!=128:
            raise SyntaxError()
        self.type = ContentType.handshake
        self.version = (2,0)
        #We don't support 2-byte-length-headers; could be a problem
        self.length = p.get(1)
        return self


class Alert(object):
    def __init__(self):
        self.contentType = ContentType.alert
        self.level = 0
        self.description = 0

    def create(self, description, level=AlertLevel.fatal):
        self.level = level
        self.description = description
        return self

    def parse(self, p):
        p.setLengthCheck(2)
        self.level = p.get(1)
        self.description = p.get(1)
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.add(self.level, 1)
        w.add(self.description, 1)
        return w.bytes


class HandshakeMsg(object):
    def __init__(self, handshakeType):
        self.contentType = ContentType.handshake
        self.handshakeType = handshakeType
    
    def postWrite(self, w):
        headerWriter = Writer()
        headerWriter.add(self.handshakeType, 1)
        headerWriter.add(len(w.bytes), 3)
        return headerWriter.bytes + w.bytes

class ClientHello(HandshakeMsg):
    def __init__(self, ssl2=False):
        HandshakeMsg.__init__(self, HandshakeType.client_hello)
        self.ssl2 = ssl2
        self.client_version = (0,0)
        self.random = bytearray(32)
        self.session_id = bytearray(0)
        self.cipher_suites = []         # a list of 16-bit values
        self.certificate_types = [CertificateType.x509]
        self.compression_methods = []   # a list of 8-bit values
        self.srp_username = None        # a string
        self.tack = False
        self.alpn_protos_advertised = None
        self.supports_npn = False
        self.server_name = bytearray(0)
        self.channel_id = False
        self.extended_master_secret = False
        self.tb_client_params = []
        self.support_signed_cert_timestamps = False
        self.status_request = False
        self.has_supported_versions = False
        self.ri = False
        self.signature_algorithms = []

    def create(self, version, random, session_id, cipher_suites,
               certificate_types=None, srpUsername=None,
               tack=False, alpn_protos_advertised=None,
               supports_npn=False, serverName=None):
        self.client_version = version
        self.random = random
        self.session_id = session_id
        self.cipher_suites = cipher_suites
        self.certificate_types = certificate_types
        self.compression_methods = [0]
        if srpUsername:
            self.srp_username = bytearray(srpUsername, "utf-8")
        self.tack = tack
        self.alpn_protos_advertised = alpn_protos_advertised
        self.supports_npn = supports_npn
        if serverName:
            self.server_name = bytearray(serverName, "utf-8")
        return self

    def parse(self, p):
        if self.ssl2:
            self.client_version = (p.get(1), p.get(1))
            cipherSpecsLength = p.get(2)
            sessionIDLength = p.get(2)
            randomLength = p.get(2)
            self.cipher_suites = p.getFixList(3, cipherSpecsLength//3)
            self.session_id = p.getFixBytes(sessionIDLength)
            self.random = p.getFixBytes(randomLength)
            if len(self.random) < 32:
                zeroBytes = 32-len(self.random)
                self.random = bytearray(zeroBytes) + self.random
            self.compression_methods = [0]#Fake this value

            #We're not doing a stopLengthCheck() for SSLv2, oh well..
        else:
            p.startLengthCheck(3)
            self.client_version = (p.get(1), p.get(1))
            self.random = p.getFixBytes(32)
            self.session_id = p.getVarBytes(1)
            self.cipher_suites = p.getVarList(2, 2)
            self.compression_methods = p.getVarList(1, 1)
            if not p.atLengthCheck():
                totalExtLength = p.get(2)
                soFar = 0
                while soFar != totalExtLength:
                    extType = p.get(2)
                    extLength = p.get(2)
                    index1 = p.index
                    if extType == ExtensionType.srp:
                        self.srp_username = p.getVarBytes(1)
                    elif extType == ExtensionType.cert_type:
                        self.certificate_types = p.getVarList(1, 1)
                    elif extType == ExtensionType.tack:
                        self.tack = True
                    elif extType == ExtensionType.alpn:
                        structLength = p.get(2)
                        if structLength + 2 != extLength:
                            raise SyntaxError()
                        self.alpn_protos_advertised = parse_next_protos(p.getFixBytes(structLength))
                    elif extType == ExtensionType.supports_npn:
                        self.supports_npn = True
                    elif extType == ExtensionType.server_name:
                        serverNameListBytes = p.getFixBytes(extLength)
                        p2 = Parser(serverNameListBytes)
                        p2.startLengthCheck(2)
                        while 1:
                            if p2.atLengthCheck():
                                break # no host_name, oh well
                            name_type = p2.get(1)
                            hostNameBytes = p2.getVarBytes(2)
                            if name_type == NameType.host_name:
                                self.server_name = hostNameBytes
                                break
                    elif extType == ExtensionType.channel_id:
                        self.channel_id = True
                    elif extType == ExtensionType.extended_master_secret:
                        self.extended_master_secret = True
                    elif extType == ExtensionType.token_binding:
                        tokenBindingBytes = p.getFixBytes(extLength)
                        p2 = Parser(tokenBindingBytes)
                        ver_minor = p2.get(1)
                        ver_major = p2.get(1)
                        if (ver_major, ver_minor) >= (0, 13):
                            p2.startLengthCheck(1)
                            while not p2.atLengthCheck():
                                self.tb_client_params.append(p2.get(1))
                    elif extType == ExtensionType.signed_cert_timestamps:
                        if extLength:
                            raise SyntaxError()
                        self.support_signed_cert_timestamps = True
                    elif extType == ExtensionType.status_request:
                        # Extension contents are currently ignored.
                        # According to RFC 6066, this is not strictly forbidden
                        # (although it is suboptimal):
                        # Servers that receive a client hello containing the
                        # "status_request" extension MAY return a suitable
                        # certificate status response to the client along with
                        # their certificate.  If OCSP is requested, they
                        # SHOULD use the information contained in the extension
                        # when selecting an OCSP responder and SHOULD include
                        # request_extensions in the OCSP request.
                        p.getFixBytes(extLength)
                        self.status_request = True
                    elif extType == ExtensionType.renegotiation_info:
                        # We don't support renegotiation, so if we receive this
                        # extension, it should contain a single null byte.
                        if extLength != 1 or p.getFixBytes(extLength)[0] != 0:
                            raise SyntaxError()
                        self.ri = True
                    elif extType == ExtensionType.supported_versions:
                        # Ignore the extension, but make a note of it for
                        # intolerance simulation.
                        self.has_supported_versions = True
                        _ = p.getFixBytes(extLength)
                    elif extType == ExtensionType.signature_algorithms:
                        numBytes = p.get(2)
                        if numBytes + 2 != extLength or numBytes % 2 != 0:
                            raise SyntaxError()
                        for _ in range(numBytes // 2):
                            hashAlg = p.get(1)
                            sigAlg = p.get(1)
                            self.signature_algorithms.append((hashAlg, sigAlg))
                    else:
                        _ = p.getFixBytes(extLength)
                    index2 = p.index
                    if index2 - index1 != extLength:
                        raise SyntaxError("Bad length for extension_data")
                    soFar += 4 + extLength
            if CipherSuite.TLS_EMPTY_RENEGOTIATION_INFO_SCSV in self.cipher_suites:
                self.ri = True
            p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.add(self.client_version[0], 1)
        w.add(self.client_version[1], 1)
        w.addFixSeq(self.random, 1)
        w.addVarSeq(self.session_id, 1, 1)
        w.addVarSeq(self.cipher_suites, 2, 2)
        w.addVarSeq(self.compression_methods, 1, 1)

        w2 = Writer() # For Extensions
        if self.certificate_types and self.certificate_types != \
                [CertificateType.x509]:
            w2.add(ExtensionType.cert_type, 2)
            w2.add(len(self.certificate_types)+1, 2)
            w2.addVarSeq(self.certificate_types, 1, 1)
        if self.srp_username:
            w2.add(ExtensionType.srp, 2)
            w2.add(len(self.srp_username)+1, 2)
            w2.addVarSeq(self.srp_username, 1, 1)
        if self.alpn_protos_advertised is not None:
            encoded_alpn_protos_advertised = next_protos_encoded(self.alpn_protos_advertised)
            w2.add(ExtensionType.alpn, 2)
            w2.add(len(encoded_alpn_protos_advertised) + 2, 2)
            w2.add(len(encoded_alpn_protos_advertised), 2)
            w2.addFixSeq(encoded_alpn_protos_advertised, 1)
        if self.supports_npn:
            w2.add(ExtensionType.supports_npn, 2)
            w2.add(0, 2)
        if self.server_name:
            w2.add(ExtensionType.server_name, 2)
            w2.add(len(self.server_name)+5, 2)
            w2.add(len(self.server_name)+3, 2)            
            w2.add(NameType.host_name, 1)
            w2.addVarSeq(self.server_name, 1, 2) 
        if self.tack:
            w2.add(ExtensionType.tack, 2)
            w2.add(0, 2)
        if len(w2.bytes):
            w.add(len(w2.bytes), 2)
            w.bytes += w2.bytes
        return self.postWrite(w)

class BadNextProtos(Exception):
    def __init__(self, l):
        self.length = l

    def __str__(self):
        return 'Cannot encode a list of next protocols because it contains an element with invalid length %d. Element lengths must be 0 < x < 256' % self.length

class InvalidALPNResponse(Exception):
    def __init__(self, l):
        self.length = l

    def __str__(self):
        return 'ALPN server response protocol list has invalid length %d.  It must be of length one.' % self.length

class ServerHello(HandshakeMsg):
    def __init__(self):
        HandshakeMsg.__init__(self, HandshakeType.server_hello)
        self.server_version = (0,0)
        self.random = bytearray(32)
        self.session_id = bytearray(0)
        self.cipher_suite = 0
        self.certificate_type = CertificateType.x509
        self.compression_method = 0
        self.tackExt = None
        self.alpn_proto_selected = None
        self.next_protos_advertised = None
        self.next_protos = None
        self.channel_id = False
        self.extended_master_secret = False
        self.tb_params = None
        self.signed_cert_timestamps = None
        self.status_request = False
        self.send_ri = False

    def create(self, version, random, session_id, cipher_suite,
               certificate_type, tackExt, alpn_proto_selected,
               next_protos_advertised):
        self.server_version = version
        self.random = random
        self.session_id = session_id
        self.cipher_suite = cipher_suite
        self.certificate_type = certificate_type
        self.compression_method = 0
        self.tackExt = tackExt
        self.alpn_proto_selected = alpn_proto_selected
        self.next_protos_advertised = next_protos_advertised
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        self.server_version = (p.get(1), p.get(1))
        self.random = p.getFixBytes(32)
        self.session_id = p.getVarBytes(1)
        self.cipher_suite = p.get(2)
        self.compression_method = p.get(1)
        if not p.atLengthCheck():
            totalExtLength = p.get(2)
            soFar = 0
            while soFar != totalExtLength:
                extType = p.get(2)
                extLength = p.get(2)
                if extType == ExtensionType.cert_type:
                    if extLength != 1:
                        raise SyntaxError()
                    self.certificate_type = p.get(1)
                elif extType == ExtensionType.tack and tackpyLoaded:
                    self.tackExt = TackExtension(p.getFixBytes(extLength))
                elif extType == ExtensionType.alpn:
                    structLength = p.get(2)
                    if structLength + 2 != extLength:
                        raise SyntaxError()
                    alpn_protos = parse_next_protos(p.getFixBytes(structLength))
                    if len(alpn_protos) != 1:
                        raise InvalidALPNResponse(len(alpn_protos));
                    self.alpn_proto_selected = alpn_protos[0]
                elif extType == ExtensionType.supports_npn:
                    self.next_protos = parse_next_protos(p.getFixBytes(extLength))
                else:
                    p.getFixBytes(extLength)
                soFar += 4 + extLength
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.add(self.server_version[0], 1)
        w.add(self.server_version[1], 1)
        w.addFixSeq(self.random, 1)
        w.addVarSeq(self.session_id, 1, 1)
        w.add(self.cipher_suite, 2)
        w.add(self.compression_method, 1)

        w2 = Writer() # For Extensions
        if self.certificate_type and self.certificate_type != \
                CertificateType.x509:
            w2.add(ExtensionType.cert_type, 2)
            w2.add(1, 2)
            w2.add(self.certificate_type, 1)
        if self.tackExt:
            b = self.tackExt.serialize()
            w2.add(ExtensionType.tack, 2)
            w2.add(len(b), 2)
            w2.bytes += b
        if self.alpn_proto_selected is not None:
            alpn_protos_single_element_list = [self.alpn_proto_selected]
            encoded_alpn_protos_advertised = next_protos_encoded(alpn_protos_single_element_list)
            w2.add(ExtensionType.alpn, 2)
            w2.add(len(encoded_alpn_protos_advertised) + 2, 2)
            w2.add(len(encoded_alpn_protos_advertised), 2)
            w2.addFixSeq(encoded_alpn_protos_advertised, 1)
        if self.next_protos_advertised is not None:
            encoded_next_protos_advertised = next_protos_encoded(self.next_protos_advertised)
            w2.add(ExtensionType.supports_npn, 2)
            w2.add(len(encoded_next_protos_advertised), 2)
            w2.addFixSeq(encoded_next_protos_advertised, 1)
        if self.channel_id:
            w2.add(ExtensionType.channel_id, 2)
            w2.add(0, 2)
        if self.extended_master_secret:
            w2.add(ExtensionType.extended_master_secret, 2)
            w2.add(0, 2)
        if self.tb_params:
            w2.add(ExtensionType.token_binding, 2)
            # length of extension
            w2.add(4, 2)
            # version
            w2.add(0, 1)
            w2.add(13, 1)
            # length of params (defined as variable length <1..2^8-1>, but in
            # this context the server can only send a single value.
            w2.add(1, 1)
            w2.add(self.tb_params, 1)
        if self.signed_cert_timestamps:
            w2.add(ExtensionType.signed_cert_timestamps, 2)
            w2.addVarSeq(bytearray(self.signed_cert_timestamps), 1, 2)
        if self.status_request:
            w2.add(ExtensionType.status_request, 2)
            w2.add(0, 2)
        if self.send_ri:
            w2.add(ExtensionType.renegotiation_info, 2)
            w2.add(1, 2)
            w2.add(0, 1)
        if len(w2.bytes):
            w.add(len(w2.bytes), 2)
            w.bytes += w2.bytes        
        return self.postWrite(w)


class Certificate(HandshakeMsg):
    def __init__(self, certificateType):
        HandshakeMsg.__init__(self, HandshakeType.certificate)
        self.certificateType = certificateType
        self.certChain = None

    def create(self, certChain):
        self.certChain = certChain
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        if self.certificateType == CertificateType.x509:
            chainLength = p.get(3)
            index = 0
            certificate_list = []
            while index != chainLength:
                certBytes = p.getVarBytes(3)
                x509 = X509()
                x509.parseBinary(certBytes)
                certificate_list.append(x509)
                index += len(certBytes)+3
            if certificate_list:
                self.certChain = X509CertChain(certificate_list)
        else:
            raise AssertionError()

        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        if self.certificateType == CertificateType.x509:
            chainLength = 0
            if self.certChain:
                certificate_list = self.certChain.x509List
            else:
                certificate_list = []
            #determine length
            for cert in certificate_list:
                bytes = cert.writeBytes()
                chainLength += len(bytes)+3
            #add bytes
            w.add(chainLength, 3)
            for cert in certificate_list:
                bytes = cert.writeBytes()
                w.addVarSeq(bytes, 1, 3)
        else:
            raise AssertionError()
        return self.postWrite(w)

class CertificateStatus(HandshakeMsg):
    def __init__(self):
        HandshakeMsg.__init__(self, HandshakeType.certificate_status)

    def create(self, ocsp_response):
        self.ocsp_response = ocsp_response
        return self

    # Defined for the sake of completeness, even though we currently only
    # support sending the status message (server-side), not requesting
    # or receiving it (client-side).
    def parse(self, p):
        p.startLengthCheck(3)
        status_type = p.get(1)
        # Only one type is specified, so hardwire it.
        if status_type != CertificateStatusType.ocsp:
            raise SyntaxError()
        ocsp_response = p.getVarBytes(3)
        if not ocsp_response:
            # Can't be empty
            raise SyntaxError()
        self.ocsp_response = ocsp_response
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.add(CertificateStatusType.ocsp, 1)
        w.addVarSeq(bytearray(self.ocsp_response), 1, 3)
        return self.postWrite(w)

class CertificateRequest(HandshakeMsg):
    def __init__(self, version):
        HandshakeMsg.__init__(self, HandshakeType.certificate_request)
        self.certificate_types = []
        self.certificate_authorities = []
        self.version = version
        self.supported_signature_algs = []

    def create(self, certificate_types, certificate_authorities, sig_algs):
        self.certificate_types = certificate_types
        self.certificate_authorities = certificate_authorities
        self.supported_signature_algs = sig_algs
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        self.certificate_types = p.getVarList(1, 1)
        if self.version >= (3,3):
            self.supported_signature_algs = \
                [(b >> 8, b & 0xff) for b in p.getVarList(2, 2)]
        ca_list_length = p.get(2)
        index = 0
        self.certificate_authorities = []
        while index != ca_list_length:
          ca_bytes = p.getVarBytes(2)
          self.certificate_authorities.append(ca_bytes)
          index += len(ca_bytes)+2
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.addVarSeq(self.certificate_types, 1, 1)
        if self.version >= (3,3):
            w.add(2 * len(self.supported_signature_algs), 2)
            for (hash, signature) in self.supported_signature_algs:
                w.add(hash, 1)
                w.add(signature, 1)
        caLength = 0
        #determine length
        for ca_dn in self.certificate_authorities:
            caLength += len(ca_dn)+2
        w.add(caLength, 2)
        #add bytes
        for ca_dn in self.certificate_authorities:
            w.addVarSeq(ca_dn, 1, 2)
        return self.postWrite(w)

class ServerKeyExchange(HandshakeMsg):
    def __init__(self, cipherSuite, version):
        HandshakeMsg.__init__(self, HandshakeType.server_key_exchange)
        self.cipherSuite = cipherSuite
        self.version = version
        self.srp_N = 0
        self.srp_g = 0
        self.srp_s = bytearray(0)
        self.srp_B = 0
        # DH params:
        self.dh_p = 0
        self.dh_g = 0
        self.dh_Ys = 0
        # ECDH params:
        self.ecdhCurve = 0
        self.ecdhPublic = bytearray(0)
        self.signature = bytearray(0)
        self.signature_algorithm = None

    def createSRP(self, srp_N, srp_g, srp_s, srp_B):
        self.srp_N = srp_N
        self.srp_g = srp_g
        self.srp_s = srp_s
        self.srp_B = srp_B
        return self
    
    def createDH(self, dh_p, dh_g, dh_Ys):
        self.dh_p = dh_p
        self.dh_g = dh_g
        self.dh_Ys = dh_Ys
        return self

    def createECDH(self, ecdhCurve, ecdhPublic):
        self.ecdhCurve = ecdhCurve
        self.ecdhPublic = ecdhPublic
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        if self.cipherSuite in CipherSuite.srpAllSuites:
            self.srp_N = bytesToNumber(p.getVarBytes(2))
            self.srp_g = bytesToNumber(p.getVarBytes(2))
            self.srp_s = p.getVarBytes(1)
            self.srp_B = bytesToNumber(p.getVarBytes(2))
            if self.cipherSuite in CipherSuite.srpCertSuites:
                self.signature = p.getVarBytes(2)
        elif self.cipherSuite in CipherSuite.anonSuites:
            self.dh_p = bytesToNumber(p.getVarBytes(2))
            self.dh_g = bytesToNumber(p.getVarBytes(2))
            self.dh_Ys = bytesToNumber(p.getVarBytes(2))
        p.stopLengthCheck()
        return self

    def write_params(self):
        w = Writer()
        if self.cipherSuite in CipherSuite.srpAllSuites:
            w.addVarSeq(numberToByteArray(self.srp_N), 1, 2)
            w.addVarSeq(numberToByteArray(self.srp_g), 1, 2)
            w.addVarSeq(self.srp_s, 1, 1)
            w.addVarSeq(numberToByteArray(self.srp_B), 1, 2)
        elif self.cipherSuite in CipherSuite.dhAllSuites:
            w.addVarSeq(numberToByteArray(self.dh_p), 1, 2)
            w.addVarSeq(numberToByteArray(self.dh_g), 1, 2)
            w.addVarSeq(numberToByteArray(self.dh_Ys), 1, 2)
        elif self.cipherSuite in CipherSuite.ecdhAllSuites:
            w.add(ECCurveType.named_curve, 1)
            w.add(self.ecdhCurve, 2)
            w.addVarSeq(self.ecdhPublic, 1, 1)
        else:
            assert(False)
        return w.bytes

    def write(self):
        w = Writer()
        w.bytes += self.write_params()
        if self.cipherSuite in CipherSuite.certAllSuites:
            if self.version >= (3,3):
                w.add(self.signature_algorithm[0], 1)
                w.add(self.signature_algorithm[1], 1)
            w.addVarSeq(self.signature, 1, 2)
        return self.postWrite(w)

    def signingPayload(self, clientRandom, serverRandom):
        return clientRandom + serverRandom + self.write_params()

class ServerHelloDone(HandshakeMsg):
    def __init__(self):
        HandshakeMsg.__init__(self, HandshakeType.server_hello_done)

    def create(self):
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        return self.postWrite(w)

class ClientKeyExchange(HandshakeMsg):
    def __init__(self, cipherSuite, version=None):
        HandshakeMsg.__init__(self, HandshakeType.client_key_exchange)
        self.cipherSuite = cipherSuite
        self.version = version
        self.srp_A = 0
        self.encryptedPreMasterSecret = bytearray(0)

    def createSRP(self, srp_A):
        self.srp_A = srp_A
        return self

    def createRSA(self, encryptedPreMasterSecret):
        self.encryptedPreMasterSecret = encryptedPreMasterSecret
        return self
    
    def createDH(self, dh_Yc):
        self.dh_Yc = dh_Yc
        return self
    
    def parse(self, p):
        p.startLengthCheck(3)
        if self.cipherSuite in CipherSuite.srpAllSuites:
            self.srp_A = bytesToNumber(p.getVarBytes(2))
        elif self.cipherSuite in CipherSuite.certSuites:
            if self.version in ((3,1), (3,2), (3,3)):
                self.encryptedPreMasterSecret = p.getVarBytes(2)
            elif self.version == (3,0):
                self.encryptedPreMasterSecret = \
                    p.getFixBytes(len(p.bytes)-p.index)
            else:
                raise AssertionError()
        elif self.cipherSuite in CipherSuite.dhAllSuites:
            self.dh_Yc = bytesToNumber(p.getVarBytes(2))
        elif self.cipherSuite in CipherSuite.ecdhAllSuites:
            self.ecdh_Yc = p.getVarBytes(1)
        else:
            raise AssertionError()
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        if self.cipherSuite in CipherSuite.srpAllSuites:
            w.addVarSeq(numberToByteArray(self.srp_A), 1, 2)
        elif self.cipherSuite in CipherSuite.certSuites:
            if self.version in ((3,1), (3,2), (3,3)):
                w.addVarSeq(self.encryptedPreMasterSecret, 1, 2)
            elif self.version == (3,0):
                w.addFixSeq(self.encryptedPreMasterSecret, 1)
            else:
                raise AssertionError()
        elif self.cipherSuite in CipherSuite.anonSuites:
            w.addVarSeq(numberToByteArray(self.dh_Yc), 1, 2)            
        else:
            raise AssertionError()
        return self.postWrite(w)

class CertificateVerify(HandshakeMsg):
    def __init__(self, version):
        HandshakeMsg.__init__(self, HandshakeType.certificate_verify)
        self.version = version
        self.signature_algorithm = None
        self.signature = bytearray(0)

    def create(self, signature_algorithm, signature):
        self.signature_algorithm = signature_algorithm
        self.signature = signature
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        if self.version >= (3,3):
            self.signature_algorithm = (p.get(1), p.get(1))
        self.signature = p.getVarBytes(2)
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        if self.version >= (3,3):
            w.add(self.signature_algorithm[0], 1)
            w.add(self.signature_algorithm[1], 1)
        w.addVarSeq(self.signature, 1, 2)
        return self.postWrite(w)

class ChangeCipherSpec(object):
    def __init__(self):
        self.contentType = ContentType.change_cipher_spec
        self.type = 1

    def create(self):
        self.type = 1
        return self

    def parse(self, p):
        p.setLengthCheck(1)
        self.type = p.get(1)
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.add(self.type,1)
        return w.bytes


class NextProtocol(HandshakeMsg):
    def __init__(self):
        HandshakeMsg.__init__(self, HandshakeType.next_protocol)
        self.next_proto = None

    def create(self, next_proto):
        self.next_proto = next_proto
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        self.next_proto = p.getVarBytes(1)
        _ = p.getVarBytes(1)
        p.stopLengthCheck()
        return self

    def write(self, trial=False):
        w = Writer()
        w.addVarSeq(self.next_proto, 1, 1)
        paddingLen = 32 - ((len(self.next_proto) + 2) % 32)
        w.addVarSeq(bytearray(paddingLen), 1, 1)
        return self.postWrite(w)

class Finished(HandshakeMsg):
    def __init__(self, version):
        HandshakeMsg.__init__(self, HandshakeType.finished)
        self.version = version
        self.verify_data = bytearray(0)

    def create(self, verify_data):
        self.verify_data = verify_data
        return self

    def parse(self, p):
        p.startLengthCheck(3)
        if self.version == (3,0):
            self.verify_data = p.getFixBytes(36)
        elif self.version in ((3,1), (3,2), (3,3)):
            self.verify_data = p.getFixBytes(12)
        else:
            raise AssertionError()
        p.stopLengthCheck()
        return self

    def write(self):
        w = Writer()
        w.addFixSeq(self.verify_data, 1)
        return self.postWrite(w)

class EncryptedExtensions(HandshakeMsg):
    def __init__(self):
        self.channel_id_key = None
        self.channel_id_proof = None

    def parse(self, p):
        p.startLengthCheck(3)
        soFar = 0
        while soFar != p.lengthCheck:
            extType = p.get(2)
            extLength = p.get(2)
            if extType == ExtensionType.channel_id:
                if extLength != 32*4:
                    raise SyntaxError()
                self.channel_id_key = p.getFixBytes(64)
                self.channel_id_proof = p.getFixBytes(64)
            else:
                p.getFixBytes(extLength)
            soFar += 4 + extLength
        p.stopLengthCheck()
        return self

class ApplicationData(object):
    def __init__(self):
        self.contentType = ContentType.application_data
        self.bytes = bytearray(0)

    def create(self, bytes):
        self.bytes = bytes
        return self
        
    def splitFirstByte(self):
        newMsg = ApplicationData().create(self.bytes[:1])
        self.bytes = self.bytes[1:]
        return newMsg

    def parse(self, p):
        self.bytes = p.bytes
        return self

    def write(self):
        return self.bytes
