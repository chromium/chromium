# Authors: 
#   Trevor Perrin
#   Google - added reqCAs parameter
#   Google (adapted by Sam Rushing and Marcelo Fernandez) - NPN support
#   Dimitris Moraitis - Anon ciphersuites
#   Martin von Loewis - python 3 port
#   Yngve Pettersen (ported by Paul Sokolovsky) - TLS 1.2
#
# See the LICENSE file for legal information regarding use of this file.

"""
MAIN CLASS FOR TLS LITE (START HERE!).
"""

import socket
from .utils.compat import formatExceptionTrace
from .tlsrecordlayer import TLSRecordLayer
from .session import Session
from .constants import *
from .utils.cryptomath import getRandomBytes
from .errors import *
from .messages import *
from .mathtls import *
from .handshakesettings import HandshakeSettings
from .utils.tackwrapper import *
from .utils.rsakey import RSAKey
from .utils import p256

class KeyExchange(object):
    def __init__(self, cipherSuite, clientHello, serverHello, privateKey):
        """
        Initializes the KeyExchange. privateKey is the signing private key.
        """
        self.cipherSuite = cipherSuite
        self.clientHello = clientHello
        self.serverHello = serverHello
        self.privateKey = privateKey

    def makeServerKeyExchange():
        """
        Returns a ServerKeyExchange object for the server's initial leg in the
        handshake. If the key exchange method does not send ServerKeyExchange
        (e.g. RSA), it returns None.
        """
        raise NotImplementedError()

    def processClientKeyExchange(clientKeyExchange):
        """
        Processes the client's ClientKeyExchange message and returns the
        premaster secret. Raises TLSLocalAlert on error.
        """
        raise NotImplementedError()

class RSAKeyExchange(KeyExchange):
    def makeServerKeyExchange(self):
        return None

    def processClientKeyExchange(self, clientKeyExchange):
        premasterSecret = self.privateKey.decrypt(\
            clientKeyExchange.encryptedPreMasterSecret)

        # On decryption failure randomize premaster secret to avoid
        # Bleichenbacher's "million message" attack
        randomPreMasterSecret = getRandomBytes(48)
        if not premasterSecret:
            premasterSecret = randomPreMasterSecret
        elif len(premasterSecret)!=48:
            premasterSecret = randomPreMasterSecret
        else:
            versionCheck = (premasterSecret[0], premasterSecret[1])
            if versionCheck != self.clientHello.client_version:
                #Tolerate buggy IE clients
                if versionCheck != self.serverHello.server_version:
                    premasterSecret = randomPreMasterSecret
        return premasterSecret

def _hexStringToNumber(s):
    s = s.replace(" ", "").replace("\n", "")
    if len(s) % 2 != 0:
        raise ValueError("Length is not even")
    return bytesToNumber(bytearray(s.decode("hex")))

class DHE_RSAKeyExchange(KeyExchange):
    # 2048-bit MODP Group (RFC 3526, Section 3)
    dh_p = _hexStringToNumber("""
FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1
29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD
EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245
E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED
EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D
C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F
83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D
670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B
E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9
DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510
15728E5A 8AACAA68 FFFFFFFF FFFFFFFF""")
    dh_g = 2

    # RFC 3526, Section 8.
    strength = 160

    def makeServerKeyExchange(self):
        # Per RFC 3526, Section 1, the exponent should have double the entropy
        # of the strength of the curve.
        self.dh_Xs = bytesToNumber(getRandomBytes(self.strength * 2 / 8))
        dh_Ys = powMod(self.dh_g, self.dh_Xs, self.dh_p)

        version = self.serverHello.server_version
        serverKeyExchange = ServerKeyExchange(self.cipherSuite, version)
        serverKeyExchange.createDH(self.dh_p, self.dh_g, dh_Ys)
        hashBytes = serverKeyExchange.hash(self.clientHello.random,
                                           self.serverHello.random)
        if version >= (3,3):
            # TODO: Signature algorithm negotiation not supported.
            hashBytes = RSAKey.addPKCS1SHA1Prefix(hashBytes)
        serverKeyExchange.signature = self.privateKey.sign(hashBytes)
        return serverKeyExchange

    def processClientKeyExchange(self, clientKeyExchange):
        dh_Yc = clientKeyExchange.dh_Yc

        # First half of RFC 2631, Section 2.1.5. Validate the client's public
        # key.
        if not 2 <= dh_Yc <= self.dh_p - 1:
            raise TLSLocalAlert(AlertDescription.illegal_parameter,
                                "Invalid dh_Yc value")

        S = powMod(dh_Yc, self.dh_Xs, self.dh_p)
        return numberToByteArray(S)

class ECDHE_RSAKeyExchange(KeyExchange):
    def makeServerKeyExchange(self):
        public, self.private = p256.generatePublicPrivate()

        version = self.serverHello.server_version
        serverKeyExchange = ServerKeyExchange(self.cipherSuite, version)
        serverKeyExchange.createECDH(NamedCurve.secp256r1, bytearray(public))
        hashBytes = serverKeyExchange.hash(self.clientHello.random,
                                           self.serverHello.random)
        if version >= (3,3):
            # TODO: Signature algorithm negotiation not supported.
            hashBytes = RSAKey.addPKCS1SHA1Prefix(hashBytes)
        serverKeyExchange.signature = self.privateKey.sign(hashBytes)
        return serverKeyExchange

    def processClientKeyExchange(self, clientKeyExchange):
        ecdh_Yc = clientKeyExchange.ecdh_Yc
        return bytearray(p256.generateSharedValue(bytes(ecdh_Yc), self.private))

