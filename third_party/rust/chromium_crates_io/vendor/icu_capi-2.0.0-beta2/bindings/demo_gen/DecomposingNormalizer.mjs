import { DecomposingNormalizer } from "icu4x"
export function normalize(s) {
    
    let decomposingNormalizer = DecomposingNormalizer.createNfd();
    
    let out = decomposingNormalizer.normalize(s);
    

    return out;
}
