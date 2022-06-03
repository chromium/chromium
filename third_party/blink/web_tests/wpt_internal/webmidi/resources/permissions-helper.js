export function setMidiPermission(options, state) {
  const sysex = options.sysex ?? false;
  return internals.setPermission(
      {name: 'midi', sysex: sysex}, state, location.origin, location.origin);
}
