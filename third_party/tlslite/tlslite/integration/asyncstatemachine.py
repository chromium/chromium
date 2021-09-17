# Author: Trevor Perrin
# See the LICENSE file for legal information regarding use of this file.

"""
A state machine for using TLS Lite with asynchronous I/O.
"""

class AsyncStateMachine:
    """
    This is an abstract class that's used to integrate TLS Lite with
    asyncore and Twisted.

    This class signals wantsReadsEvent() and wantsWriteEvent().  When
    the underlying socket has become readable or writeable, the event
    should be passed to this class by calling inReadEvent() or
    inWriteEvent().  This class will then try to read or write through
    the socket, and will update its state appropriately.

    This class will forward higher-level events to its subclass.  For
    example, when a complete TLS record has been received,
    outReadEvent() will be called with the decrypted data.
    """

    def __init__(self):
        self._clear()

    def _clear(self):
        #These store the various asynchronous operations (i.e.
        #generators).  Only one of them, at most, is ever active at a
        #time.
        self.handshaker = None
        self.closer = None
        self.reader = None
        self.writer = None

        #This stores the result from the last call to the
        #currently active operation.  If 0 it indicates that the
        #operation wants to read, if 1 it indicates that the
        #operation wants to write.  If None, there is no active
        #operation.
        self.result = None

    def _checkAssert(self, maxActive=1):
        #This checks that only one operation, at most, is
        #active, and that self.result is set appropriately.
        activeOps = 0
        if self.handshaker:
            activeOps += 1
        if self.closer:
            activeOps += 1
        if self.reader:
            activeOps += 1
        if self.writer:
            activeOps += 1

        if self.result == None:
            if activeOps != 0:
                raise AssertionError()
        elif self.result in (0,1):
            if activeOps != 1:
                raise AssertionError()
        else:
            raise AssertionError()
        if activeOps > maxActive:
            raise AssertionError()

    def wantsReadEvent(self):
        """If the state machine wants to read.

        If an operation is active, this returns whether or not the
        operation wants to read from the socket.  If an operation is
        not active, this returns None.

        @rtype: bool or None
        @return: If the state machine wants to read.
        """
        if self.result != None:
            return self.result == 0
        return None

    def wantsWriteEvent(self):
        """If the state machine wants to write.

        If an operation is active, this returns whether or not the
        operation wants to write to the socket.  If an operation is
        not active, this returns None.

        @rtype: bool or None
        @return: If the state machine wants to write.
        """
        if self.result != None:
            return self.result == 1
        return None

    def outConnectEvent(self):
        """Called when a handshake operation completes.

        May be overridden in subclass.
        """
        pass

    def outCloseEvent(self):
        """Called when a close operation completes.

        May be overridden in subclass.
        """
        pass

    def outReadEvent(self, readBuffer):
        """Called when a read operation completes.

        May be overridden in subclass."""
        pass

    def outWriteEvent(self):
        """Called when a write operation completes.

        May be overridden in subclass."""
        pass

    def inReadEvent(self):
        """Tell the state machine it can read from the socket."""
        try:
            self._checkAssert()
            if self.handshaker:
                self._doHandshakeOp()
            elif self.closer:
                self._doCloseOp()
            elif self.reader:
                self._doReadOp()
            elif self.writer:
                self._doWriteOp()
            else:
                self.reader = self.tlsConnection.readAsync(16384)
                self._doReadOp()
        except:
            self._clear()
            raise

    def inWriteEvent(self):
        """Tell the state machine it can write to the socket."""
        try:
            self._checkAssert()
            if self.handshaker:
                self._doHandshakeOp()
            elif self.closer:
                self._doCloseOp()
            elif self.reader:
                self._doReadOp()
            elif self.writer:
                self._doWriteOp()
            else:
                self.outWriteEvent()
        except:
            self._clear()
            raise

    def _doHandshakeOp(self):
        try:
            self.result = self.handshaker.next()
        except StopIteration:
            self.handshaker = None
            self.result = None
            self.outConnectEvent()

    def _doCloseOp(self):
        try:
            self.result = self.closer.next()
        except StopIteration:
            self.closer = None
            self.result = None
            self.outCloseEvent()

    def _doReadOp(self):
        self.result = self.reader.next()
        if not self.result in (0,1):
            readBuffer = self.result
            self.reader = None
            self.result = None
            self.outReadEvent(readBuffer)

    def _doWriteOp(self):
        try:
            self.result = self.writer.next()
        except StopIteration:
            self.writer = None
            self.result = None

    def setHandshakeOp(self, handshaker):
        """Start a handshake operation.

        @type handshaker: generator
        @param handshaker: A generator created by using one of the
        asynchronous handshake functions (i.e. handshakeServerAsync, or
        handshakeClientxxx(..., is_async=True).
        """
        try:
            self._checkAssert(0)
            self.handshaker = handshaker
            self._doHandshakeOp()
        except:
            self._clear()
            raise

    def setServerHandshakeOp(self, **args):
        """Start a handshake operation.

        The arguments passed to this function will be forwarded to
        L{tlslite.tlsconnection.TLSConnection.handshakeServerAsync}.
        """
        handshaker = self.tlsConnection.handshakeServerAsync(**args)
        self.setHandshakeOp(handshaker)

    def setCloseOp(self):
        """Start a close operation.
        """
        try:
            self._checkAssert(0)
            self.closer = self.tlsConnection.closeAsync()
            self._doCloseOp()
        except:
            self._clear()
            raise

    def setWriteOp(self, writeBuffer):
        """Start a write operation.

        @type writeBuffer: str
        @param writeBuffer: The string to transmit.
        """
        try:
            self._checkAssert(0)
            self.writer = self.tlsConnection.writeAsync(writeBuffer)
            self._doWriteOp()
        except:
            self._clear()
            raise

