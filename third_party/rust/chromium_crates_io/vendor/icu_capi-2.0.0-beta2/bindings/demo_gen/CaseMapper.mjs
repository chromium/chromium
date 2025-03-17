import { CaseMapper } from "icu4x"
import { Locale } from "icu4x"
import { TitlecaseOptions } from "icu4x"
export function lowercase(s, localeName) {
    
    let caseMapper = new CaseMapper();
    
    let locale = Locale.fromString(localeName);
    
    let out = caseMapper.lowercase(s,locale);
    

    return out;
}
export function uppercase(s, localeName) {
    
    let caseMapper = new CaseMapper();
    
    let locale = Locale.fromString(localeName);
    
    let out = caseMapper.uppercase(s,locale);
    

    return out;
}
export function titlecaseSegmentWithOnlyCaseData(s, localeName, optionsLeadingAdjustment, optionsTrailingCase) {
    
    let caseMapper = new CaseMapper();
    
    let locale = Locale.fromString(localeName);
    
    let options = TitlecaseOptions.fromFields({
        leadingAdjustment: optionsLeadingAdjustment,
        trailingCase: optionsTrailingCase
    });
    
    let out = caseMapper.titlecaseSegmentWithOnlyCaseData(s,locale,options);
    

    return out;
}
export function fold(s) {
    
    let caseMapper = new CaseMapper();
    
    let out = caseMapper.fold(s);
    

    return out;
}
export function foldTurkic(s) {
    
    let caseMapper = new CaseMapper();
    
    let out = caseMapper.foldTurkic(s);
    

    return out;
}
