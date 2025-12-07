def gen_range_tables(out, name, s_suffix, l_suffix, ranges):
    numshort = 0
    numlong = 0
    sptr = "NULL"
    lptr = "NULL"

    for range in ranges:
        (low, high) = range
        if high < 0x10000:
            if numshort == 0:
                sptr = name + s_suffix
                pline = "static const xmlChSRange %s[] = {" % sptr
            else:
                pline += ","
            numshort += 1
        else:
            if numlong == 0:
                if numshort > 0:
                    out.write(pline + "};\n")
                lptr = name + l_suffix
                pline = "static const xmlChLRange %s[] = {" % lptr
            else:
                pline += ","
            numlong += 1
        if len(pline) > 60:
            out.write(pline + "\n")
            pline = "    "
        elif pline[-1:] == ",":
            pline += " "
        pline += "{%s, %s}" % (hex(low), hex(high))

    out.write(pline + "};\n")

    return "{%s,%s,%s,%s}" % (numshort, numlong, sptr, lptr)
