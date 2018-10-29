/**
 * @fileoverview Closure definitions of Mojo core IDL and bindings objects.
 */

const Mojo = {};

/**
 * @param {string} name
 * @param {MojoHandle} handle
 */
Mojo.bindInterface = function(name, handle) {};

const MojoHandle = class {};

const mojo = {};

// Core Mojo.

mojo.Binding = class {
  /**
   * @param {!mojo.Interface} interfaceType
   * @param {!Object} impl
   * @param {mojo.InterfaceRequest=} request
   */
  constructor(interfaceType, impl, request) {};

  /**
   * Closes the message pipe. The bound object will no longer receive messages.
   */
  close() {}

  /**
   * Binds to the given request.
   * @param {!mojo.InterfaceRequest} request
   */
  bind(request) {}

  /** @param {function()} callback */
  setConnectionErrorHandler(callback) {}

  /**
   * Creates an interface ptr and bind it to this instance.
   * @return {!mojo.InterfacePtr} The interface ptr.
   */
  createInterfacePtrAndBind() {}
};


mojo.InterfaceRequest = class {
  constructor() {
    /** @type {MojoHandle} */
    this.handle;
  }

  /**
   * Closes the message pipe. The object can no longer be bound to an
   * implementation.
   */
  close() {}
};


/** @interface */
mojo.InterfacePtr = class {};


mojo.InterfacePtrController = class {
  /**
   * Closes the message pipe. Messages can no longer be sent with this object.
   */
  reset() {}

  /** @param {function()} callback */
  setConnectionErrorHandler(callback) {}
};


mojo.Interface = class {
  constructor() {
    /** @type {string} */
    this.name;

    /** @type {number} */
    this.kVersion;

    /** @type {function()} */
    this.ptrClass;

    /** @type {function()} */
    this.proxyClass;

    /** @type {function()} */
    this.stubClass;

    /** @type {function()} */
    this.validateRequest;

    /** @type {function()} */
    this.validateResponse;
  }
}

/**
 * @param {!mojo.InterfacePtr} interfacePtr
 * @return {!mojo.InterfaceRequest}
 */
mojo.makeRequest = function(interfacePtr) {};

/** @const */
mojo.internal = {};

mojo.internal.InterfaceProxyBase = class {
  /**
   * @param {MojoHandle=} opt_handle
   */
  constructor(opt_handle) {}

  /**
   * @param {!MojoHandle} handle
   */
  bindHandle(handle) {}

  unbind() {}

  /**
   * @param {number} ordinal
   * @param {!Object} paramStruct
   * @param {!Object} responseStruct
   * @param {!Array} args
   * @return {!Promise}
   */
  sendMessage(ordinal, paramStruct, responseStruct, args) {}
};

mojo.internal.InterfaceTarget = class {
  constructor() {}

  /**
   * @param {number} ordinal
   * @param {!Object} paramStruct
   * @param {!Object} responseStruct
   * @param {!Function} handler
   */
  registerHandler(ordinal, paramStruct, responseStruct, handler) {}
};

mojo.internal.InterfaceCallbackTarget = class {
  constructor() {}

  /**
   * @param {!Function} listener
   * @return {number}
   */
  addListener(listener) {}

  /**
   * @param {number} id
   * @return {boolean}
   */
  removeListener(id) {}

  /**
   * @param {boolean} expectsResponse
   * @return {!Function}
   */
  createTargetHandler(expectsResponse) {}
};
