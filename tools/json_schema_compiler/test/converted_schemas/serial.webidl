// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary DeviceInfo {
  // The device's system path. This should be passed as the <code>path</code>
  // argument to <code>chrome.serial.connect</code> in order to connect to
  // this device.
  required DOMString path;

  // A PCI or USB vendor ID if one can be determined for the underlying
  // device.
  long vendorId;

  // A USB product ID if one can be determined for the underlying device.
  long productId;

  // A human-readable display name for the underlying device if one can be
  // queried from the host driver.
  DOMString displayName;
};

enum DataBits { "seven", "eight" };

enum ParityBit { "no", "odd", "even" };

enum StopBits { "one", "two" };

dictionary ConnectionOptions {
  // Flag indicating whether or not the connection should be left open when
  // the application is suspended (see
  // <a href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is "false." When the application is
  // loaded, any serial connections previously opened with persistent=true
  // can be fetched with <code>getConnections</code>.
  boolean persistent;

  // An application-defined string to associate with the connection.
  DOMString name;

  // The size of the buffer used to receive data. The default value is 4096.
  long bufferSize;

  // The requested bitrate of the connection to be opened. For compatibility
  // with the widest range of hardware, this number should match one of
  // commonly-available bitrates, such as 110, 300, 1200, 2400, 4800, 9600,
  // 14400, 19200, 38400, 57600, 115200. There is no guarantee, of course,
  // that the device connected to the serial port will support the requested
  // bitrate, even if the port itself supports that bitrate. <code>9600</code>
  // will be passed by default.
  long bitrate;

  // <code>"eight"</code> will be passed by default.
  DataBits dataBits;

  // <code>"no"</code> will be passed by default.
  ParityBit parityBit;

  // <code>"one"</code> will be passed by default.
  StopBits stopBits;

  // Flag indicating whether or not to enable RTS/CTS hardware flow control.
  // Defaults to false.
  boolean ctsFlowControl;

  // The maximum amount of time (in milliseconds) to wait for new data before
  // raising an <code>onReceiveError</code> event with a "timeout" error.
  // If zero, receive timeout errors will not be raised for the connection.
  // Defaults to 0.
  long receiveTimeout;

  // The maximum amount of time (in milliseconds) to wait for a
  // <code>send</code> operation to complete before calling the callback with
  // a "timeout" error. If zero, send timeout errors will not be triggered.
  // Defaults to 0.
  long sendTimeout;
};

// Result of the <code>getInfo</code> method.
dictionary ConnectionInfo {
  // The id of the serial port connection.
  required long connectionId;

  // Flag indicating whether the connection is blocked from firing onReceive
  // events.
  required boolean paused;

  // See <code>ConnectionOptions.persistent</code>
  required boolean persistent;

  // See <code>ConnectionOptions.name</code>
  required DOMString name;

  // See <code>ConnectionOptions.bufferSize</code>
  required long bufferSize;

  // See <code>ConnectionOptions.receiveTimeout</code>
  required long receiveTimeout;

  // See <code>ConnectionOptions.sendTimeout</code>
  required long sendTimeout;

  // See <code>ConnectionOptions.bitrate</code>. This field may be omitted
  // or inaccurate if a non-standard bitrate is in use, or if an error
  // occurred while querying the underlying device.
  long bitrate;

  // See <code>ConnectionOptions.dataBits</code>. This field may be omitted
  // if an error occurred while querying the underlying device.
  DataBits dataBits;

  // See <code>ConnectionOptions.parityBit</code>. This field may be omitted
  // if an error occurred while querying the underlying device.
  ParityBit parityBit;

  // See <code>ConnectionOptions.stopBits</code>. This field may be omitted
  // if an error occurred while querying the underlying device.
  StopBits stopBits;

  // See <code>ConnectionOptions.ctsFlowControl</code>. This field may be
  // omitted if an error occurred while querying the underlying device.
  boolean ctsFlowControl;
};

enum SendError {
  // The connection was disconnected.
  "disconnected",

  // A send was already pending.
  "pending",

  // The send timed out.
  "timeout",

  // A system error occurred and the connection may be unrecoverable.
  "system_error"
};

dictionary SendInfo {
  // The number of bytes sent.
  required long bytesSent;

  // An error code if an error occurred.
  SendError error;
};

// The set of control signals which may be sent to a connected serial device
// using <code>setControlSignals</code>. Note that support for these signals
// is device-dependent.
dictionary HostControlSignals {
  // DTR (Data Terminal Ready).
  boolean dtr;

  // RTS (Request To Send).
  boolean rts;
};

// The set of control signals which may be set by a connected serial device.
// These can be queried using <code>getControlSignals</code>. Note that
// support for these signals is device-dependent.
dictionary DeviceControlSignals {
  // DCD (Data Carrier Detect) or RLSD (Receive Line Signal/ Detect).
  required boolean dcd;

  // CTS (Clear To Send).
  required boolean cts;

  // RI (Ring Indicator).
  required boolean ri;

  // DSR (Data Set Ready).
  required boolean dsr;
};

// Data from an <code>onReceive</code> event.
dictionary ReceiveInfo {
  // The connection identifier.
  required long connectionId;

  // The data received.
  required ArrayBuffer data;
};

enum ReceiveError {
  // The connection was disconnected.
  "disconnected",

