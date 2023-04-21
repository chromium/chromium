/*
 * geolocation-mock contains a mock implementation of Geolocation and
 * PermissionService.
 */

import {GeolocationReceiver} from '/gen/services/device/public/mojom/geolocation.mojom.m.js';
import {GeopositionErrorCode} from '/gen/services/device/public/mojom/geoposition.mojom.m.js';
import {GeolocationService, GeolocationServiceReceiver} from '/gen/third_party/blink/public/mojom/geolocation/geolocation_service.mojom.m.js';
import {PermissionStatus} from '/gen/third_party/blink/public/mojom/permissions/permission_status.mojom.m.js';

export class GeolocationMock {
  constructor() {
    this.geolocationServiceInterceptor_ =
        new MojoInterfaceInterceptor(GeolocationService.$interfaceName);
    this.geolocationServiceInterceptor_.oninterfacerequest =
        e => this.connectGeolocationService_(e.handle);
    this.geolocationServiceInterceptor_.start();

    /**
     * The next result to return in response to a queryNextPosition()
     * call.
    */
    this.result_ = null;

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
     * The status to respond to permission requests with. If set to ASK, then
     * permission requests will block until setGeolocationPermission is called
     * to allow or deny permission requests.
     *
     * @type {!PermissionStatus}
     */
    this.permissionStatus_ = PermissionStatus.ASK;
    this.rejectGeolocationServiceConnections_ = false;

    this.systemPermissionStatus_ = PermissionStatus.GRANTED;

    /**
     * Set by interceptQueryNextPosition() and used to resolve the promise
     * returned by that call once the next incoming queryNextPosition() is
     * received.
     */
    this.queryNextPositionIntercept_ = null;

    this.geolocationReceiver_ = new GeolocationReceiver(this);
    this.geolocationServiceReceiver_ = new GeolocationServiceReceiver(this);
  }

  connectGeolocationService_(handle) {
    if (this.rejectGeolocationServiceConnections_) {
      handle.close();
      return;
    }
    this.geolocationServiceReceiver_.$.bindHandle(handle);
  }

  setHighAccuracy(highAccuracy) {
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

  makeGeoposition(latitude, longitude, accuracy, altitude = undefined,
                  altitudeAccuracy = undefined, heading = undefined,
                  speed = undefined) {
    // The new Date().getTime() returns the number of milliseconds since the
    // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
    // device.mojom.Geoposition represents the value of microseconds since the
    // Windows FILETIME epoch (1601-01-01 00:00:00 UTC). So add the delta when
    // sets the |internalValue|. See more info in //base/time/time.h.
    const windowsEpoch = Date.UTC(1601,0,1,0,0,0,0);
    const unixEpoch = Date.UTC(1970,0,1,0,0,0,0);
    // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
    const epochDeltaInMs = unixEpoch - windowsEpoch;
    const timestamp =
        {internalValue: BigInt((new Date().getTime() + epochDeltaInMs) * 1000)};
    return {latitude, longitude, accuracy, altitude, altitudeAccuracy, heading,
            speed, timestamp};
  }

  /**
   * Sets the position to return to the next queryNextPosition() call. If any
   * queryNextPosition() requests are outstanding, they will all receive the
   * position set by this call.
   */
  setGeolocationPosition(latitude, longitude, accuracy, altitude,
                         altitudeAccuracy, heading, speed) {
    const position = this.makeGeoposition(latitude, longitude, accuracy,
        altitude, altitudeAccuracy, heading, speed);
    this.result_ = {position};
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
  createGeolocation(receiver, user_gesture) {
    switch (this.permissionStatus_) {
     case PermissionStatus.ASK:
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          resolve(this.createGeolocation(receiver, user_gesture));
        }, 50);
      });
      setTimeout(() => { this.createGeolocation(receiver, user_gesture)}, 50);
      break;

     case PermissionStatus.GRANTED:
      this.geolocationReceiver_.$.bindHandle(receiver.handle);
      break;

     default:
      receiver.handle.close();
    }
    return Promise.resolve(this.permissionStatus_);
  }

  /**
   * Sets whether the next geolocation permission request should be allowed.
   */
  setGeolocationPermission(allowed) {
    this.permissionStatus_ = allowed ? PermissionStatus.GRANTED
                                     : PermissionStatus.DENIED;
  }

  setSystemGeolocationPermission(allowed) {
    this.systemPermissionStatus_ = allowed ? PermissionStatus.GRANTED
                                           : PermissionStatus.DENIED;
  }
}
