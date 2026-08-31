/*
 * geolocation-mock contains a mock implementation of Geolocation and
 * PermissionService.
 */

import {GeolocationReceiver} from '/gen/services/device/public/mojom/geolocation.mojom.m.js';
import {GeopositionErrorCode} from '/gen/services/device/public/mojom/geoposition.mojom.m.js';
import {GeolocationService, GeolocationServiceReceiver} from '/gen/third_party/blink/public/mojom/geolocation/geolocation_service.mojom.m.js';
import {PermissionObserverRemote, PermissionService, PermissionServiceReceiver} from '/gen/third_party/blink/public/mojom/permissions/permission.mojom.m.js';
import {GeolocationAccuracy, PermissionStatus} from '/gen/third_party/blink/public/mojom/permissions/permission_status.mojom.m.js';

export const GeolocationPermissionStatus = {
  ASK: 'ASK',
  DENIED: 'DENIED',
  GRANTED_PRECISE: 'GRANTED_PRECISE',
  GRANTED_APPROXIMATE: 'GRANTED_APPROXIMATE',
};

export class GeolocationMock {
  constructor() {
    this.geolocationServiceInterceptor_ =
        new MojoInterfaceInterceptor(GeolocationService.$interfaceName);
    this.geolocationServiceInterceptor_.oninterfacerequest =
        e => this.connectGeolocationService_(e.handle);
    this.geolocationServiceInterceptor_.start();

    this.permissionServiceInterceptor_ =
        new MojoInterfaceInterceptor(PermissionService.$interfaceName);
    this.permissionServiceInterceptor_.oninterfacerequest = e =>
        this.connectPermissionService_(e.handle);
    this.permissionServiceInterceptor_.start();

    /**
     * The next result to return in response to a queryNextPosition()
     * call.
    */
    this.result_ = null;
    this.cachedResult_ = null;

    /**
     * While true, position requests will result in a timeout error.
     */
    this.shouldTimeout_ = false;

    /**
     * A pending request for permission awaiting a decision to be set via a
     * setGeolocationPermission call.
     *
     * @type {?Function}
     */
    this.pendingPermissionRequest_ = null;

    /**
     * The status to respond to permission requests with.
     *
     * @type {!GeolocationPermissionStatus}
     */
    this.permissionStatus_ = GeolocationPermissionStatus.ASK;
    this.rejectGeolocationServiceConnections_ = false;

    this.systemPermissionStatus_ = PermissionStatus.GRANTED;

    /**
     * Set by interceptQueryNextPosition() and used to resolve the promise
     * returned by that call once the next incoming queryNextPosition() is
     * received.
     */
    this.queryNextPositionIntercept_ = null;

    this.permissionObservers_ = [];
    this.geolocationReceiver_ = new GeolocationReceiver(this);
    this.geolocationServiceReceiver_ = new GeolocationServiceReceiver(this);
    this.permissionServiceReceiver_ = new PermissionServiceReceiver(this);
  }

  connectPermissionService_(handle) {
    this.permissionServiceReceiver_.$.bindHandle(handle);
  }

  hasPermission(permission) {
    let status = PermissionStatus.ASK;
    let accuracy = GeolocationAccuracy.kPrecise;
    if (this.permissionStatus_ ===
        GeolocationPermissionStatus.GRANTED_PRECISE) {
      status = PermissionStatus.GRANTED;
      accuracy = GeolocationAccuracy.kPrecise;
    } else if (this.permissionStatus_ ===
               GeolocationPermissionStatus.GRANTED_APPROXIMATE) {
      status = PermissionStatus.GRANTED;
      accuracy = GeolocationAccuracy.kApproximate;
    } else if (this.permissionStatus_ === GeolocationPermissionStatus.DENIED) {
      status = PermissionStatus.DENIED;
    }
    return Promise.resolve({
      status: {
        status: status,
        details: {
          geolocationAccuracy: accuracy,
        },
      }
    });
  }

  registerPageEmbeddedPermissionControl(permissions, descriptor, client) {}

  requestPageEmbeddedPermission(permissions, descriptor) {
    return Promise.resolve({status: 1});
  }