  // No data has been received for <code>receiveTimeout</code> milliseconds.
  "timeout",

  // The device was most likely disconnected from the host.
  "device_lost",

  // The device detected a break condition.
  "break",

  // The device detected a framing error.
  "frame_error",

  // A character-buffer overrun has occurred. The next character is lost.
  "overrun",

  // An input buffer overflow has occurred. There is either no room in the
  // input buffer, or a character was received after the end-of-file (EOF)
  // character.
  "buffer_overflow",

  // The device detected a parity error.
  "parity_error",

  // A system error occurred and the connection may be unrecoverable.
  "system_error"
};

// Data from an <code>onReceiveError</code> event.
dictionary ReceiveErrorInfo {
  // The connection identifier.
  required long connectionId;

  // An error code indicating what went wrong.
  required ReceiveError error;
};

// |info| : Event data.
callback OnReceiveListener = undefined(ReceiveInfo info);

interface OnReceiveEvent : ExtensionEvent {
  static undefined addListener(OnReceiveListener listener);
  static undefined removeListener(OnReceiveListener listener);
  static boolean hasListener(OnReceiveListener listener);
};

// Listener callback for the onReceiveError event.
callback OnReceiveErrorListener = undefined(ReceiveErrorInfo info);

interface OnReceiveErrorEvent : ExtensionEvent {
  static undefined addListener(OnReceiveErrorListener listener);
  static undefined removeListener(OnReceiveErrorListener listener);
  static boolean hasListener(OnReceiveErrorListener listener);
};

// Use the <code>chrome.serial</code> API to read from and write to a device
// connected to a serial port.
interface Serial {
  // Returns information about available serial devices on the system.
  // The list is regenerated each time this method is called.
  // |Returns| : Called with the list of <code>DeviceInfo</code> objects.
  // |PromiseValue| : ports
  [requiredCallback] static Promise<sequence<DeviceInfo>> getDevices();

  // Connects to a given serial port.
  // |path| : The system path of the serial port to open.
  // |options| : Port configuration options.
  // |Returns| : Called when the connection has been opened.
  // |PromiseValue| : connectionInfo
  [requiredCallback] static Promise<ConnectionInfo> connect(
      DOMString path, optional ConnectionOptions options);

  // Update the option settings on an open serial port connection.
  // |connectionId| : The id of the opened connection.
  // |options| : Port configuration options.
  // |Returns| : Called when the configuation has completed.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> update(long connectionId,
                                                    ConnectionOptions options);

  // Disconnects from a serial port.
  // |connectionId| : The id of the opened connection.
  // |Returns| : Called when the connection has been closed.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> disconnect(long connectionId);

  // Pauses or unpauses an open connection.
  // |connectionId| : The id of the opened connection.
  // |paused| : Flag to indicate whether to pause or unpause.
  // |Returns| : Called when the connection has been successfully paused or
  //              unpaused.
  [requiredCallback] static Promise<undefined> setPaused(long connectionId,
                                                         boolean paused);

  // Retrieves the state of a given connection.
  // |connectionId| : The id of the opened connection.
  // |Returns| : Called with connection state information when available.
  // |PromiseValue| : connectionInfo
  [requiredCallback] static Promise<ConnectionInfo> getInfo(long connectionId);

  // Retrieves the list of currently opened serial port connections owned by
  // the application.
  // |Returns| : Called with the list of connections when available.
  // |PromiseValue| : connectionInfos
  [requiredCallback] static Promise<sequence<ConnectionInfo>> getConnections();

  // Writes data to the given connection.
  // |connectionId| : The id of the connection.
  // |data| : The data to send.
  // |Returns| : Called when the operation has completed.
  // |PromiseValue| : sendInfo
  [requiredCallback] static Promise<SendInfo> send(long connectionId,
                                                   ArrayBuffer data);

  // Flushes all bytes in the given connection's input and output buffers.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> flush(long connectionId);

  // Retrieves the state of control signals on a given connection.
  // |connectionId| : The id of the connection.
  // |Returns| : Called when the control signals are available.
  // |PromiseValue| : signals
  [requiredCallback] static Promise<DeviceControlSignals> getControlSignals(
      long connectionId);

  // Sets the state of control signals on a given connection.
  // |connectionId| : The id of the connection.
  // |signals| : The set of signal changes to send to the device.
  // |Returns| : Called once the control signals have been set.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> setControlSignals(
      long connectionId, HostControlSignals signals);

  // Suspends character transmission on a given connection and places the
  // transmission line in a break state until the clearBreak is called.
  // |connectionId| : The id of the connection.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> setBreak(long connectionId);

  // Restore character transmission on a given connection and place the
  // transmission line in a nonbreak state.
  // |connectionId| : The id of the connection.
  // |PromiseValue| : result
  [requiredCallback] static Promise<boolean> clearBreak(long connectionId);

  // Event raised when data has been read from the connection.
  static attribute OnReceiveEvent onReceive;

  // Event raised when an error occurred while the runtime was waiting for
  // data on the serial port. Once this event is raised, the connection may be
  // set to <code>paused</code>. A <code>"timeout"</code> error does not pause
  // the connection.
  static attribute OnReceiveErrorEvent onReceiveError;
};

partial interface Browser {
  static attribute Serial serial;
};
