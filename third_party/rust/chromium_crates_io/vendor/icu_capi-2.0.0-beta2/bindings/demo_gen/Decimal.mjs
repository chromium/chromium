import { Decimal } from "icu4x"
export function toString(decimalF, decimalMagnitude) {
    
    let decimal = Decimal.fromNumberWithLowerMagnitude(decimalF,decimalMagnitude);
    
    let out = decimal.toString();
    

    return out;
}