  requestPermission(permission) {
    return this.hasPermission(permission);
  }

  requestPermissions(permissions) {
    return Promise.resolve({
      statuses: permissions.map(() => ({
                                  status: PermissionStatus.GRANTED,
                                  details: null,
                                }))
    });
  }

  revokePermission(permission) {
    return this.hasPermission(permission);
  }

  addPermissionObserver(permission, last_known_status, observer) {
    this.permissionObservers_.push(observer);
  }

  addPageEmbeddedPermissionObserver(permission, last_known_status, observer) {
    this.permissionObservers_.push(observer);
  }

  notifyEventListener(permission, eventType, isAdded) {}

  connectGeolocationService_(handle) {
    if (this.rejectGeolocationServiceConnections_) {
      handle.close();
      return;
    }
    this.geolocationServiceReceiver_.$.bindHandle(handle);
  }

  setHighAccuracyHint(highAccuracy) {
    // FIXME: We need to add some tests regarding "high accuracy" mode.
    // See https://bugs.webkit.org/show_bug.cgi?id=49438
  }

  /**
   * Waits for the next queryPosition() call, and returns a function which can
   * be used to respond to it. This allows tests to have fine-grained control
   * over exactly when and how the mock responds to a specific request.
   */
  async interceptQueryNextPosition() {
    if (this.queryNextPositionIntercept_) {
      throw new Error(
          'interceptQueryNextPosition called twice in a row, with no interim ' +
          'queryPosition');
    }
    return new Promise(resolve => {
      this.queryNextPositionIntercept_ = resolver => {
        this.queryNextPositionIntercept_ = null;
        resolve(result => { resolver({result}); });
      };
    });
  }

  /**
   * A mock implementation of GeolocationService.queryNextPosition(). This
   * returns the position set by a call to setGeolocationPosition() or
   * setGeolocationPositionUnavailableError().
   */
  queryNextPosition() {
    if (this.shouldTimeout_) {
      // Return a promise that will never be resolved. Since no result is
      // returned, the request will eventually time out.
      return new Promise((resolve, reject) => {});
    }
    if (this.queryNextPositionIntercept_) {
      return new Promise(resolve => {
        this.queryNextPositionIntercept_(resolve);
      });
    }

    if (this.systemPermissionStatus_ != PermissionStatus.GRANTED) {
      const error = {
        errorMessage: "User has not allowed access to system location.",
        errorCode: GeopositionErrorCode.kPermissionDenied,
        errorTechnical: "",
      };
      this.result_ = {error};
    }

    if (!this.result_) {
      this.setGeolocationPositionUnavailableError(
          'Test error: position not set before call to queryNextPosition()');
    }
    let result = this.result_;
    this.result_ = null;
    return Promise.resolve({result});
  }

  /**
   * A mock implementation of GeolocationService.queryCachedPosition(). This
   * returns the current cached location or kPositionUnavailable error.
   */
  queryCachedPosition() {
    if (this.cachedResult_ && this.cachedResult_.position) {
      return Promise.resolve({result: this.cachedResult_});
    }

    const error = {
      errorMessage: "",
      errorCode: GeopositionErrorCode.kPositionUnavailable,
      errorTechnical: "",
    };
    return Promise.resolve({result: {error}});
  }

  makeGeoposition(latitude, longitude, accuracy, altitude = undefined,
                  altitudeAccuracy = undefined, heading = undefined,
                  speed = undefined, accuracyMode = 'precise') {
    // The new Date().getTime() returns the number of milliseconds since the
    // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
    // device.mojom.Geoposition represents the value of microseconds since the
    // Windows FILETIME epoch (1601-01-01 00:00:00 UTC). So add the delta when
    // sets the |internalValue|. See more info in //base/time/time.h.
    const windowsEpoch = Date.UTC(1601,0,1,0,0,0,0);
    const unixEpoch = Date.UTC(1970,0,1,0,0,0,0);
    // |epochDeltaInMs| equals to
    // base::Time::kMicrosecondsFromWindowsToUnixEpoch.
    const epochDeltaInMs = unixEpoch - windowsEpoch;
    const timestamp =
        {internalValue: BigInt((new Date().getTime() + epochDeltaInMs) * 1000)};
    return {
      latitude,
      longitude,
      accuracy,
      altitude,
      altitudeAccuracy,
      heading,
      speed,
      timestamp,
      accuracyMode
    };
  }

