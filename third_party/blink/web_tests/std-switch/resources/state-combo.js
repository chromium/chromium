function generateSwitchElements(callback) {
  const MASK_ON = 1;
  const MASK_FOCUS = 1 << 1;
  const MASK_HOVER = 1 << 2;
  const MASK_ACTIVE = 1 << 3;
  const MASK_DISABLED = 1 << 4;
  const MASK_END = 1 << 5;
  for (let mask = 0; mask < MASK_END; ++mask) {
    let indicator = '';
    let switchElement = document.createElement('std-switch');
    if (mask & MASK_ON) {
      switchElement.on = true;
      indicator += 'o';
    } else {
      indicator += '-';
    }
    if (mask & MASK_FOCUS) {
      internals.setPseudoClassState(switchElement, ':focus', true);
      indicator += 'f';
    } else {
    indicator += '-';
    }
    if (mask & MASK_HOVER) {
      internals.setPseudoClassState(switchElement, ':hover', true);
      indicator += 'h';
    } else {
      indicator += '-';
    }
    if (mask & MASK_ACTIVE) {
      internals.setPseudoClassState(switchElement, ':active', true);
      indicator += 'a';
    } else {
      indicator += '-';
    }
    if (mask & MASK_DISABLED) {
      switchElement.disabled = true;
      indicator += 'd';
    } else {
      indicator += '-';
    }
    // Skip some impossible combinations
    if (mask & MASK_DISABLED && (mask & MASK_FOCUS || mask & MASK_ACTIVE)) {
      continue;
    }
    callback(indicator, switchElement);
  }
}
