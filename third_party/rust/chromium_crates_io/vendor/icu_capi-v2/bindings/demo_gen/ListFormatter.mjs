import { ListFormatter } from "icu4x"
import { Locale } from "icu4x"
export function format(listFormatterLocaleName, listFormatterLength, list) {
    
    let listFormatterLocale = Locale.fromString(listFormatterLocaleName);
    
    let listFormatter = ListFormatter.createAndWithLength(listFormatterLocale,listFormatterLength);
    
    let out = listFormatter.format(list);
    

    return out;
}
