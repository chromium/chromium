import {setMockSensorDataForType, waitForEvent} from './sensor-helpers.js';

export function generateMotionData(accelerationX, accelerationY, accelerationZ,
                                   accelerationIncludingGravityX,
                                   accelerationIncludingGravityY,
                                   accelerationIncludingGravityZ,
                                   rotationRateAlpha, rotationRateBeta, rotationRateGamma,
                                   interval = 16) {
  var motionData = {accelerationX: accelerationX,
                    accelerationY: accelerationY,
                    accelerationZ: accelerationZ,
                    accelerationIncludingGravityX: accelerationIncludingGravityX,
                    accelerationIncludingGravityY: accelerationIncludingGravityY,
                    accelerationIncludingGravityZ: accelerationIncludingGravityZ,
                    rotationRateAlpha: rotationRateAlpha,
                    rotationRateBeta: rotationRateBeta,
                    rotationRateGamma: rotationRateGamma,
                    interval: interval};
  return motionData;
}

export function generateOrientationData(alpha, beta, gamma, absolute) {
  var orientationData = {alpha: alpha,
                         beta: beta,
                         gamma: gamma,
                         absolute: absolute};
  return orientationData;
}

// Device[Orientation|Motion]EventPump treat NaN as a missing value.
let nullToNan = x => (x === null ? NaN : x);

export function setMockMotionData(sensorProvider, motionData) {
  const degToRad = Math.PI / 180;
  return Promise.all([
      setMockSensorDataForType(sensorProvider, "Accelerometer", [
          nullToNan(motionData.accelerationIncludingGravityX),
          nullToNan(motionData.accelerationIncludingGravityY),
          nullToNan(motionData.accelerationIncludingGravityZ),
      ]),
      setMockSensorDataForType(sensorProvider, "LinearAccelerationSensor", [
          nullToNan(motionData.accelerationX),
          nullToNan(motionData.accelerationY),
          nullToNan(motionData.accelerationZ),
      ]),
      setMockSensorDataForType(sensorProvider, "Gyroscope", [
          nullToNan(motionData.rotationRateAlpha) * degToRad,
          nullToNan(motionData.rotationRateBeta) * degToRad,
          nullToNan(motionData.rotationRateGamma) * degToRad,
      ]),
  ]);
}

export function setMockOrientationData(sensorProvider, orientationData) {
  let sensorType = orientationData.absolute
      ? "AbsoluteOrientationEulerAngles" : "RelativeOrientationEulerAngles";
  return setMockSensorDataForType(sensorProvider, sensorType, [
      nullToNan(orientationData.beta),
      nullToNan(orientationData.gamma),
      nullToNan(orientationData.alpha),
  ]);
}

export function waitForOrientation(expectedOrientationData,
                                   targetWindow = window) {
  return waitForEvent(
      new DeviceOrientationEvent('deviceorientation', {
        alpha: expectedOrientationData.alpha,
        beta: expectedOrientationData.beta,
        gamma: expectedOrientationData.gamma,
        absolute: expectedOrientationData.absolute,
      }),
      targetWindow);
}

export function waitForMotion(expectedMotionData, targetWindow = window) {
  return waitForEvent(
      new DeviceMotionEvent('devicemotion', {
        acceleration: {
          x: expectedMotionData.accelerationX,
          y: expectedMotionData.accelerationY,
          z: expectedMotionData.accelerationZ,
        },
        accelerationIncludingGravity: {
          x: expectedMotionData.accelerationIncludingGravityX,
          y: expectedMotionData.accelerationIncludingGravityY,
          z: expectedMotionData.accelerationIncludingGravityZ,
        },
        rotationRate: {
          alpha: expectedMotionData.rotationRateAlpha,
          beta: expectedMotionData.rotationRateBeta,
          gamma: expectedMotionData.rotationRateGamma,
        },
        interval: expectedMotionData.interval,
      }),
      targetWindow);
}

export async function setMainWindowHidden(hidden) {
  return new Promise(resolve => {
    document.addEventListener('visibilitychange', resolve, {once: true});
    testRunner.setMainWindowHidden(hidden);
  });
}
