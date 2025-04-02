import { Decimal } from "icu4x"
import { DecimalFormatter } from "icu4x"
import { Locale } from "icu4x"
export function format(decimalFormatterLocaleName, decimalFormatterGroupingStrategy, valueF, valueMagnitude) {
    
    let decimalFormatterLocale = Locale.fromString(decimalFormatterLocaleName);
    
    let decimalFormatter = DecimalFormatter.createWithGroupingStrategy(decimalFormatterLocale,decimalFormatterGroupingStrategy);
    
    let value = Decimal.fromNumberWithLowerMagnitude(valueF,valueMagnitude);
    
    let out = decimalFormatter.format(value);
    

    return out;
}
