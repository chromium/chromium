import { Locale } from "icu4x"
export function basename(localeName) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.basename;
    

    return out;
}
export function getUnicodeExtension(localeName, s) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.getUnicodeExtension(s);
    

    return out;
}
export function language(localeName) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.language;
    

    return out;
}
export function region(localeName) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.region;
    

    return out;
}
export function script(localeName) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.script;
    

    return out;
}
export function normalize(s) {
    
    let out = Locale.normalize(s);
    

    return out;
}
export function toString(localeName) {
    
    let locale = Locale.fromString(localeName);
    
    let out = locale.toString();
    

    return out;
}