  /**
   * Sets the position to return to the next queryNextPosition() call. If any
   * queryNextPosition() requests are outstanding, they will all receive the
   * position set by this call.
   */
  setGeolocationPosition(latitude, longitude, accuracy, altitude,
                         altitudeAccuracy, heading, speed,
                         accuracyMode = 'precise') {
    const position =
        this.makeGeoposition(latitude, longitude, accuracy, altitude,
                             altitudeAccuracy, heading, speed, accuracyMode);
    this.result_ = {position};
    this.cachedResult_ = {position};
  }

  /**
   * Sets the error message to return to the next queryNextPosition() call. If
   * any queryNextPosition() requests are outstanding, they will all receive
   * the error set by this call.
   */
  setGeolocationPositionUnavailableError(message) {
    const error = {
      errorMessage: message,
      errorCode: GeopositionErrorCode.kPositionUnavailable,
      errorTechnical: "",
    };
    this.result_ = {error};
    this.cachedResult_ = {error};
  }

  /**
   * Sets whether geolocation requests should cause timeout errors.
   */
  setGeolocationTimeoutError(shouldTimeout) {
    this.shouldTimeout_ = shouldTimeout;
  }

  /**
   * Reject any connection requests for the geolocation service. This will
   * trigger a connection error in the client.
   */
  rejectGeolocationServiceConnections() {
    this.rejectGeolocationServiceConnections_ = true;
  }

  /**
   * A mock implementation of GeolocationService.createGeolocation().
   * This accepts the request as long as the permission has been set to
   * granted.
   */
  createGeolocation(receiver, user_gesture, accuracy) {
    if (this.permissionStatus_ === GeolocationPermissionStatus.ASK) {
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          resolve(this.createGeolocation(receiver, user_gesture, accuracy));
        }, 50);
      });
    } else if (this.permissionStatus_ ===
                   GeolocationPermissionStatus.GRANTED_PRECISE ||
               this.permissionStatus_ ===
                   GeolocationPermissionStatus.GRANTED_APPROXIMATE) {
      this.geolocationReceiver_.$.bindHandle(receiver.handle);
      return Promise.resolve(PermissionStatus.GRANTED);
    } else {
      receiver.handle.close();
    }
    return Promise.resolve(PermissionStatus.DENIED);
  }

  /**
   * Sets whether the next geolocation permission request should be allowed.
   *
   * @param {!GeolocationPermissionStatus} status
   */
  async setGeolocationPermission(status) {
    this.permissionStatus_ = status;

    let permissionStatus = PermissionStatus.ASK;
    let accuracy = GeolocationAccuracy.kPrecise;
    if (this.permissionStatus_ ===
        GeolocationPermissionStatus.GRANTED_PRECISE) {
      permissionStatus = PermissionStatus.GRANTED;
      accuracy = GeolocationAccuracy.kPrecise;
    } else if (this.permissionStatus_ ===
               GeolocationPermissionStatus.GRANTED_APPROXIMATE) {
      permissionStatus = PermissionStatus.GRANTED;
      accuracy = GeolocationAccuracy.kApproximate;
    } else if (this.permissionStatus_ === GeolocationPermissionStatus.DENIED) {
      permissionStatus = PermissionStatus.DENIED;
    }

    const flushPromises = [];
    for (const observer of this.permissionObservers_) {
      observer.onPermissionStatusChange({
        status: permissionStatus,
        details: {
          geolocationAccuracy: accuracy,
        },
      });
      flushPromises.push(observer.$.flushForTesting().catch(() => {}));
    }
    await Promise.all(flushPromises);
  }

  setSystemGeolocationPermission(allowed) {
    this.systemPermissionStatus_ = allowed ? PermissionStatus.GRANTED
                                           : PermissionStatus.DENIED;
  }
}
