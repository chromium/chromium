import { ComposingNormalizer } from "icu4x"
export function normalize(s) {
    
    let composingNormalizer = ComposingNormalizer.createNfc();
    
    let out = composingNormalizer.normalize(s);
    

    return out;
}
