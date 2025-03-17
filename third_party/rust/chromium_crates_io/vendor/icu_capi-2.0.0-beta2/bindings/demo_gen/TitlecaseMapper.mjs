import { Locale } from "icu4x"
import { TitlecaseMapper } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function titlecaseSegment(s, localeName, optionsLeadingAdjustment, optionsTrailingCase) {
    
    let titlecaseMapper = new TitlecaseMapper();
    
    let locale = Locale.fromString(localeName);
    
    let options = TitlecaseOptions.fromFields({
        leadingAdjustment: optionsLeadingAdjustment,
        trailingCase: optionsTrailingCase
    });
    
    let out = titlecaseMapper.titlecaseSegment(s,locale,options);
    

    return out;
}
