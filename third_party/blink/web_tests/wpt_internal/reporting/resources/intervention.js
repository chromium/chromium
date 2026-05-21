function causeIntervention() {
  // Calling navigator.vibrate() in a frame that has never had user activation
  // generates an intervention report.
  navigator.vibrate(100);
}