class TLSConnection(TLSRecordLayer):
    """
    This class wraps a socket and provides TLS handshaking and data
    transfer.

    To use this class, create a new instance, passing a connected
    socket into the constructor.  Then call some handshake function.
    If the handshake completes without raising an exception, then a TLS
    connection has been negotiated.  You can transfer data over this
    connection as if it were a socket.

    This class provides both synchronous and asynchronous versions of
    its key functions.  The synchronous versions should be used when
    writing single-or multi-threaded code using blocking sockets.  The
    asynchronous versions should be used when performing asynchronous,
    event-based I/O with non-blocking sockets.

    Asynchronous I/O is a complicated subject; typically, you should
    not use the asynchronous functions directly, but should use some
    framework like asyncore or Twisted which TLS Lite integrates with
    (see
    L{tlslite.integration.tlsasyncdispatchermixin.TLSAsyncDispatcherMixIn}).
    """

    def __init__(self, sock):
        """Create a new TLSConnection instance.

        @param sock: The socket data will be transmitted on.  The
        socket should already be connected.  It may be in blocking or
        non-blocking mode.

        @type sock: L{socket.socket}
        """
        TLSRecordLayer.__init__(self, sock)
        self.clientRandom = b""
        self.serverRandom = b""

    #*********************************************************
    # Client Handshake Functions
    #*********************************************************

    def handshakeClientAnonymous(self, session=None, settings=None, 
                                checker=None, serverName="",
                                async=False):
        """Perform an anonymous handshake in the role of client.

        This function performs an SSL or TLS handshake using an
        anonymous Diffie Hellman ciphersuite.
        
        Like any handshake function, this can be called on a closed
        TLS connection, or on a TLS connection that is already open.
        If called on an open connection it performs a re-handshake.

        If the function completes without raising an exception, the
        TLS connection will be open and available for data transfer.

        If an exception is raised, the connection will have been
        automatically closed (if it was ever open).

        @type session: L{tlslite.Session.Session}
        @param session: A TLS session to attempt to resume.  If the
        resumption does not succeed, a full handshake will be
        performed.

        @type settings: L{tlslite.HandshakeSettings.HandshakeSettings}
        @param settings: Various settings which can be used to control
        the ciphersuites, certificate types, and SSL/TLS versions
        offered by the client.

        @type checker: L{tlslite.Checker.Checker}
        @param checker: A Checker instance.  This instance will be
        invoked to examine the other party's authentication
        credentials, if the handshake completes succesfully.
        
        @type serverName: string
        @param serverName: The ServerNameIndication TLS Extension.

        @type async: bool
        @param async: If False, this function will block until the
        handshake is completed.  If True, this function will return a
        generator.  Successive invocations of the generator will
        return 0 if it is waiting to read from the socket, 1 if it is
        waiting to write to the socket, or will raise StopIteration if
        the handshake operation is completed.

        @rtype: None or an iterable
        @return: If 'async' is True, a generator object will be
        returned.

        @raise socket.error: If a socket error occurs.
        @raise tlslite.errors.TLSAbruptCloseError: If the socket is closed
        without a preceding alert.
        @raise tlslite.errors.TLSAlert: If a TLS alert is signalled.
        @raise tlslite.errors.TLSAuthenticationError: If the checker
        doesn't like the other party's authentication credentials.
        """
        handshaker = self._handshakeClientAsync(anonParams=(True),
                                                session=session,
                                                settings=settings,
                                                checker=checker,
                                                serverName=serverName)
        if async:
            return handshaker
        for result in handshaker:
            pass

    def handshakeClientSRP(self, username, password, session=None,
                           settings=None, checker=None, 
                           reqTack=True, serverName="",
                           async=False):
        """Perform an SRP handshake in the role of client.

        This function performs a TLS/SRP handshake.  SRP mutually
        authenticates both parties to each other using only a
        username and password.  This function may also perform a
        combined SRP and server-certificate handshake, if the server
        chooses to authenticate itself with a certificate chain in
        addition to doing SRP.

        If the function completes without raising an exception, the
        TLS connection will be open and available for data transfer.

        If an exception is raised, the connection will have been
        automatically closed (if it was ever open).

        @type username: str
        @param username: The SRP username.

        @type password: str
        @param password: The SRP password.

        @type session: L{tlslite.session.Session}
        @param session: A TLS session to attempt to resume.  This
        session must be an SRP session performed with the same username
        and password as were passed in.  If the resumption does not
        succeed, a full SRP handshake will be performed.

        @type settings: L{tlslite.handshakesettings.HandshakeSettings}
        @param settings: Various settings which can be used to control
        the ciphersuites, certificate types, and SSL/TLS versions
        offered by the client.

        @type checker: L{tlslite.checker.Checker}
        @param checker: A Checker instance.  This instance will be
        invoked to examine the other party's authentication
        credentials, if the handshake completes succesfully.

        @type reqTack: bool
        @param reqTack: Whether or not to send a "tack" TLS Extension, 
        requesting the server return a TackExtension if it has one.

        @type serverName: string
        @param serverName: The ServerNameIndication TLS Extension.

        @type async: bool
        @param async: If False, this function will block until the
        handshake is completed.  If True, this function will return a
        generator.  Successive invocations of the generator will
        return 0 if it is waiting to read from the socket, 1 if it is
        waiting to write to the socket, or will raise StopIteration if
        the handshake operation is completed.

        @rtype: None or an iterable
        @return: If 'async' is True, a generator object will be
        returned.

        @raise socket.error: If a socket error occurs.
        @raise tlslite.errors.TLSAbruptCloseError: If the socket is closed
        without a preceding alert.
        @raise tlslite.errors.TLSAlert: If a TLS alert is signalled.
        @raise tlslite.errors.TLSAuthenticationError: If the checker
        doesn't like the other party's authentication credentials.
        """
        handshaker = self._handshakeClientAsync(srpParams=(username, password),
                        session=session, settings=settings, checker=checker,
                        reqTack=reqTack, serverName=serverName)
        # The handshaker is a Python Generator which executes the handshake.
        # It allows the handshake to be run in a "piecewise", asynchronous
        # fashion, returning 1 when it is waiting to able to write, 0 when
        # it is waiting to read.
        #
        # If 'async' is True, the generator is returned to the caller, 
        # otherwise it is executed to completion here.  
        if async:
            return handshaker
        for result in handshaker:
            pass

    def handshakeClientCert(self, certChain=None, privateKey=None,
                            session=None, settings=None, checker=None,
                            nextProtos=None, reqTack=True, serverName="",
                            async=False):
        """Perform a certificate-based handshake in the role of client.

        This function performs an SSL or TLS handshake.  The server
        will authenticate itself using an X.509 certificate
        chain.  If the handshake succeeds, the server's certificate
        chain will be stored in the session's serverCertChain attribute.
        Unless a checker object is passed in, this function does no
        validation or checking of the server's certificate chain.

        If the server requests client authentication, the
        client will send the passed-in certificate chain, and use the
        passed-in private key to authenticate itself.  If no
        certificate chain and private key were passed in, the client
        will attempt to proceed without client authentication.  The
        server may or may not allow this.

        If the function completes without raising an exception, the
        TLS connection will be open and available for data transfer.

        If an exception is raised, the connection will have been
        automatically closed (if it was ever open).

        @type certChain: L{tlslite.x509certchain.X509CertChain}
        @param certChain: The certificate chain to be used if the
        server requests client authentication.

        @type privateKey: L{tlslite.utils.rsakey.RSAKey}
        @param privateKey: The private key to be used if the server
        requests client authentication.

        @type session: L{tlslite.session.Session}
        @param session: A TLS session to attempt to resume.  If the
        resumption does not succeed, a full handshake will be
        performed.

        @type settings: L{tlslite.handshakesettings.HandshakeSettings}
        @param settings: Various settings which can be used to control
        the ciphersuites, certificate types, and SSL/TLS versions
        offered by the client.

        @type checker: L{tlslite.checker.Checker}
        @param checker: A Checker instance.  This instance will be
        invoked to examine the other party's authentication
        credentials, if the handshake completes succesfully.
        
        @type nextProtos: list of strings.
        @param nextProtos: A list of upper layer protocols ordered by
        preference, to use in the Next-Protocol Negotiation Extension.
        
        @type reqTack: bool
        @param reqTack: Whether or not to send a "tack" TLS Extension, 
        requesting the server return a TackExtension if it has one.        

        @type serverName: string
        @param serverName: The ServerNameIndication TLS Extension.

        @type async: bool
        @param async: If False, this function will block until the
        handshake is completed.  If True, this function will return a
        generator.  Successive invocations of the generator will
        return 0 if it is waiting to read from the socket, 1 if it is
        waiting to write to the socket, or will raise StopIteration if
        the handshake operation is completed.

        @rtype: None or an iterable
        @return: If 'async' is True, a generator object will be
        returned.

        @raise socket.error: If a socket error occurs.
        @raise tlslite.errors.TLSAbruptCloseError: If the socket is closed
        without a preceding alert.
        @raise tlslite.errors.TLSAlert: If a TLS alert is signalled.
        @raise tlslite.errors.TLSAuthenticationError: If the checker
        doesn't like the other party's authentication credentials.
        """
        handshaker = self._handshakeClientAsync(certParams=(certChain,
                        privateKey), session=session, settings=settings,
                        checker=checker, serverName=serverName, 
                        nextProtos=nextProtos, reqTack=reqTack)
        # The handshaker is a Python Generator which executes the handshake.
        # It allows the handshake to be run in a "piecewise", asynchronous
        # fashion, returning 1 when it is waiting to able to write, 0 when
        # it is waiting to read.
        #
        # If 'async' is True, the generator is returned to the caller, 
        # otherwise it is executed to completion here.                        
        if async:
            return handshaker
        for result in handshaker:
            pass


    def _handshakeClientAsync(self, srpParams=(), certParams=(), anonParams=(),
                             session=None, settings=None, checker=None,
                             nextProtos=None, serverName="", reqTack=True):

        handshaker = self._handshakeClientAsyncHelper(srpParams=srpParams,
                certParams=certParams,
                anonParams=anonParams,
                session=session,
                settings=settings,
                serverName=serverName,
                nextProtos=nextProtos,
                reqTack=reqTack)
        for result in self._handshakeWrapperAsync(handshaker, checker):
            yield result


    def _handshakeClientAsyncHelper(self, srpParams, certParams, anonParams,
                               session, settings, serverName, nextProtos, reqTack):
        
        self._handshakeStart(client=True)

        #Unpack parameters
        srpUsername = None      # srpParams[0]
        password = None         # srpParams[1]
        clientCertChain = None  # certParams[0]
        privateKey = None       # certParams[1]

        # Allow only one of (srpParams, certParams, anonParams)
        if srpParams:
            assert(not certParams)
            assert(not anonParams)
            srpUsername, password = srpParams
        if certParams:
            assert(not srpParams)
            assert(not anonParams)            
            clientCertChain, privateKey = certParams
        if anonParams:
            assert(not srpParams)         
            assert(not certParams)

        #Validate parameters
        if srpUsername and not password:
            raise ValueError("Caller passed a username but no password")
        if password and not srpUsername:
            raise ValueError("Caller passed a password but no username")
        if clientCertChain and not privateKey:
            raise ValueError("Caller passed a certChain but no privateKey")
        if privateKey and not clientCertChain:
            raise ValueError("Caller passed a privateKey but no certChain")
        if reqTack:
            if not tackpyLoaded:
                reqTack = False
            if not settings or not settings.useExperimentalTackExtension:
                reqTack = False
        if nextProtos is not None:
            if len(nextProtos) == 0:
                raise ValueError("Caller passed no nextProtos")
        
        # Validates the settings and filters out any unsupported ciphers
        # or crypto libraries that were requested        
        if not settings:
            settings = HandshakeSettings()
        settings = settings._filter()

        if settings.alpnProtos is not None:
            if len(settings.alpnProtos) == 0:
                raise ValueError("Caller passed no alpnProtos")

        if clientCertChain:
            if not isinstance(clientCertChain, X509CertChain):
                raise ValueError("Unrecognized certificate type")
            if "x509" not in settings.certificateTypes:
                raise ValueError("Client certificate doesn't match "\
                                 "Handshake Settings")
                                  
        if session:
            # session.valid() ensures session is resumable and has 
            # non-empty sessionID
            if not session.valid():
                session = None #ignore non-resumable sessions...
            elif session.resumable: 
                if session.srpUsername != srpUsername:
                    raise ValueError("Session username doesn't match")
                if session.serverName != serverName:
                    raise ValueError("Session servername doesn't match")

        #Add Faults to parameters
        if srpUsername and self.fault == Fault.badUsername:
            srpUsername += "GARBAGE"
        if password and self.fault == Fault.badPassword:
            password += "GARBAGE"

        #Tentatively set the version to the client's minimum version.
        #We'll use this for the ClientHello, and if an error occurs
        #parsing the Server Hello, we'll use this version for the response
        self.version = settings.maxVersion
        
        # OK Start sending messages!
        # *****************************

        # Send the ClientHello.
        for result in self._clientSendClientHello(settings, session, 
                                        srpUsername, srpParams, certParams,
                                        anonParams, serverName, nextProtos,
                                        reqTack):
            if result in (0,1): yield result
            else: break
        clientHello = result
        
        #Get the ServerHello.
        for result in self._clientGetServerHello(settings, clientHello):
            if result in (0,1): yield result
            else: break
        serverHello = result
        cipherSuite = serverHello.cipher_suite
        
        # Choose a matching Next Protocol from server list against ours
        # (string or None)
        nextProto = self._clientSelectNextProto(nextProtos, serverHello)

        #If the server elected to resume the session, it is handled here.
        for result in self._clientResume(session, serverHello, 
                        clientHello.random, 
                        settings.cipherImplementations,
                        nextProto):
            if result in (0,1): yield result
            else: break
        if result == "resumed_and_finished":
            self._handshakeDone(resumed=True)
            return

        #If the server selected an SRP ciphersuite, the client finishes
        #reading the post-ServerHello messages, then derives a
        #premasterSecret and sends a corresponding ClientKeyExchange.
        if cipherSuite in CipherSuite.srpAllSuites:
            for result in self._clientSRPKeyExchange(\
                    settings, cipherSuite, serverHello.certificate_type, 
                    srpUsername, password,
                    clientHello.random, serverHello.random, 
                    serverHello.tackExt):                
                if result in (0,1): yield result
                else: break                
            (premasterSecret, serverCertChain, tackExt) = result

        #If the server selected an anonymous ciphersuite, the client
        #finishes reading the post-ServerHello messages.
        elif cipherSuite in CipherSuite.anonSuites:
            for result in self._clientAnonKeyExchange(settings, cipherSuite,
                                    clientHello.random, serverHello.random):
                if result in (0,1): yield result
                else: break
            (premasterSecret, serverCertChain, tackExt) = result     
               
        #If the server selected a certificate-based RSA ciphersuite,
        #the client finishes reading the post-ServerHello messages. If 
        #a CertificateRequest message was sent, the client responds with
        #a Certificate message containing its certificate chain (if any),
        #and also produces a CertificateVerify message that signs the 
        #ClientKeyExchange.
        else:
            for result in self._clientRSAKeyExchange(settings, cipherSuite,
                                    clientCertChain, privateKey,
                                    serverHello.certificate_type,
                                    clientHello.random, serverHello.random,
                                    serverHello.tackExt):
                if result in (0,1): yield result
                else: break
            (premasterSecret, serverCertChain, clientCertChain, 
             tackExt) = result
                        
        #After having previously sent a ClientKeyExchange, the client now
        #initiates an exchange of Finished messages.
        for result in self._clientFinished(premasterSecret,
                            clientHello.random, 
                            serverHello.random,
                            cipherSuite, settings.cipherImplementations,
                            nextProto):
                if result in (0,1): yield result
                else: break
        masterSecret = result
        
        self.clientRandom = clientHello.random
        self.serverRandom = serverHello.random

        # Create the session object which is used for resumptions
        self.session = Session()
        self.session.create(masterSecret, serverHello.session_id, cipherSuite,
            srpUsername, clientCertChain, serverCertChain,
            tackExt, serverHello.tackExt!=None, serverName)
        self._handshakeDone(resumed=False)


    def _clientSendClientHello(self, settings, session, srpUsername,
                                srpParams, certParams, anonParams, 
                                serverName, nextProtos, reqTack):
        #Initialize acceptable ciphersuites
        cipherSuites = [CipherSuite.TLS_EMPTY_RENEGOTIATION_INFO_SCSV]
        if srpParams:
            cipherSuites += CipherSuite.getSrpAllSuites(settings)
        elif certParams:
            # TODO: Client DHE_RSA not supported.
            # cipherSuites += CipherSuite.getDheCertSuites(settings)
            cipherSuites += CipherSuite.getCertSuites(settings)
        elif anonParams:
            cipherSuites += CipherSuite.getAnonSuites(settings)
        else:
            assert(False)

        #Initialize acceptable certificate types
        certificateTypes = settings._getCertificateTypes()
            
        #Either send ClientHello (with a resumable session)...
        if session and session.sessionID:
            #If it's resumable, then its
            #ciphersuite must be one of the acceptable ciphersuites
            if session.cipherSuite not in cipherSuites:
                raise ValueError("Session's cipher suite not consistent "\
                                 "with parameters")
            else:
                clientHello = ClientHello()
                clientHello.create(settings.maxVersion, getRandomBytes(32),
                                   session.sessionID, cipherSuites,
                                   certificateTypes, 
                                   session.srpUsername,
                                   reqTack, settings.alpnProtos,
                                   nextProtos is not None,
                                   session.serverName)

        #Or send ClientHello (without)
        else:
            clientHello = ClientHello()
            clientHello.create(settings.maxVersion, getRandomBytes(32),
                               bytearray(0), cipherSuites,
                               certificateTypes, 
                               srpUsername,
                               reqTack, settings.alpnProtos,
                               nextProtos is not None,
                               serverName)
        for result in self._sendMsg(clientHello):
            yield result
        yield clientHello


    def _clientGetServerHello(self, settings, clientHello):
        for result in self._getMsg(ContentType.handshake,
                                  HandshakeType.server_hello):
            if result in (0,1): yield result
            else: break
        serverHello = result

        #Get the server version.  Do this before anything else, so any
        #error alerts will use the server's version
        self.version = serverHello.server_version

        #Future responses from server must use this version
        self._versionCheck = True

        #Check ServerHello
        if serverHello.server_version < settings.minVersion:
            for result in self._sendError(\
                AlertDescription.protocol_version,
                "Too old version: %s" % str(serverHello.server_version)):
                yield result
        if serverHello.server_version > settings.maxVersion:
            for result in self._sendError(\
                AlertDescription.protocol_version,
                "Too new version: %s" % str(serverHello.server_version)):
                yield result
        if serverHello.cipher_suite not in clientHello.cipher_suites:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with incorrect ciphersuite"):
                yield result
        if serverHello.certificate_type not in clientHello.certificate_types:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with incorrect certificate type"):
                yield result
        if serverHello.compression_method != 0:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with incorrect compression method"):
                yield result
        if serverHello.tackExt:            
            if not clientHello.tack:
                for result in self._sendError(\
                    AlertDescription.illegal_parameter,
                    "Server responded with unrequested Tack Extension"):
                    yield result
        if serverHello.alpn_proto_selected and not clientHello.alpn_protos_advertised:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with unrequested ALPN Extension"):
                yield result
        if serverHello.alpn_proto_selected and serverHello.next_protos:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with both ALPN and NPN extension"):
                yield result
        if serverHello.next_protos and not clientHello.supports_npn:
            for result in self._sendError(\
                AlertDescription.illegal_parameter,
                "Server responded with unrequested NPN Extension"):
                yield result
            if not serverHello.tackExt.verifySignatures():
                for result in self._sendError(\
                    AlertDescription.decrypt_error,
                    "TackExtension contains an invalid signature"):
                    yield result
        yield serverHello

    def _clientSelectNextProto(self, nextProtos, serverHello):
        # nextProtos is None or non-empty list of strings
        # serverHello.next_protos is None or possibly-empty list of strings
        #
        # !!! We assume the client may have specified nextProtos as a list of
        # strings so we convert them to bytearrays (it's awkward to require
        # the user to specify a list of bytearrays or "bytes", and in 
        # Python 2.6 bytes() is just an alias for str() anyways...
        if nextProtos is not None and serverHello.next_protos is not None:
            for p in nextProtos:
                if bytearray(p) in serverHello.next_protos:
                    return bytearray(p)
            else:
                # If the client doesn't support any of server's protocols,
                # or the server doesn't advertise any (next_protos == [])
                # the client SHOULD select the first protocol it supports.
                return bytearray(nextProtos[0])
        return None
 
    def _clientResume(self, session, serverHello, clientRandom, 
                      cipherImplementations, nextProto):
        #If the server agrees to resume
        if session and session.sessionID and \
            serverHello.session_id == session.sessionID:

            if serverHello.cipher_suite != session.cipherSuite:
                for result in self._sendError(\
                    AlertDescription.illegal_parameter,\
                    "Server's ciphersuite doesn't match session"):
                    yield result

            #Calculate pending connection states
            self._calcPendingStates(session.cipherSuite, 
                                    session.masterSecret, 
                                    clientRandom, serverHello.random, 
                                    cipherImplementations)                                   

            #Exchange ChangeCipherSpec and Finished messages
            for result in self._getFinished(session.masterSecret):
                yield result
            for result in self._sendFinished(session.masterSecret, nextProto):
                yield result

            #Set the session for this connection
            self.session = session
            yield "resumed_and_finished"        
            
    def _clientSRPKeyExchange(self, settings, cipherSuite, certificateType, 
            srpUsername, password,
            clientRandom, serverRandom, tackExt):

        #If the server chose an SRP+RSA suite...
        if cipherSuite in CipherSuite.srpCertSuites:
            #Get Certificate, ServerKeyExchange, ServerHelloDone
            for result in self._getMsg(ContentType.handshake,
                    HandshakeType.certificate, certificateType):
                if result in (0,1): yield result
                else: break
            serverCertificate = result
        else:
            serverCertificate = None

        for result in self._getMsg(ContentType.handshake,
                HandshakeType.server_key_exchange, cipherSuite):
            if result in (0,1): yield result
            else: break
        serverKeyExchange = result

        for result in self._getMsg(ContentType.handshake,
                HandshakeType.server_hello_done):
            if result in (0,1): yield result
            else: break
        serverHelloDone = result
            
        #Calculate SRP premaster secret
        #Get and check the server's group parameters and B value
        N = serverKeyExchange.srp_N
        g = serverKeyExchange.srp_g
        s = serverKeyExchange.srp_s
        B = serverKeyExchange.srp_B

        if (g,N) not in goodGroupParameters:
            for result in self._sendError(\
                    AlertDescription.insufficient_security,
                    "Unknown group parameters"):
                yield result
        if numBits(N) < settings.minKeySize:
            for result in self._sendError(\
                    AlertDescription.insufficient_security,
                    "N value is too small: %d" % numBits(N)):
                yield result
        if numBits(N) > settings.maxKeySize:
            for result in self._sendError(\
                    AlertDescription.insufficient_security,
                    "N value is too large: %d" % numBits(N)):
                yield result
        if B % N == 0:
            for result in self._sendError(\
                    AlertDescription.illegal_parameter,
                    "Suspicious B value"):
                yield result

        #Check the server's signature, if server chose an
        #SRP+RSA suite
        serverCertChain = None
        if cipherSuite in CipherSuite.srpCertSuites:
            #Hash ServerKeyExchange/ServerSRPParams
            hashBytes = serverKeyExchange.hash(clientRandom, serverRandom)

            #Extract signature bytes from ServerKeyExchange
            sigBytes = serverKeyExchange.signature
            if len(sigBytes) == 0:
                for result in self._sendError(\
                        AlertDescription.illegal_parameter,
                        "Server sent an SRP ServerKeyExchange "\
                        "message without a signature"):
                    yield result

            # Get server's public key from the Certificate message
            # Also validate the chain against the ServerHello's TACKext (if any)
            # If none, and a TACK cert is present, return its TACKext  
            for result in self._clientGetKeyFromChain(serverCertificate,
                                               settings, tackExt):
                if result in (0,1): yield result
                else: break
            publicKey, serverCertChain, tackExt = result

            #Verify signature
            if not publicKey.verify(sigBytes, hashBytes):
                for result in self._sendError(\
                        AlertDescription.decrypt_error,
                        "Signature failed to verify"):
                    yield result

        #Calculate client's ephemeral DH values (a, A)
        a = bytesToNumber(getRandomBytes(32))
        A = powMod(g, a, N)

        #Calculate client's static DH values (x, v)
        x = makeX(s, bytearray(srpUsername, "utf-8"),
                    bytearray(password, "utf-8"))
        v = powMod(g, x, N)

        #Calculate u
        u = makeU(N, A, B)

        #Calculate premaster secret
        k = makeK(N, g)
        S = powMod((B - (k*v)) % N, a+(u*x), N)

        if self.fault == Fault.badA:
            A = N
            S = 0
            
        premasterSecret = numberToByteArray(S)

        #Send ClientKeyExchange
        for result in self._sendMsg(\
                ClientKeyExchange(cipherSuite).createSRP(A)):
            yield result
        yield (premasterSecret, serverCertChain, tackExt)
                   

    def _clientRSAKeyExchange(self, settings, cipherSuite, 
                                clientCertChain, privateKey,
                                certificateType,
                                clientRandom, serverRandom,
                                tackExt):

        #Get Certificate[, CertificateRequest], ServerHelloDone
        for result in self._getMsg(ContentType.handshake,
                HandshakeType.certificate, certificateType):
            if result in (0,1): yield result
            else: break
        serverCertificate = result

        # Get CertificateRequest or ServerHelloDone
        for result in self._getMsg(ContentType.handshake,
                (HandshakeType.server_hello_done,
                HandshakeType.certificate_request)):
            if result in (0,1): yield result
            else: break
        msg = result
        certificateRequest = None
        if isinstance(msg, CertificateRequest):
            certificateRequest = msg
            # We got CertificateRequest, so this must be ServerHelloDone
            for result in self._getMsg(ContentType.handshake,
                    HandshakeType.server_hello_done):
                if result in (0,1): yield result
                else: break
            serverHelloDone = result
        elif isinstance(msg, ServerHelloDone):
            serverHelloDone = msg

        # Get server's public key from the Certificate message
        # Also validate the chain against the ServerHello's TACKext (if any)
        # If none, and a TACK cert is present, return its TACKext  
        for result in self._clientGetKeyFromChain(serverCertificate,
                                           settings, tackExt):
            if result in (0,1): yield result
            else: break
        publicKey, serverCertChain, tackExt = result

        #Calculate premaster secret
        premasterSecret = getRandomBytes(48)
        premasterSecret[0] = settings.maxVersion[0]
        premasterSecret[1] = settings.maxVersion[1]

        if self.fault == Fault.badPremasterPadding:
            premasterSecret[0] = 5
        if self.fault == Fault.shortPremasterSecret:
            premasterSecret = premasterSecret[:-1]

        #Encrypt premaster secret to server's public key
        encryptedPreMasterSecret = publicKey.encrypt(premasterSecret)

        #If client authentication was requested, send Certificate
        #message, either with certificates or empty
        if certificateRequest:
            clientCertificate = Certificate(certificateType)

            if clientCertChain:
                #Check to make sure we have the same type of
                #certificates the server requested
                wrongType = False
                if certificateType == CertificateType.x509:
                    if not isinstance(clientCertChain, X509CertChain):
                        wrongType = True
                if wrongType:
                    for result in self._sendError(\
                            AlertDescription.handshake_failure,
                            "Client certificate is of wrong type"):
                        yield result

                clientCertificate.create(clientCertChain)
            for result in self._sendMsg(clientCertificate):
                yield result
        else:
            #The server didn't request client auth, so we
            #zeroize these so the clientCertChain won't be
            #stored in the session.
            privateKey = None
            clientCertChain = None

        #Send ClientKeyExchange
        clientKeyExchange = ClientKeyExchange(cipherSuite,
                                              self.version)
        clientKeyExchange.createRSA(encryptedPreMasterSecret)
        for result in self._sendMsg(clientKeyExchange):
            yield result

        #If client authentication was requested and we have a
        #private key, send CertificateVerify
        if certificateRequest and privateKey:
            signatureAlgorithm = None
            if self.version == (3,0):
                masterSecret = calcMasterSecret(self.version,
                                         premasterSecret,
                                         clientRandom,
                                         serverRandom,
                                         b"", False)
                verifyBytes = self._calcSSLHandshakeHash(masterSecret, b"")
            elif self.version in ((3,1), (3,2)):
                verifyBytes = self._handshake_md5.digest() + \
                                self._handshake_sha.digest()
            elif self.version == (3,3):
                # TODO: Signature algorithm negotiation not supported.
                signatureAlgorithm = (HashAlgorithm.sha1, SignatureAlgorithm.rsa)
                verifyBytes = self._handshake_sha.digest()
                verifyBytes = RSAKey.addPKCS1SHA1Prefix(verifyBytes)
            if self.fault == Fault.badVerifyMessage:
                verifyBytes[0] = ((verifyBytes[0]+1) % 256)
            signedBytes = privateKey.sign(verifyBytes)
            certificateVerify = CertificateVerify(self.version)
            certificateVerify.create(signatureAlgorithm, signedBytes)
            for result in self._sendMsg(certificateVerify):
                yield result
        yield (premasterSecret, serverCertChain, clientCertChain, tackExt)

    def _clientAnonKeyExchange(self, settings, cipherSuite, clientRandom, 
                               serverRandom):
        for result in self._getMsg(ContentType.handshake,
                HandshakeType.server_key_exchange, cipherSuite):
            if result in (0,1): yield result
            else: break
        serverKeyExchange = result

        for result in self._getMsg(ContentType.handshake,
                HandshakeType.server_hello_done):
            if result in (0,1): yield result
            else: break
        serverHelloDone = result
            
        #calculate Yc
        dh_p = serverKeyExchange.dh_p
        dh_g = serverKeyExchange.dh_g
        dh_Xc = bytesToNumber(getRandomBytes(32))
        dh_Ys = serverKeyExchange.dh_Ys
        dh_Yc = powMod(dh_g, dh_Xc, dh_p)
        
        #Send ClientKeyExchange
        for result in self._sendMsg(\
                ClientKeyExchange(cipherSuite, self.version).createDH(dh_Yc)):
            yield result
            
        #Calculate premaster secret
        S = powMod(dh_Ys, dh_Xc, dh_p)
        premasterSecret = numberToByteArray(S)
                     
        yield (premasterSecret, None, None)
        
    def _clientFinished(self, premasterSecret, clientRandom, serverRandom,
                        cipherSuite, cipherImplementations, nextProto):

        masterSecret = calcMasterSecret(self.version, premasterSecret,
                            clientRandom, serverRandom, b"", False)
        self._calcPendingStates(cipherSuite, masterSecret, 
                                clientRandom, serverRandom, 
                                cipherImplementations)

        #Exchange ChangeCipherSpec and Finished messages
        for result in self._sendFinished(masterSecret, nextProto):
            yield result
        for result in self._getFinished(masterSecret, nextProto=nextProto):
            yield result
        yield masterSecret

    def _clientGetKeyFromChain(self, certificate, settings, tackExt=None):
        #Get and check cert chain from the Certificate message
        certChain = certificate.certChain
        if not certChain or certChain.getNumCerts() == 0:
            for result in self._sendError(AlertDescription.illegal_parameter,
                    "Other party sent a Certificate message without "\
                    "certificates"):
                yield result

        #Get and check public key from the cert chain
        publicKey = certChain.getEndEntityPublicKey()
        if len(publicKey) < settings.minKeySize:
            for result in self._sendError(AlertDescription.handshake_failure,
                    "Other party's public key too small: %d" % len(publicKey)):
                yield result
        if len(publicKey) > settings.maxKeySize:
            for result in self._sendError(AlertDescription.handshake_failure,
                    "Other party's public key too large: %d" % len(publicKey)):
                yield result
        
        # If there's no TLS Extension, look for a TACK cert
        if tackpyLoaded:
            if not tackExt:
                tackExt = certChain.getTackExt()
         
            # If there's a TACK (whether via TLS or TACK Cert), check that it
            # matches the cert chain   
            if tackExt and tackExt.tacks:
                for tack in tackExt.tacks: 
                    if not certChain.checkTack(tack):
                        for result in self._sendError(  
                                AlertDescription.illegal_parameter,
                                "Other party's TACK doesn't match their public key"):
                                yield result

        yield publicKey, certChain, tackExt


    #*********************************************************
    # Server Handshake Functions
    #*********************************************************


    def handshakeServer(self, verifierDB=None,
                        certChain=None, privateKey=None, reqCert=False,
                        sessionCache=None, settings=None, checker=None,
                        reqCAs = None, reqCertTypes = None,
                        tacks=None, activationFlags=0,
                        nextProtos=None, anon=False,
                        signedCertTimestamps=None,
                        fallbackSCSV=False, ocspResponse=None):
        """Perform a handshake in the role of server.

        This function performs an SSL or TLS handshake.  Depending on
        the arguments and the behavior of the client, this function can
        perform an SRP, or certificate-based handshake.  It
        can also perform a combined SRP and server-certificate
        handshake.

        Like any handshake function, this can be called on a closed
        TLS connection, or on a TLS connection that is already open.
        If called on an open connection it performs a re-handshake.
        This function does not send a Hello Request message before
        performing the handshake, so if re-handshaking is required,
        the server must signal the client to begin the re-handshake
        through some other means.

        If the function completes without raising an exception, the
        TLS connection will be open and available for data transfer.

        If an exception is raised, the connection will have been
        automatically closed (if it was ever open).

        @type verifierDB: L{tlslite.verifierdb.VerifierDB}
        @param verifierDB: A database of SRP password verifiers
        associated with usernames.  If the client performs an SRP
        handshake, the session's srpUsername attribute will be set.

        @type certChain: L{tlslite.x509certchain.X509CertChain}
        @param certChain: The certificate chain to be used if the
        client requests server certificate authentication.

        @type privateKey: L{tlslite.utils.rsakey.RSAKey}
        @param privateKey: The private key to be used if the client
        requests server certificate authentication.

        @type reqCert: bool
        @param reqCert: Whether to request client certificate
        authentication.  This only applies if the client chooses server
        certificate authentication; if the client chooses SRP
        authentication, this will be ignored.  If the client
        performs a client certificate authentication, the sessions's
        clientCertChain attribute will be set.

        @type sessionCache: L{tlslite.sessioncache.SessionCache}
        @param sessionCache: An in-memory cache of resumable sessions.
        The client can resume sessions from this cache.  Alternatively,
        if the client performs a full handshake, a new session will be
        added to the cache.

        @type settings: L{tlslite.handshakesettings.HandshakeSettings}
        @param settings: Various settings which can be used to control
        the ciphersuites and SSL/TLS version chosen by the server.

        @type checker: L{tlslite.checker.Checker}
        @param checker: A Checker instance.  This instance will be
        invoked to examine the other party's authentication
        credentials, if the handshake completes succesfully.
        
        @type reqCAs: list of L{bytearray} of unsigned bytes
        @param reqCAs: A collection of DER-encoded DistinguishedNames that
        will be sent along with a certificate request. This does not affect
        verification.        

        @type reqCertTypes: list of int
        @param reqCertTypes: A list of certificate_type values to be sent
        along with a certificate request. This does not affect verification.

        @type nextProtos: list of strings.
        @param nextProtos: A list of upper layer protocols to expose to the
        clients through the Next-Protocol Negotiation Extension, 
        if they support it.

        @type signedCertTimestamps: str
        @param signedCertTimestamps: A SignedCertificateTimestampList (as a
        binary 8-bit string) that will be sent as a TLS extension whenever
        the client announces support for the extension.

        @type fallbackSCSV: bool
        @param fallbackSCSV: if true, the server will implement
        TLS_FALLBACK_SCSV and thus reject connections using less than the
        server's maximum TLS version that include this cipher suite.

        @type ocspResponse: str
        @param ocspResponse: An OCSP response (as a binary 8-bit string) that
        will be sent stapled in the handshake whenever the client announces
        support for the status_request extension.
        Note that the response is sent independent of the ClientHello
        status_request extension contents, and is thus only meant for testing
        environments. Real OCSP stapling is more complicated as it requires
        choosing a suitable response based on the ClientHello status_request
        extension contents.

        @raise socket.error: If a socket error occurs.
        @raise tlslite.errors.TLSAbruptCloseError: If the socket is closed
        without a preceding alert.
        @raise tlslite.errors.TLSAlert: If a TLS alert is signalled.
        @raise tlslite.errors.TLSAuthenticationError: If the checker
        doesn't like the other party's authentication credentials.
        """
        for result in self.handshakeServerAsync(verifierDB,
                certChain, privateKey, reqCert, sessionCache, settings,
                checker, reqCAs, reqCertTypes,
                tacks=tacks, activationFlags=activationFlags, 
                nextProtos=nextProtos, anon=anon,
                signedCertTimestamps=signedCertTimestamps,
                fallbackSCSV=fallbackSCSV, ocspResponse=ocspResponse):
            pass


    def handshakeServerAsync(self, verifierDB=None,
                             certChain=None, privateKey=None, reqCert=False,
                             sessionCache=None, settings=None, checker=None,
                             reqCAs=None, reqCertTypes=None,
                             tacks=None, activationFlags=0,
                             nextProtos=None, anon=False,
                             signedCertTimestamps=None,
                             fallbackSCSV=False,
                             ocspResponse=None
                             ):
        """Start a server handshake operation on the TLS connection.

        This function returns a generator which behaves similarly to
        handshakeServer().  Successive invocations of the generator
        will return 0 if it is waiting to read from the socket, 1 if it is
        waiting to write to the socket, or it will raise StopIteration
        if the handshake operation is complete.

        @rtype: iterable
        @return: A generator; see above for details.
        """
        handshaker = self._handshakeServerAsyncHelper(\
            verifierDB=verifierDB, certChain=certChain,
            privateKey=privateKey, reqCert=reqCert,
            sessionCache=sessionCache, settings=settings, 
            reqCAs=reqCAs, reqCertTypes=reqCertTypes,
            tacks=tacks, activationFlags=activationFlags, 
            nextProtos=nextProtos, anon=anon,
            signedCertTimestamps=signedCertTimestamps,
            fallbackSCSV=fallbackSCSV,
            ocspResponse=ocspResponse)
        for result in self._handshakeWrapperAsync(handshaker, checker):
            yield result
        if settings and settings.alertAfterHandshake:
            for result in self._sendError(AlertDescription.internal_error,
                                          "Spurious alert"):
                yield result


    def _handshakeServerAsyncHelper(self, verifierDB,
                             certChain, privateKey, reqCert, sessionCache,
                             settings, reqCAs, reqCertTypes,
                             tacks, activationFlags, 
                             nextProtos, anon,
                             signedCertTimestamps, fallbackSCSV,
                             ocspResponse):

        self._handshakeStart(client=False)

        if (not verifierDB) and (not certChain) and not anon:
            raise ValueError("Caller passed no authentication credentials")
        if certChain and not privateKey:
            raise ValueError("Caller passed a certChain but no privateKey")
        if privateKey and not certChain:
            raise ValueError("Caller passed a privateKey but no certChain")
        if reqCAs and not reqCert:
            raise ValueError("Caller passed reqCAs but not reqCert")            
        if reqCertTypes and not reqCert:
            raise ValueError("Caller passed reqCertTypes but not reqCert")
        if certChain and not isinstance(certChain, X509CertChain):
            raise ValueError("Unrecognized certificate type")
        if activationFlags and not tacks:
            raise ValueError("Nonzero activationFlags requires tacks")
        if tacks:
            if not tackpyLoaded:
                raise ValueError("tackpy is not loaded")
            if not settings or not settings.useExperimentalTackExtension:
                raise ValueError("useExperimentalTackExtension not enabled")
        if signedCertTimestamps and not certChain:
            raise ValueError("Caller passed signedCertTimestamps but no "
                             "certChain")

        if not settings:
            settings = HandshakeSettings()
        settings = settings._filter()
        
        # OK Start exchanging messages
        # ******************************
        
        # Handle ClientHello and resumption
        for result in self._serverGetClientHello(settings, certChain,\
                                            verifierDB, sessionCache,
                                            anon, fallbackSCSV):
            if result in (0,1): yield result
            elif result == None:
                self._handshakeDone(resumed=True)                
                return # Handshake was resumed, we're done 
            else: break
        (clientHello, cipherSuite) = result

        # Save the ClientHello for external code to query.
        self.clientHello = clientHello
        
        #If not a resumption...

        # Create the ServerHello message
        if sessionCache:
            sessionID = getRandomBytes(32)
        else:
            sessionID = bytearray(0)
        
        alpn_proto_selected = None
        if (clientHello.alpn_protos_advertised is not None
                and settings.alpnProtos is not None):
            for proto in settings.alpnProtos:
                if proto in clientHello.alpn_protos_advertised:
                    alpn_proto_selected = proto
                    nextProtos = None
                    break;

        if not clientHello.supports_npn:
            nextProtos = None

        # If not doing a certificate-based suite, discard the TACK
        if not cipherSuite in CipherSuite.certAllSuites:
            tacks = None

        # Prepare a TACK Extension if requested
        if clientHello.tack:
            tackExt = TackExtension.create(tacks, activationFlags)
        else:
            tackExt = None
        serverRandom = getRandomBytes(32)
        # See https://tools.ietf.org/html/rfc8446#section-4.1.3
        if settings.simulateTLS13Downgrade:
            serverRandom = serverRandom[:24] + \
                bytearray("\x44\x4f\x57\x4e\x47\x52\x44\x01")
        elif settings.simulateTLS12Downgrade:
            serverRandom = serverRandom[:24] + \
                bytearray("\x44\x4f\x57\x4e\x47\x52\x44\x00")
        serverHello = ServerHello()
        serverHello.create(self.version, serverRandom, sessionID, \
                            cipherSuite, CertificateType.x509, tackExt,
                            alpn_proto_selected,
                            nextProtos)
        serverHello.channel_id = \
            clientHello.channel_id and settings.enableChannelID
        serverHello.extended_master_secret = \
            clientHello.extended_master_secret and \
            settings.enableExtendedMasterSecret
        for param in clientHello.tb_client_params:
            if param in settings.supportedTokenBindingParams:
                serverHello.tb_params = param
                break
        if clientHello.support_signed_cert_timestamps:
            serverHello.signed_cert_timestamps = signedCertTimestamps
        if clientHello.status_request:
            serverHello.status_request = ocspResponse
        if clientHello.ri:
            serverHello.send_ri = True

        # Perform the SRP key exchange
        clientCertChain = None
        if cipherSuite in CipherSuite.srpAllSuites:
            for result in self._serverSRPKeyExchange(clientHello, serverHello, 
                                    verifierDB, cipherSuite, 
                                    privateKey, certChain):
                if result in (0,1): yield result
                else: break
            premasterSecret = result

        # Perform a certificate-based key exchange
        elif cipherSuite in CipherSuite.certAllSuites:
            if cipherSuite in CipherSuite.certSuites:
                keyExchange = RSAKeyExchange(cipherSuite,
                                             clientHello,
                                             serverHello,
                                             privateKey)
            elif cipherSuite in CipherSuite.dheCertSuites:
                keyExchange = DHE_RSAKeyExchange(cipherSuite,
                                                 clientHello,
                                                 serverHello,
                                                 privateKey)
            elif cipherSuite in CipherSuite.ecdheCertSuites:
                keyExchange = ECDHE_RSAKeyExchange(cipherSuite,
                                                   clientHello,
                                                   serverHello,
                                                   privateKey)
            else:
                assert(False)
            for result in self._serverCertKeyExchange(clientHello, serverHello, 
                                        certChain, keyExchange,
                                        reqCert, reqCAs, reqCertTypes, cipherSuite,
                                        settings, ocspResponse):
                if result in (0,1): yield result
                else: break
            (premasterSecret, clientCertChain) = result

        # Perform anonymous Diffie Hellman key exchange
        elif cipherSuite in CipherSuite.anonSuites:
            for result in self._serverAnonKeyExchange(clientHello, serverHello, 
                                        cipherSuite, settings):
                if result in (0,1): yield result
                else: break
            premasterSecret = result
        
        else:
            assert(False)
                        
        # Exchange Finished messages      
        for result in self._serverFinished(premasterSecret, 
                                clientHello.random, serverHello.random,
                                cipherSuite, settings.cipherImplementations,
                                nextProtos, serverHello.channel_id,
                                serverHello.extended_master_secret):
                if result in (0,1): yield result
                else: break
        masterSecret = result

        self.clientRandom = clientHello.random
        self.serverRandom = serverHello.random

        #Create the session object
        self.session = Session()
        if cipherSuite in CipherSuite.certAllSuites:        
            serverCertChain = certChain
        else:
            serverCertChain = None
        srpUsername = None
        serverName = None
        if clientHello.srp_username:
            srpUsername = clientHello.srp_username.decode("utf-8")
        if clientHello.server_name:
            serverName = clientHello.server_name.decode("utf-8")
        self.session.create(masterSecret, serverHello.session_id, cipherSuite,
            srpUsername, clientCertChain, serverCertChain,
            tackExt, serverHello.tackExt!=None, serverName)
            
        #Add the session object to the session cache
        if sessionCache and sessionID:
            sessionCache[sessionID] = self.session

        self._handshakeDone(resumed=False)


    def _isIntolerant(self, settings, clientHello):
        if settings.tlsIntolerant is None:
            return False
        clientVersion = clientHello.client_version
        if clientHello.has_supported_versions:
            clientVersion = (3, 4)
        return clientVersion >= settings.tlsIntolerant


    def _serverGetClientHello(self, settings, certChain, verifierDB,
                                sessionCache, anon, fallbackSCSV):
        #Tentatively set version to most-desirable version, so if an error
        #occurs parsing the ClientHello, this is what we'll use for the
        #error alert
        self.version = settings.maxVersion

        #Get ClientHello
        for result in self._getMsg(ContentType.handshake,
                                   HandshakeType.client_hello):
            if result in (0,1): yield result
            else: break
        clientHello = result

        #If client's version is too low, reject it
        if clientHello.client_version < settings.minVersion:
            self.version = settings.minVersion
            for result in self._sendError(\
                  AlertDescription.protocol_version,
                  "Too old version: %s" % str(clientHello.client_version)):
                yield result

        #If simulating TLS intolerance, reject certain TLS versions.
        elif self._isIntolerant(settings, clientHello):
            if settings.tlsIntoleranceType == "alert":
                for result in self._sendError(\
                    AlertDescription.handshake_failure):
                    yield result
            elif settings.tlsIntoleranceType == "close":
                self._abruptClose()
                raise TLSUnsupportedError("Simulating version intolerance")
            elif settings.tlsIntoleranceType == "reset":
                self._abruptClose(reset=True)
                raise TLSUnsupportedError("Simulating version intolerance")
            else:
                raise ValueError("Unknown intolerance type: '%s'" %
                                 settings.tlsIntoleranceType)

        #If client's version is too high, propose my highest version
        elif clientHello.client_version > settings.maxVersion:
            self.version = settings.maxVersion

        #Detect if the client performed an inappropriate fallback.
        elif fallbackSCSV and clientHello.client_version < settings.maxVersion:
            self.version = clientHello.client_version
            if CipherSuite.TLS_FALLBACK_SCSV in clientHello.cipher_suites:
                for result in self._sendError(\
                        AlertDescription.inappropriate_fallback):
                    yield result

        else:
            #Set the version to the client's version
            self.version = clientHello.client_version

        #Initialize acceptable cipher suites
        cipherSuites = []
        if verifierDB:
            if certChain:
                cipherSuites += \
                    CipherSuite.getSrpCertSuites(settings, self.version)
            cipherSuites += CipherSuite.getSrpSuites(settings, self.version)
        elif certChain:
            cipherSuites += CipherSuite.getEcdheCertSuites(settings, self.version)
            cipherSuites += CipherSuite.getDheCertSuites(settings, self.version)
            cipherSuites += CipherSuite.getCertSuites(settings, self.version)
        elif anon:
            cipherSuites += CipherSuite.getAnonSuites(settings, self.version)
        else:
            assert(False)

        alpn_proto_selected = None
        if (clientHello.alpn_protos_advertised is not None
                and settings.alpnProtos is not None):
            for proto in settings.alpnProtos:
                if proto in clientHello.alpn_protos_advertised:
                    alpn_proto_selected = proto
                    break;

        #If resumption was requested and we have a session cache...
        if clientHello.session_id and sessionCache:
            session = None

            #Check in the session cache
            if sessionCache and not session:
                try:
                    session = sessionCache[clientHello.session_id]
                    if not session.resumable:
                        raise AssertionError()
                    #Check for consistency with ClientHello
                    if session.cipherSuite not in cipherSuites:
                        for result in self._sendError(\
                                AlertDescription.handshake_failure):
                            yield result
                    if session.cipherSuite not in clientHello.cipher_suites:
                        for result in self._sendError(\
                                AlertDescription.handshake_failure):
                            yield result
                    if clientHello.srp_username:
                        if not session.srpUsername or \
                            clientHello.srp_username != bytearray(session.srpUsername, "utf-8"):
                            for result in self._sendError(\
                                    AlertDescription.handshake_failure):
                                yield result
                    if clientHello.server_name:
                        if not session.serverName or \
                            clientHello.server_name != bytearray(session.serverName, "utf-8"):
                            for result in self._sendError(\
                                    AlertDescription.handshake_failure):
                                yield result                    
                except KeyError:
                    pass

            #If a session is found..
            if session:
                #Send ServerHello
                serverHello = ServerHello()
                serverHello.create(self.version, getRandomBytes(32),
                                   session.sessionID, session.cipherSuite,
                                   CertificateType.x509, None,
                                   alpn_proto_selected, None)
                serverHello.extended_master_secret = \
                    clientHello.extended_master_secret and \
                    settings.enableExtendedMasterSecret
                for param in clientHello.tb_client_params:
                    if param in settings.supportedTokenBindingParams:
                          serverHello.tb_params = param
                          break
                if clientHello.ri:
                    serverHello.send_ri = True
                for result in self._sendMsg(serverHello):
                    yield result

                #From here on, the client's messages must have right version
                self._versionCheck = True

                #Calculate pending connection states
                self._calcPendingStates(session.cipherSuite, 
                                        session.masterSecret,
                                        clientHello.random, 
                                        serverHello.random,
                                        settings.cipherImplementations)

                #Exchange ChangeCipherSpec and Finished messages
                for result in self._sendFinished(session.masterSecret):
                    yield result
                for result in self._getFinished(session.masterSecret):
                    yield result

                #Set the session
                self.session = session
                    
                self.clientRandom = clientHello.random
                self.serverRandom = serverHello.random
                yield None # Handshake done!

        #Calculate the first cipher suite intersection.
        #This is the 'privileged' ciphersuite.  We'll use it if we're
        #doing a new negotiation.  In fact,
        #the only time we won't use it is if we're resuming a
        #session, in which case we use the ciphersuite from the session.
        #
        #Given the current ciphersuite ordering, this means we prefer SRP
        #over non-SRP.
        for cipherSuite in cipherSuites:
            if cipherSuite in clientHello.cipher_suites:
                break
        else:
            for result in self._sendError(\
                    AlertDescription.handshake_failure,
                    "No mutual ciphersuite"):
                yield result
        if cipherSuite in CipherSuite.srpAllSuites and \
                            not clientHello.srp_username:
            for result in self._sendError(\
                    AlertDescription.unknown_psk_identity,
                    "Client sent a hello, but without the SRP username"):
                yield result
           
        #If an RSA suite is chosen, check for certificate type intersection
        if cipherSuite in CipherSuite.certAllSuites and CertificateType.x509 \
                                not in clientHello.certificate_types:
            for result in self._sendError(\
                    AlertDescription.handshake_failure,
                    "the client doesn't support my certificate type"):
                yield result

        # If resumption was not requested, or
        # we have no session cache, or
        # the client's session_id was not found in cache:
        yield (clientHello, cipherSuite)

    def _serverSRPKeyExchange(self, clientHello, serverHello, verifierDB, 
                                cipherSuite, privateKey, serverCertChain):

        srpUsername = clientHello.srp_username.decode("utf-8")
        self.allegedSrpUsername = srpUsername
        #Get parameters from username
        try:
            entry = verifierDB[srpUsername]
        except KeyError:
            for result in self._sendError(\
                    AlertDescription.unknown_psk_identity):
                yield result
        (N, g, s, v) = entry

        #Calculate server's ephemeral DH values (b, B)
        b = bytesToNumber(getRandomBytes(32))
        k = makeK(N, g)
        B = (powMod(g, b, N) + (k*v)) % N

        #Create ServerKeyExchange, signing it if necessary
        serverKeyExchange = ServerKeyExchange(cipherSuite, self.version)
        serverKeyExchange.createSRP(N, g, s, B)
        if cipherSuite in CipherSuite.srpCertSuites:
            hashBytes = serverKeyExchange.hash(clientHello.random,
                                               serverHello.random)
            serverKeyExchange.signature = privateKey.sign(hashBytes)

        #Send ServerHello[, Certificate], ServerKeyExchange,
        #ServerHelloDone
        msgs = []
        msgs.append(serverHello)
        if cipherSuite in CipherSuite.srpCertSuites:
            certificateMsg = Certificate(CertificateType.x509)
            certificateMsg.create(serverCertChain)
            msgs.append(certificateMsg)
        msgs.append(serverKeyExchange)
        msgs.append(ServerHelloDone())
        for result in self._sendMsgs(msgs):
            yield result

        #From here on, the client's messages must have the right version
        self._versionCheck = True

        #Get and check ClientKeyExchange
        for result in self._getMsg(ContentType.handshake,
                                  HandshakeType.client_key_exchange,
                                  cipherSuite):
            if result in (0,1): yield result
            else: break
        clientKeyExchange = result
        A = clientKeyExchange.srp_A
        if A % N == 0:
            for result in self._sendError(AlertDescription.illegal_parameter,
                    "Suspicious A value"):
                yield result
            assert(False) # Just to ensure we don't fall through somehow

        #Calculate u
        u = makeU(N, A, B)

        #Calculate premaster secret
        S = powMod((A * powMod(v,u,N)) % N, b, N)
        premasterSecret = numberToByteArray(S)
        
        yield premasterSecret


    def _serverCertKeyExchange(self, clientHello, serverHello, 
                                serverCertChain, keyExchange,
                                reqCert, reqCAs, reqCertTypes, cipherSuite,
                                settings, ocspResponse):
        #Send ServerHello, Certificate[, ServerKeyExchange]
        #[, CertificateRequest], ServerHelloDone
        msgs = []

        # If we verify a client cert chain, return it
        clientCertChain = None

        msgs.append(serverHello)
        msgs.append(Certificate(CertificateType.x509).create(serverCertChain))
        if serverHello.status_request:
            msgs.append(CertificateStatus().create(ocspResponse))
        serverKeyExchange = keyExchange.makeServerKeyExchange()
        if serverKeyExchange is not None:
            msgs.append(serverKeyExchange)
        if reqCert:
            reqCAs = reqCAs or []
            #Apple's Secure Transport library rejects empty certificate_types,
            #so default to rsa_sign.
            reqCertTypes = reqCertTypes or [ClientCertificateType.rsa_sign]
            #Only SHA-1 + RSA is supported.
            sigAlgs = [(HashAlgorithm.sha1, SignatureAlgorithm.rsa)]
            msgs.append(CertificateRequest(self.version).create(reqCertTypes,
                                                                reqCAs,
                                                                sigAlgs))
        msgs.append(ServerHelloDone())
        for result in self._sendMsgs(msgs):
            yield result

        #From here on, the client's messages must have the right version
        self._versionCheck = True

        #Get [Certificate,] (if was requested)
        if reqCert:
            if self.version == (3,0):
                for result in self._getMsg((ContentType.handshake,
                                           ContentType.alert),
                                           HandshakeType.certificate,
                                           CertificateType.x509):
                    if result in (0,1): yield result
                    else: break
                msg = result

                if isinstance(msg, Alert):
                    #If it's not a no_certificate alert, re-raise
                    alert = msg
                    if alert.description != \
                            AlertDescription.no_certificate:
                        self._shutdown(False)
                        raise TLSRemoteAlert(alert)
                elif isinstance(msg, Certificate):
                    clientCertificate = msg
                    if clientCertificate.certChain and \
                            clientCertificate.certChain.getNumCerts()!=0:
                        clientCertChain = clientCertificate.certChain
                else:
                    raise AssertionError()
            elif self.version in ((3,1), (3,2), (3,3)):
                for result in self._getMsg(ContentType.handshake,
                                          HandshakeType.certificate,
                                          CertificateType.x509):
                    if result in (0,1): yield result
                    else: break
                clientCertificate = result
                if clientCertificate.certChain and \
                        clientCertificate.certChain.getNumCerts()!=0:
                    clientCertChain = clientCertificate.certChain
            else:
                raise AssertionError()

        #Get ClientKeyExchange
        for result in self._getMsg(ContentType.handshake,
                                  HandshakeType.client_key_exchange,
                                  cipherSuite):
            if result in (0,1): yield result
            else: break
        clientKeyExchange = result

        #Process ClientKeyExchange
        try:
            premasterSecret = \
                keyExchange.processClientKeyExchange(clientKeyExchange)
        except TLSLocalAlert, alert:
            for result in self._sendError(alert.description, alert.message):
                yield result

        #Get and check CertificateVerify, if relevant
        if clientCertChain:
            if self.version == (3,0):
                masterSecret = calcMasterSecret(self.version, premasterSecret,
                                         clientHello.random, serverHello.random,
                                         b"", False)
                verifyBytes = self._calcSSLHandshakeHash(masterSecret, b"")
            elif self.version in ((3,1), (3,2)):
                verifyBytes = self._handshake_md5.digest() + \
                                self._handshake_sha.digest()
            elif self.version == (3,3):
                verifyBytes = self._handshake_sha.digest()
                verifyBytes = RSAKey.addPKCS1SHA1Prefix(verifyBytes)
            for result in self._getMsg(ContentType.handshake,
                                      HandshakeType.certificate_verify):
                if result in (0,1): yield result
                else: break
            certificateVerify = result
            publicKey = clientCertChain.getEndEntityPublicKey()
            if len(publicKey) < settings.minKeySize:
                for result in self._sendError(\
                        AlertDescription.handshake_failure,
                        "Client's public key too small: %d" % len(publicKey)):
                    yield result

            if len(publicKey) > settings.maxKeySize:
                for result in self._sendError(\
                        AlertDescription.handshake_failure,
                        "Client's public key too large: %d" % len(publicKey)):
                    yield result

            if not publicKey.verify(certificateVerify.signature, verifyBytes):
                for result in self._sendError(\
                        AlertDescription.decrypt_error,
                        "Signature failed to verify"):
                    yield result
        yield (premasterSecret, clientCertChain)


    def _serverAnonKeyExchange(self, clientHello, serverHello, cipherSuite, 
                               settings):
        # Calculate DH p, g, Xs, Ys
        dh_p = getRandomSafePrime(32, False)
        dh_g = getRandomNumber(2, dh_p)        
        dh_Xs = bytesToNumber(getRandomBytes(32))        
        dh_Ys = powMod(dh_g, dh_Xs, dh_p)

        #Create ServerKeyExchange
        serverKeyExchange = ServerKeyExchange(cipherSuite, self.version)
        serverKeyExchange.createDH(dh_p, dh_g, dh_Ys)
        
        #Send ServerHello[, Certificate], ServerKeyExchange,
        #ServerHelloDone  
        msgs = []
        msgs.append(serverHello)
        msgs.append(serverKeyExchange)
        msgs.append(ServerHelloDone())
        for result in self._sendMsgs(msgs):
            yield result
        
        #From here on, the client's messages must have the right version
        self._versionCheck = True
        
        #Get and check ClientKeyExchange
        for result in self._getMsg(ContentType.handshake,
                                   HandshakeType.client_key_exchange,
                                   cipherSuite):
            if result in (0,1):
                yield result 
            else:
                break
        clientKeyExchange = result
        dh_Yc = clientKeyExchange.dh_Yc
        
        if dh_Yc % dh_p == 0:
            for result in self._sendError(AlertDescription.illegal_parameter,
                    "Suspicious dh_Yc value"):
                yield result
            assert(False) # Just to ensure we don't fall through somehow            

        #Calculate premaster secre
        S = powMod(dh_Yc,dh_Xs,dh_p)
        premasterSecret = numberToByteArray(S)
        
        yield premasterSecret


    def _serverFinished(self,  premasterSecret, clientRandom, serverRandom,
                        cipherSuite, cipherImplementations, nextProtos,
                        doingChannelID, useExtendedMasterSecret):
        masterSecret = calcMasterSecret(self.version, premasterSecret,
                                      clientRandom, serverRandom,
                                      self._ems_handshake_hash,
                                      useExtendedMasterSecret)
        
        #Calculate pending connection states
        self._calcPendingStates(cipherSuite, masterSecret, 
                                clientRandom, serverRandom,
                                cipherImplementations)

        #Exchange ChangeCipherSpec and Finished messages
        for result in self._getFinished(masterSecret, 
                        expect_next_protocol=nextProtos is not None,
                        expect_channel_id=doingChannelID):
            yield result

        for result in self._sendFinished(masterSecret):
            yield result
        
        yield masterSecret        


    #*********************************************************
    # Shared Handshake Functions
    #*********************************************************


    def _sendFinished(self, masterSecret, nextProto=None):
        #Send ChangeCipherSpec
        for result in self._sendMsg(ChangeCipherSpec()):
            yield result

        #Switch to pending write state
        self._changeWriteState()

        if nextProto is not None:
            nextProtoMsg = NextProtocol().create(nextProto)
            for result in self._sendMsg(nextProtoMsg):
                yield result

        #Calculate verification data
        verifyData = self._calcFinished(masterSecret, True)
        if self.fault == Fault.badFinished:
            verifyData[0] = (verifyData[0]+1)%256

        #Send Finished message under new state
        finished = Finished(self.version).create(verifyData)
        for result in self._sendMsg(finished):
            yield result

    def _getFinished(self, masterSecret, expect_next_protocol=False, nextProto=None,
                     expect_channel_id=False):
        #Get and check ChangeCipherSpec
        for result in self._getMsg(ContentType.change_cipher_spec):
            if result in (0,1):
                yield result
        changeCipherSpec = result

        if changeCipherSpec.type != 1:
            for result in self._sendError(AlertDescription.illegal_parameter,
                                         "ChangeCipherSpec type incorrect"):
                yield result

        #Switch to pending read state
        self._changeReadState()

        #Server Finish - Are we waiting for a next protocol echo? 
        if expect_next_protocol:
            for result in self._getMsg(ContentType.handshake, HandshakeType.next_protocol):
                if result in (0,1):
                    yield result
            if result is None:
                for result in self._sendError(AlertDescription.unexpected_message,
                                             "Didn't get NextProtocol message"):
                    yield result

            self.next_proto = result.next_proto
        else:
            self.next_proto = None

        #Client Finish - Only set the next_protocol selected in the connection
        if nextProto:
            self.next_proto = nextProto

        #Server Finish - Are we waiting for a EncryptedExtensions?
        if expect_channel_id:
            for result in self._getMsg(ContentType.handshake, HandshakeType.encrypted_extensions):
                if result in (0,1):
                    yield result
            if result is None:
                for result in self._sendError(AlertDescription.unexpected_message,
                                             "Didn't get EncryptedExtensions message"):
                    yield result
            encrypted_extensions = result
            self.channel_id = result.channel_id_key
        else:
            self.channel_id = None

        #Calculate verification data
        verifyData = self._calcFinished(masterSecret, False)

        #Get and check Finished message under new state
        for result in self._getMsg(ContentType.handshake,
                                  HandshakeType.finished):
            if result in (0,1):
                yield result
        finished = result
        if finished.verify_data != verifyData:
            for result in self._sendError(AlertDescription.decrypt_error,
                                         "Finished message is incorrect"):
                yield result

    def _calcFinished(self, masterSecret, send=True):
        if self.version == (3,0):
            if (self._client and send) or (not self._client and not send):
                senderStr = b"\x43\x4C\x4E\x54"
            else:
                senderStr = b"\x53\x52\x56\x52"

            verifyData = self._calcSSLHandshakeHash(masterSecret, senderStr)
            return verifyData

        elif self.version in ((3,1), (3,2)):
            if (self._client and send) or (not self._client and not send):
                label = b"client finished"
            else:
                label = b"server finished"

            handshakeHashes = self._handshake_md5.digest() + \
                                self._handshake_sha.digest()
            verifyData = PRF(masterSecret, label, handshakeHashes, 12)
            return verifyData
        elif self.version == (3,3):
            if (self._client and send) or (not self._client and not send):
                label = b"client finished"
            else:
                label = b"server finished"

            handshakeHashes = self._handshake_sha256.digest()
            verifyData = PRF_1_2(masterSecret, label, handshakeHashes, 12)
            return verifyData
        else:
            raise AssertionError()


    def _handshakeWrapperAsync(self, handshaker, checker):
        if not self.fault:
            try:
                for result in handshaker:
                    yield result
                if checker:
                    try:
                        checker(self)
                    except TLSAuthenticationError:
                        alert = Alert().create(AlertDescription.close_notify,
                                               AlertLevel.fatal)
                        for result in self._sendMsg(alert):
                            yield result
                        raise
            except GeneratorExit:
                raise
            except TLSAlert as alert:
                if not self.fault:
                    raise
                if alert.description not in Fault.faultAlerts[self.fault]:
                    raise TLSFaultError(str(alert))
                else:
                    pass
            except:
                self._shutdown(False)
                raise


    def exportKeyingMaterial(self, label, context, use_context, length):
        """Returns the exported keying material as defined in RFC 5705."""

        seed = self.clientRandom + self.serverRandom
        if use_context:
            if len(context) > 65535:
                raise ValueError("Context is too long")
            seed += bytearray(2)
            seed[len(seed) - 2] = len(context) >> 8
            seed[len(seed) - 1] = len(context) & 0xFF
            seed += context
        if self.version in ((3,1), (3,2)):
            return PRF(self.session.masterSecret, label, seed, length)
        elif self.version == (3,3):
            return PRF_1_2(self.session.masterSecret, label, seed, length)
        else:
            raise AssertionError()
