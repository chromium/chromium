use std::borrow::Cow;

#[derive(Eq, PartialEq, PartialOrd, Ord, Clone, Copy)]
struct UnitDef {
	singular: &'static str,
	plural: &'static str,
	definition: &'static str,
}

// singular, plural (or empty), definition, description
type UnitTuple = (&'static str, &'static str, &'static str, &'static str);

const BASE_UNITS: &[UnitTuple] = &[
	("second", "seconds", "l@!", ""),
	("meter", "meters", "l@!", ""),
	("kilogram", "kilograms", "l@!", ""),
	("kelvin", "", "l@!", ""),
	("ampere", "amperes", "l@!", ""),
	("mole", "moles", "l@!", ""),
	("candela", "candelas", "l@!", ""),
	("neper", "nepers", "l@!", ""),
];

const BASE_UNIT_ABBREVIATIONS: &[UnitTuple] = &[
	("s", "", "s@second", ""),
	("metre", "metres", "l@meter", ""),
	("m", "", "s@meter", ""),
	("gram", "grams", "l@1/1000 kilogram", ""),
	("jin", "jin", "l@1/2 kilogram", ""),
	("g", "", "s@gram", ""),
	("K", "", "s@kelvin", ""),
	("\u{b0}K", "", "=K", ""),
	("amp", "amps", "l@ampere", ""),
	("A", "", "s@ampere", ""),
	("mol", "", "s@mole", ""),
	("cd", "", "s@candela", ""),
	("Np", "", "s@neper", ""),
];

// some temperature scales have special support for conversions
const TEMPERATURE_SCALES: &[UnitTuple] = &[
	("celsius", "", "l@!", ""),
	("\u{b0}C", "", "celsius", ""), // degree symbol
	("oC", "", "=\u{b0}C", ""),
	("rankine", "", "l@5/9 K", ""),
	("\u{b0}R", "", "rankine", ""),
	("fahrenheit", "", "l@!", ""),
	("\u{b0}F", "", "fahrenheit", ""),
	("oF", "", "=\u{b0}F", ""),
];

const BITS_AND_BYTES: &[UnitTuple] = &[
	("bit", "bits", "l@!", ""),
	("bps", "", "s@bits/second", ""),
	("byte", "bytes", "l@8 bits", ""),
	("b", "", "s@bit", ""),
	("B", "", "s@byte", ""),
	("octet", "octets", "l@8 bits", ""),
	("nibble", "nibbles", "l@4 bits", ""),
];

const STANDARD_PREFIXES: &[UnitTuple] = &[
	("quecca", "", "lp@1e30", ""),
	("ronna", "", "lp@1e27", ""),
	("yotta", "", "lp@1e24", ""),
	("zetta", "", "lp@1e21", ""),
	("exa", "", "lp@1e18", ""),
	("peta", "", "lp@1e15", ""),
	("tera", "", "lp@1e12", ""),
	("giga", "", "lp@1e9", ""),
	("mega", "", "lp@1e6", ""),
	("myria", "", "lp@1e4", ""),
	("kilo", "", "lp@1e3", ""),
	("hecto", "", "lp@1e2", ""),
	("deca", "", "lp@1e1", ""),
	("deka", "", "lp@deca", ""),
	("deci", "", "lp@1e-1", ""),
	("centi", "", "lp@1e-2", ""),
	("milli", "", "lp@1e-3", ""),
	("micro", "", "lp@1e-6", ""),
	("nano", "", "lp@1e-9", ""),
	("pico", "", "lp@1e-12", ""),
	("femto", "", "lp@1e-15", ""),
	("atto", "", "lp@1e-18", ""),
	("zepto", "", "lp@1e-21", ""),
	("yocto", "", "lp@1e-24", ""),
	("ronto", "", "lp@1e-27", ""),
	("quecto", "", "lp@1e-30", ""),
	("k", "", "=1000", ""),
	("M", "", "=1,000,000", ""),
	("G", "", "=1,000,000,000", ""),
	("T", "", "=1,000,000,000,000", ""),
];

const NON_STANDARD_PREFIXES: &[UnitTuple] = &[
	("quarter", "", "lp@1/4", ""),
	("semi", "", "lp@0.5", ""),
	("demi", "", "lp@0.5", ""),
	("hemi", "", "lp@0.5", ""),
	("half", "", "lp@0.5", ""),
	("double", "", "lp@2", ""),
	("triple", "", "lp@3", ""),
	("treble", "", "lp@3", ""),
];

const BINARY_PREFIXES: &[UnitTuple] = &[
	("kibi", "", "lp@2^10", ""),
	("mebi", "", "lp@2^20", ""),
	("gibi", "", "lp@2^30", ""),
	("tebi", "", "lp@2^40", ""),
	("pebi", "", "lp@2^50", ""),
	("exbi", "", "lp@2^60", ""),
	("zebi", "", "lp@2^70", ""),
	("yobi", "", "lp@2^80", ""),
	("Ki", "", "=2^10", ""),
	("Mi", "", "=2^20", ""),
	("Gi", "", "=2^30", ""),
	("Ti", "", "=2^40", ""),
];

const NUMBER_WORDS: &[UnitTuple] = &[
	("tithe", "", "=1/10", ""),
	("one", "", "=1", ""),
	("two", "", "=2", ""),
	("couple", "", "=2", ""),
	("three", "", "=3", ""),
	("four", "", "=4", ""),
	("quadruple", "", "=4", ""),
	("five", "", "=5", ""),
	("quintuple", "", "=5", ""),
	("six", "", "=6", ""),
	("seven", "", "=7", ""),
	("eight", "", "=8", ""),
	("nine", "", "=9", ""),
	("ten", "", "=10", ""),
	("eleven", "", "=11", ""),
	("twelve", "", "=12", ""),
	("dozen", "", "=12", ""),
	("thirteen", "", "=13", ""),
	("bakersdozen", "", "=13", ""),
	("fourteen", "", "=14", ""),
	("fifteen", "", "=15", ""),
	("sixteen", "", "=16", ""),
	("seventeen", "", "=17", ""),
	("eighteen", "", "=18", ""),
	("nineteen", "", "=19", ""),
	("twenty", "", "=20", ""),
	("score", "", "=20", ""),
	("thirty", "", "=30", ""),
	("forty", "", "=40", ""),
	("fifty", "", "=50", ""),
	("sixty", "", "=60", ""),
	("seventy", "", "=70", ""),
	("eighty", "", "=80", ""),
	("ninety", "", "=90", ""),
	("hundred", "", "=100", ""),
	("gross", "", "=144", ""),
	("greatgross", "", "=12 gross", ""),
	("thousand", "", "=1000", ""),
	("million", "", "=1e6", ""),
	("billion", "", "=1e9", ""),
	("trillion", "", "=1e12", ""),
	("quadrillion", "", "=1e15", ""),
	("quintillion", "", "=1e18", ""),
	("sextillion", "", "=1e21", ""),
	("septillion", "", "=1e24", ""),
	("octillion", "", "=1e27", ""),
	("nonillion", "", "=1e30", ""),
	("decillion", "", "=1e33", ""),
	("undecillion", "", "=1e36", ""),
	("duodecillion", "", "=1e39", ""),
	("tredecillion", "", "=1e42", ""),
	("quattuordecillion", "", "=1e45", ""),
	("quindecillion", "", "=1e48", ""),
	("sexdecillion", "", "=1e51", ""),
	("septendecillion", "", "=1e54", ""),
	("octodecillion", "", "=1e57", ""),
	("novemdecillion", "", "=1e60", ""),
	("vigintillion", "", "=1e63", ""),
	("unvigintillion", "", "=1e66", ""),
	("duovigintillion", "", "=1e69", ""),
	("trevigintillion", "", "=1e72", ""),
	("quattuorvigintillion", "", "=1e75", ""),
	("quinvigintillion", "", "=1e78", ""),
	("sexvigintillion", "", "=1e81", ""),
	("septenvigintillion", "", "=1e84", ""),
	("octovigintillion", "", "=1e87", ""),
	("novemvigintillion", "", "=1e90", ""),
	("trigintillion", "", "=1e93", ""),
	("untrigintillion", "", "=1e96", ""),
	("duotrigintillion", "", "=1e99", ""),
	("googol", "", "=1e100", ""),
	("tretrigintillion", "", "=1e102", ""),
	("quattuortrigintillion", "", "=1e105", ""),
	("quintrigintillion", "", "=1e108", ""),
	("sextrigintillion", "", "=1e111", ""),
	("septentrigintillion", "", "=1e114", ""),
	("octotrigintillion", "", "=1e117", ""),
	("novemtrigintillion", "", "=1e120", ""),
	("centillion", "", "=1e303", ""),
];

const CONSTANTS: &[UnitTuple] = &[
	(
		"c",
		"",
		"=299792458 m/s",
		"speed of light in vacuum (exact)",
	),
	(
		"planck",
		"",
		"=6.62607015e-34 J s",
		"Planck constant (exact)",
	),
	(
		"boltzmann",
		"",
		"=1.380649e-23 J/K",
		"Boltzmann constant (exact)",
	),
	("electron_charge", "", "=1.602176634e-19 coulomb", ""),
	("electroncharge", "", "=electron_charge", ""),
	("electronmass", "", "=9.1093837015e-31 kg", ""),
	("electron_mass", "", "=electronmass", ""),
	("protonmass", "", "=1.67262192369e-27 kg", ""),
	("proton_mass", "", "=protonmass", ""),
	("neutronmass", "", "=1.67492749804e-27 kg", ""),
	("neutron_mass", "", "=neutronmass", ""),
	(
		"avogadro",
		"",
		"=6.02214076e23 / mol",
		"size of a mole (exact)",
	),
	("N_A", "", "=avogadro", ""),
	(
		"gravitational_constant",
		"",
		"=6.67430e-11 N m^2 / kg^2",
		"gravitational constant",
	),
	("gravity", "", "=9.80665 m/s^2", ""),
	("force", "", "gravity", ""), // used to convert some units
];

const ANGLES: &[UnitTuple] = &[
	("radian", "radians", "l@1", ""),
	("rad", "", "radian", ""),
	("circle", "circles", "l@2 pi radian", ""),
	("degree", "degrees", "l@1/360 circle", ""),
	("deg", "degs", "l@degree", ""),
	("\u{b0}", "", "degree", ""), // degree symbol
	("arcdeg", "arcdegs", "degree", ""),
	("arcmin", "arcmins", "l@1/60 degree", ""),
	("arcminute", "arcminutes", "l@arcmin", ""),
	("arcsec", "arcsecs", "l@1/60 arcmin", ""),
	("arcsecond", "arcseconds", "l@arcsec", ""),
	("rightangle", "rightangles", "l@90 degrees", ""),
	("quadrant", "quadrants", "l@1/4 circle", ""),
	("quintant", "quintants", "l@1/5 circle", ""),
	("sextant", "sextants", "l@1/6 circle", ""),
	(
		"zodiac_sign",
		"zodiac_signs",
		"l@1/12 circle",
		"Angular extent of one sign of the zodiac",
	),
	("turn", "turns", "l@circle", ""),
	("revolution", "revolutions", "l@circle", ""),
	("rev", "revs", "l@circle", ""),
	("gradian", "gradians", "l@1/100 rightangle", ""),
	("gon", "gons", "l@gradian", ""),
	("grad", "", "l@gradian", ""),
	("mas", "", "milliarcsec", ""),
];

const SOLID_ANGLES: &[UnitTuple] = &[
	("steradian", "steradians", "l@1", ""),
	("sr", "sr", "s@steradian", ""),
	("sphere", "spheres", "4 pi steradians", ""),
	(
		"squaredegree",
		"squaredegrees",
		"(1/180)^2 pi^2 steradians",
		"",
	),
	("squareminute", "squareminutes", "(1/60)^2 squaredegree", ""),
	("squaresecond", "squareseconds", "(1/60)^2 squareminute", ""),
	("squarearcmin", "squarearcmins", "squareminute", ""),
	("squarearcsec", "squarearcsecs", "squaresecond", ""),
	(
		"sphericalrightangle",
		"sphericalrightangles",
		"0.5 pi steradians",
		"",
	),
	("octant", "octants", "0.5 pi steradians", ""),
];

const COMMON_SI_DERIVED_UNITS: &[UnitTuple] = &[
	("newton", "newtons", "l@kg m / s^2", "force"),
	("N", "", "s@newton", ""),
	("pascal", "pascals", "l@N/m^2", "pressure or stress"),
	("Pa", "", "s@pascal", ""),
	("joule", "joules", "l@N m", "energy"),
	("J", "", "s@joule", ""),
	("watt", "watts", "l@J/s", "power"),
	("W", "", "s@watt", ""),
	(
		"horsepower",
		"horsepowers",
		"l@745.69987158227022 watts",
		"",
	),
	("hp", "", "s@horsepower", ""),
	("coulomb", "", "l@A s", "charge"),
	("C", "", "s@coulomb", ""),
	("volt", "volts", "l@W/A", "potential difference"),
	("V", "", "s@volt", ""),
	("Ah", "", "s@ampere hour", ""),
	("ohm", "ohms", "l@V/A", "electrical resistance"),
	("siemens", "", "l@A/V", "electrical conductance"),
	("S", "", "s@siemens", ""),
	("farad", "", "l@coulomb/V", "capacitance"),
	("F", "", "s@farad", ""),
	("weber", "", "l@V s", "magnetic flux"),
	("Wb", "", "s@weber", ""),
	("henry", "", "l@V s / A", "inductance"),
	("H", "", "s@henry", ""),
	("tesla", "", "l@Wb/m^2", "magnetic flux density"),
	("T", "", "s@tesla", ""),
	("hertz", "", "l@/s", "frequency"),
	("Hz", "", "s@hertz", ""),
	("nit", "nits", "l@candela / meter^2", "luminance"),
	("nt", "", "nit", ""),
	("lumen", "lumens", "l@cd sr", "luminous flux"),
	("lm", "", "s@lumen", ""),
	("lux", "", "l@lm/m^2", "illuminance"),
	("lx", "", "lux", "illuminance"),
	("phot", "phots", "l@1e4 lx", ""),
	("ph", "", "s@phot", ""),
	("becquerel", "becquerels", "l@/s", "radioactivity"),
	("Bq", "", "s@becquerel", ""),
	("curie", "curies", "l@3.7e10 Bq", ""),
	("Ci", "", "s@curie", ""),
	("rutherford", "rutherfords", "l@1e6 Bq", ""),
	("Rd", "", "s@rutherford", ""),
	(
		"gray",
		"grays",
		"l@J/kg",
		"absorbed dose of ionising radiation",
	),
	("Gy", "", "s@gray", ""),
	("rad_radiation", "", "l@1/100 Gy", ""),
	(
		"sievert",
		"sieverts",
		"l@J / kg",
		"equivalent dose of ionising radiation",
	),
	("Sv", "", "s@sievert", ""),
	("rem", "", "l@1/100 Sv", ""),
	("roentgen", "roentgens", "l@0.000258 coulomb/kg", ""),
	("R", "", "s@roentgen", ""),
];

const TIME_UNITS: &[UnitTuple] = &[
	("sec", "secs", "s@second", ""),
	("minute", "minutes", "l@60 seconds", ""),
	("min", "mins", "s@minute", ""),
	("hour", "hours", "l@60 minutes", ""),
	("hr", "hrs", "s@hour", ""),
	("h", "h", "s@hour", ""),
	("day", "days", "l@24 hours", ""),
	("d", "", "s@day", ""),
	("da", "", "s@day", ""),
	("week", "weeks", "l@7 days", ""),
	("wk", "", "s@week", ""),
	("fortnight", "fortnights", "l@14 day", ""),
	(
		"sidereal_year",
		"sidereal_years",
		"365.256363004 days",
		concat!(
			"the time taken for the Earth to complete one revolution of its orbit, ",
			"as measured against a fixed frame of reference (such as the fixed stars, ",
			"Latin sidera, singular sidus)"
		),
	),
	(
		"tropical_year",
		"tropical_years",
		"365.242198781 days",
		"the period of time for the mean ecliptic longitude of the Sun to increase by 360 degrees",
	),
	(
		"anomalistic_year",
		"anomalistic_years",
		"365.259636 days",
		"the time taken for the Earth to complete one revolution with respect to its apsides",
	),
	("year", "years", "l@tropical_year", ""),
	("yr", "", "year", ""),
	("month", "months", "l@1/12 year", ""),
	("mo", "", "month", ""),
	("decade", "decades", "10 years", ""),
	("century", "centuries", "100 years", ""),
	("millennium", "millennia", "1000 years", ""),
	("solar_year", "solar_years", "year", ""),
	("calendar_year", "calendar_years", "365 days", ""),
	("common_year", "common_years", "365 days", ""),
	("leap_year", "leap_years", "366 days", ""),
	("julian_year", "julian_years", "365.25 days", ""),
	("gregorian_year", "gregorian_years", "365.2425 days", ""),
	// french revolutionary time
	("decimal_hour", "decimal_hours", "l@1/10 day", ""),
	(
		"decimal_minute",
		"decimal_minutes",
		"l@1/100 decimal_hour",
		"",
	),
	(
		"decimal_second",
		"decimal_seconds",
		"l@1/100 decimal_minute",
		"",
	),
	("beat", "beats", "l@decimal_minute", "Swatch Internet Time"),
	("scaramucci", "scaramuccis", "11 days", ""),
	("mooch", "mooches", "scaramucci", ""),
	(
		"sol",
		"sols",
		"24 hours 39 minutes 35 seconds",
		"martian day",
	),
];

const RATIOS: &[UnitTuple] = &[
	("\u{2030}", "", "=0.001", ""), // per mille
	("percent", "", "=0.01", ""),
	("%", "", "=percent", ""),
	("bel", "bels", "0.5 * ln(10) neper", ""),
	("decibel", "decibels", "1/10 bel", ""),
	("dB", "", "decibel", ""),
	("mill", "mills", "0.001", ""),
	("ppm", "", "1e-6", ""),
	("parts_per_million", "", "ppm", ""),
	("ppb", "", "1e-9", ""),
	("parts_per_billion", "", "ppb", ""),
	("ppt", "", "1e-12", ""),
	("parts_per_trillion", "", "ppt", ""),
	("karat", "", "1/24", "measure of gold purity"),
	("basispoint", "", "0.01 %", ""),
];

const COMMON_PHYSICAL_UNITS: &[UnitTuple] = &[
	("electron_volt", "electron_volts", "l@electron_charge V", ""),
	("eV", "", "s@electron_volt", ""),
	("light_year", "light_years", "c julian_year", ""),
	("ly", "", "lightyear", ""),
	("light_second", "light_seconds", "c second", ""),
	("light_minute", "light_minutes", "c minute", ""),
	("light_hour", "light_hours", "c hour", ""),
	("light_day", "light_days", "c day", ""),
	("light_speed", "", "c", ""),
	("lightspeed", "", "c", ""),
	("parsec", "parsecs", "l@au / tan(arcsec)", ""),
	("pc", "", "s@parsec", ""),
	(
		"astronomical_unit",
		"astronomical_units",
		"149597870700 m",
		"",
	),
	("au", "", "astronomical_unit", ""),
	("AU", "", "astronomical_unit", ""),
	("barn", "", "l@1e-28 m^2", ""),
	("shed", "", "l@1e-24 barn", ""),
	("cc", "", "cm^3", ""),
	("are", "ares", "l@100 meter^2", ""),
	("liter", "liters", "l@1000 cc", ""),
	("litre", "litres", "liter", ""),
	("l", "", "s@liter", ""),
	("L", "", "s@liter", ""),
	("micron", "microns", "l@micrometer", ""),
	("bicron", "bicrons", "l@picometer", ""),
	("gsm", "", "grams / meter^2", ""),
	("hectare", "hectares", "hectoare", ""),
	("ha", "", "s@hectare", ""),
	("decare", "decares", "l@decaare", ""),
	("da", "", "s@decare", ""),
	("calorie", "calories", "l@4.184 J", ""),
	("cal", "", "s@calorie", ""),
	(
		"british_thermal_unit",
		"british_thermal_units",
		"1055.05585 J",
		"",
	),
	("btu", "", "british_thermal_unit", ""),
	("Wh", "", "s@W hour", ""),
	("atmosphere", "atmospheres", "l@101325 Pa", ""),
	("atm", "", "s@atmosphere", ""),
	("mmHg", "", "l@1/760 atm", "millimeter of mercury"),
	("inHg", "", "l@25.4 mmHg", "inch of mercury"),
	("bar", "", "l@1e5 Pa", "about 1 atmosphere"),
	("diopter", "", "l@/m", "reciprocal of focal length"),
	("sqm", "", "=m^2", ""),
	("sqmm", "", "=mm^2", ""),
	("gongjin", "", "l@1 kilogram", ""),
	(
		"toe",
		"toe",
		"s@41.868 gigajoules",
		"tonne of oil equivalent",
	),
	("kgoe", "kgoe", "1/1000 toe", "kilogram of oil equivalent"),
	("ton_of_tnt", "", "l@4.184 gigajoules", "TNT equivalent"),
	// TODO remove these compatibility units
	("lightyear", "lightyears", "light_year", ""),
	("light", "", "c", ""),
];

const CGS_UNITS: &[UnitTuple] = &[
	("gal", "gals", "cm/s^2", "acceleration"),
	("dyne", "dynes", "g*gal", "force"),
	("erg", "ergs", "g*cm^2/s^2", "work, energy"),
	("barye", "baryes", "g/(cm*s^2)", "pressure"),
	("poise", "poises", "g/(cm*s)", ""),
	("stokes", "", "cm^2/s", ""),
	("kayser", "kaysers", "cm^-1", ""),
	("biot", "biots", "10 amperes", ""),
	("emu", "emus", "0.001 A m^2", ""),
	("franklin", "franklins", "dyn^1/2*cm", ""),
	("gauss", "", "10^-4 tesla", ""),
	("maxwell", "maxwells", "10^-8 weber", ""),
	("phot", "phots", "10000 lux", ""),
	("stilb", "stilbs", "10000 candela/m^2", ""),
	// abbrevations
	("gallileo", "gallileos", "gal", ""),
	("dyn", "dyns", "dyne", ""),
	("Ba", "", "barye", ""),
	("P", "", "poise", ""),
	("St", "", "stokes", ""),
	("K", "", "kayser", ""),
	("Bi", "", "biot", ""),
	("Fr", "", "franklin", ""),
	("G", "", "gauss", ""),
	("Mx", "", "maxwell", ""),
	("ph", "", "phot", ""),
];

const IMPERIAL_UNITS: &[UnitTuple] = &[
	("inch", "inches", "2.54 cm", ""),
	("mil", "mils", "1/1000 inch", ""),
	("\u{2019}", "", "foot", ""), // unicode single quote
	("\u{201d}", "", "inch", ""), // unicode double quote
	("'", "", "foot", ""),
	("\"", "", "inch", ""),
	("foot", "feet", "l@12 inch", ""),
	("sqft", "", "=ft^2", ""),
	("yard", "yards", "l@3 ft", ""),
	("mile", "miles", "l@5280 ft", ""),
	("line", "lines", "1/12 inch", ""),
	("rod", "rods", "5.5 yard", ""),
	("pole", "poles", "rod", ""),
	("perch", "perches", "rod", ""),
	("firkin", "firkins", "90 lb", ""),
	("furlong", "furlongs", "40 rod", ""),
	("statute_mile", "statute_miles", "mile", ""),
	("league", "leagues", "3 mile", ""),
	("chain", "chains", "66 feet", ""),
	("link", "links", "1/100 chain", ""),
	("thou", "", "1/1000 inch", "thousandth of an inch"),
	("acre", "acres", "10 chain^2", ""),
	("section", "sections", "mile^2", ""),
	("township", "townships", "36 sections", ""),
	("homestead", "homesteads", "160 acres", ""),
	("point", "points", "l@1/72 inch", ""),
	("twip", "twips", "l@1/20 point", ""),
	("poppyseed", "poppyseeds", "l@line", ""),
	("pica", "picas", "l@12 points", ""),
	("barleycorn", "barleycorns", "l@4 poppyseed", ""),
	("finger", "fingers", "l@63 points", ""),
	("stick", "sticks", "l@2 inches", ""),
	("palm", "palms", "l@3 inches", ""),
	("digit", "digits", "l@1/4 palms", ""),
	("nail", "nails", "l@3 digits", ""),
	("span", "spans", "l@4 nails", ""),
	("hand", "hands", "l@2 sticks", ""),
	("shaftment", "shaftments", "l@2 palm", ""),
	("cubit", "cubits", "l@2 span", ""),
	("ell", "ells", "l@5 span", ""),
	("skein", "skeins", "l@96 ell", ""),
	("spindle", "spindles", "l@120 skein", ""),
	("link", "links", "l@1/25 rod", ""),
	("fathom", "fathoms", "l@2 yard", ""),
	("shackle", "shackles", "l@15 yard", ""),
	("pace", "paces", "l@5 shaftments", ""),
	("step", "steps", "l@2 paces", ""),
	("grade", "grades", "l@pace", ""),
	("rope", "ropes", "l@4 steps", ""),
	("ramsdens_chain", "", "l@5 rope", ""),
	("roman_mile", "roman_miles", "l@50 ramsdens_chain", ""),
	("gunters_chain", "gunters_chains", "l@4 rod", ""),
	("rack_unit", "rack_units", "l@1.75 inches", ""),
	("U", "", "rack_unit", ""),
];

const LIQUID_UNITS: &[UnitTuple] = &[
	("gallon", "gallons", "231 inch^3", ""),
	("gal", "", "gallon", ""),
	("quart", "quarts", "1/4 gallon", ""),
	("pint", "pints", "1/2 quart", ""),
	("cup", "cups", "1/2 pint", ""),
	("gill", "", "1/4 pint", ""),
	("fluid_ounce", "", "1/16 pint", ""),
	("tablespoon", "tablespoons", "1/2 floz", ""),
	("teaspoon", "teaspoons", "1/3 tablespoon", ""),
	("fluid_dram", "", "1/8 floz", ""),
	("qt", "", "quart", ""),
	("pt", "", "pint", ""),
	("floz", "", "fluid_ounce", ""),
	("tbsp", "", "tablespoon", ""),
	("tbs", "", "tablespoon", ""),
	("tsp", "", "teaspoon", ""),
];

const AVOIRDUPOIS_WEIGHT: &[UnitTuple] = &[
	("pound", "pounds", "0.45359237 kg", ""),
	("lb", "lbs", "pound", ""),
	("grain", "grains", "1/7000 pound", ""),
	("ounce", "ounces", "1/16 pound", ""),
	("oz", "", "ounce", ""),
	("dram", "drams", "1/16 ounce", ""),
	("dr", "", "dram", ""),
	("hundredweight", "hundredweights", "100 pounds", ""),
	("cwt", "", "hundredweight", ""),
	("short_ton", "short_tons", "2000 pounds", ""),
	("quarterweight", "quarterweights", "1/4 short_ton", ""),
	("stone", "stones", "14 pounds", ""),
	("st", "", "stone", ""),
];

const TROY_WEIGHT: &[UnitTuple] = &[
	("troy_pound", "troy_pounds", "5760 grains", ""),
	("troy_ounce", "troy_ounces", "1/12 troy_pound", ""),
	("ozt", "", "troy_ounce", ""),
	("pennyweight", "pennyweights", "1/20 troy_ounce", ""),
	("dwt", "", "pennyweight", ""),
];

const OTHER_WEIGHTS: &[UnitTuple] = &[
	("metric_grain", "metric_grains", "50 mg", ""),
	("carat", "carats", "0.2 grams", ""),
	("ct", "", "carat", ""),
	("jewellers_point", "jewellers_points", "1/100 carat", ""),
	("tonne", "tonnes", "l@1000 kg", ""),
	("t", "", "tonne", ""),
];

const IMPERIAL_ABBREVIATIONS: &[UnitTuple] = &[
	("yd", "", "yard", ""),
	("ch", "", "chain", ""),
	("ft", "", "foot", ""),
	("mph", "", "mile/hr", ""),
	("mpg", "", "mile/gal", ""),
	("kph", "", "km/hr", ""),
	("kmh", "", "km/hr", ""),
	("fpm", "", "ft/min", ""),
	("fps", "", "ft/s", ""),
	("rpm", "", "rev/min", ""),
	("rps", "", "rev/sec", ""),
	("mi", "", "mile", ""),
	("smi", "", "mile", ""),
	("nmi", "", "nautical_mile", ""),
	("mbh", "", "1e3 btu/hour", ""),
	("ipy", "", "inch/year", ""),
	("ccf", "", "100 ft^3", ""),
	("Mcf", "", "1000 ft^3", ""),
	("plf", "", "lb / foot", "pounds per linear foot"),
	("lbf", "", "lb force", ""),
	("psi", "", "pound force / inch^2", ""),
	("fur", "furs", "furlong", ""),
	("fir", "firs", "firkin", ""),
	("ftn", "ftns", "fortnight", ""),
];

const NAUTICAL_UNITS: &[UnitTuple] = &[
	("fathom", "fathoms", "6 ft", ""),
	("nautical_mile", "nautical_miles", "1852 m", ""),
	("cable", "cables", "1/10 nautical_mile", ""),
	("marine_league", "marine_leagues", "3 nautical_mile", ""),
	("knot", "knots", "nautical_mile / hr", ""),
	("kn", "", "=knots", ""),
	("click", "clicks", "km", ""),
	("NM", "", "nautical_mile", ""),
];

const CURRENCIES: &[UnitTuple] = &[
	("BASE_CURRENCY", "BASE_CURRENCY", "!", ""),
	("dollar", "dollars", "USD", ""),
	("cent", "cents", "0.01 USD", ""),
	("US$", "US$", "USD", ""),
	("$", "$", "USD", ""),
	("euro", "euros", "EUR", ""),
	("\u{20ac}", "\u{20ac}", "EUR", ""), // Euro symbol
	("\u{a3}", "\u{a3}", "GBP", ""),     // £
	("\u{a5}", "\u{a5}", "JPY", ""),     // ¥
	("AU$", "AU$", "AUD", ""),
	("HK$", "HK$", "HKD", ""),
	("NZ$", "NZ$", "NZD", ""),
	("zł", "zł", "PLN", ""), // the local abbreviation for PLN, see https://en.wikipedia.org/wiki/Polish_z%C5%82oty
	("zl", "zl", "PLN", ""),
];

const HISTORICAL_UNITS: &[UnitTuple] = &[
	("shaku", "shaku", "0.303 m", ""),
	("tsubo", "tsubo", "3.306 m^2", ""),
	("tatami", "tatami", "tsubo / 2", ""),
	("tatami_mat", "tatami_mats", "=tatami", ""),
];

// from https://en.wikipedia.org/wiki/ISO_4217
const CURRENCY_IDENTIFIERS: &[&str] = &[
	"AED", "AFN", "ALL", "AMD", "ANG", "AOA", "ARS", "AUD", "AWG", "AZN", "BAM", "BBD", "BDT",
	"BGN", "BHD", "BIF", "BMD", "BND", "BOB", "BOV", "BRL", "BSD", "BTN", "BWP", "BYN", "BZD",
	"CAD", "CDF", "CHE", "CHF", "CHW", "CLF", "CLP", "CNY", "COP", "COU", "CRC", "CUC", "CUP",
	"CVE", "CZK", "DJF", "DKK", "DOP", "DZD", "EGP", "ERN", "ETB", "EUR", "FJD", "FKP", "GBP",
	"GEL", "GHS", "GIP", "GMD", "GNF", "GTQ", "GYD", "HKD", "HNL", "HRK", "HTG", "HUF", "IDR",
	"ILS", "INR", "IQD", "IRR", "ISK", "JMD", "JOD", "JPY", "KES", "KGS", "KHR", "KMF", "KPW",
	"KRW", "KWD", "KYD", "KZT", "LAK", "LBP", "LKR", "LRD", "LSL", "LYD", "MAD", "MDL", "MGA",
	"MKD", "MMK", "MNT", "MOP", "MRU", "MUR", "MVR", "MWK", "MXN", "MXV", "MYR", "MZN", "NAD",
	"NGN", "NIO", "NOK", "NPR", "NZD", "OMR", "PAB", "PEN", "PGK", "PHP", "PKR", "PLN", "PYG",
	"QAR", "RON", "RSD", "RUB", "RWF", "SAR", "SBD", "SCR", "SDG", "SEK", "SGD", "SHP", "SLE",
	"SLL", "SOS", "SRD", "SSP", "STN", "SVC", "SYP", "SZL", "THB", "TJS", "TMT", "TND", "TOP",
	"TRY", "TTD", "TWD", "TZS", "UAH", "UGX", "USD", "USN", "UYI", "UYU", "UYW", "UZS", "VED",
	"VES", "VND", "VUV", "WST", "XAF", "XAG", "XAU", "XBA", "XBB", "XBC", "XBD", "XCD", "XDR",
	"XOF", "XPD", "XPF", "XPT", "XSU", "XTS", "XUA", "XXX", "YER", "ZAR", "ZMW", "ZWL",
];

pub(crate) const ALL_UNIT_DEFS: &[&[UnitTuple]] = &[
	BASE_UNITS,
	BASE_UNIT_ABBREVIATIONS,
	TEMPERATURE_SCALES,
	BITS_AND_BYTES,
	STANDARD_PREFIXES,
	NON_STANDARD_PREFIXES,
	BINARY_PREFIXES,
	NUMBER_WORDS,
	CONSTANTS,
	ANGLES,
	SOLID_ANGLES,
	COMMON_SI_DERIVED_UNITS,
	TIME_UNITS,
	RATIOS,
	COMMON_PHYSICAL_UNITS,
	IMPERIAL_UNITS,
	LIQUID_UNITS,
	AVOIRDUPOIS_WEIGHT,
	TROY_WEIGHT,
	OTHER_WEIGHTS,
	IMPERIAL_ABBREVIATIONS,
	NAUTICAL_UNITS,
	CURRENCIES,
	CGS_UNITS,
	HISTORICAL_UNITS,
];

const SHORT_PREFIXES: &[(&str, &str)] = &[
	("Ki", "sp@kibi"),
	("Mi", "sp@mebi"),
	("Gi", "sp@gibi"),
	("Ti", "sp@tebi"),
	("Pi", "sp@pebi"),
	("Ei", "sp@exbi"),
	("Zi", "sp@zebi"),
	("Yi", "sp@yobi"),
	("Y", "sp@yotta"),
	("Z", "sp@zetta"),
	("E", "sp@exa"),
	("P", "sp@peta"),
	("T", "sp@tera"),
	("G", "sp@giga"),
	("M", "sp@mega"),
	("k", "sp@kilo"),
	("h", "sp@hecto"),
	("da", "sp@deka"),
	("d", "sp@deci"),
	("c", "sp@centi"),
	("m", "sp@milli"),
	("u", "sp@micro"),       // alternative to µ
	("\u{b5}", "sp@micro"),  // U+00B5 (micro sign)
	("\u{3bc}", "sp@micro"), // U+03BC (lowercase µ)
	("n", "sp@nano"),
	("p", "sp@pico"),
	("f", "sp@femto"),
	("a", "sp@atto"),
	("z", "sp@zepto"),
	("y", "sp@yocto"),
];

#[allow(clippy::too_many_lines)]
pub(crate) fn query_unit(
	ident: &str,
	short_prefixes: bool,
	case_sensitive: bool,
) -> Option<(Cow<'static, str>, Cow<'static, str>, Cow<'static, str>)> {
	if short_prefixes {
		for (name, def) in SHORT_PREFIXES {
			if *name == ident || (!case_sensitive && name.eq_ignore_ascii_case(ident)) {
				return Some((Cow::Borrowed(name), Cow::Borrowed(name), Cow::Borrowed(def)));
			}
		}
	}
	if let Ok(idx) = CURRENCY_IDENTIFIERS.binary_search(
		&if case_sensitive {
			ident.to_string()
		} else {
			ident.to_uppercase()
		}
		.as_str(),
	) {
		let name = CURRENCY_IDENTIFIERS[idx];
		return Some((
			Cow::Borrowed(name),
			Cow::Borrowed(name),
			Cow::Borrowed("$CURRENCY"),
		));
	}
	let mut candidates = vec![];
	for group in ALL_UNIT_DEFS {
		for def in *group {
			let def = UnitDef {
				singular: def.0,
				plural: if def.1.is_empty() { def.0 } else { def.1 },
				definition: def.2,
			};
			if def.singular == ident || def.plural == ident {
				return Some((
					Cow::Borrowed(def.singular),
					Cow::Borrowed(def.plural),
					Cow::Borrowed(def.definition),
				));
			}
			if !case_sensitive
				&& (def.singular.eq_ignore_ascii_case(ident)
					|| def.plural.eq_ignore_ascii_case(ident))
			{
				candidates.push(Some((
					Cow::Borrowed(def.singular),
					Cow::Borrowed(def.plural),
					Cow::Borrowed(def.definition),
				)));
			}
		}
	}
	if candidates.len() == 1 {
		return candidates.into_iter().next().unwrap();
	}
	None
}

const DEFAULT_UNITS: &[(&str, &str)] = &[
	("hertz", "second^-1"),
	("newton", "kilogram^1 meter^1 second^-2"),
	("pascal", "kilogram^1 meter^-1 second^-2"),
	("joule", "kilogram^1 meter^2 second^-2"),
	("watt", "kilogram^1 meter^2 second^-3"),
	("ohm", "ampere^-2 kilogram^1 meter^2 second^-3"),
	("volt", "ampere^-1 kilogram^1 meter^2 second^-3"),
	("liter", "meter^3"),
];

pub(crate) fn lookup_default_unit(base_units: &str) -> Option<&str> {
	if let Some((unit_name, _)) = DEFAULT_UNITS.iter().find(|(_, base)| *base == base_units) {
		return Some(unit_name);
	}
	if let Some((singular, _, _, _)) = BASE_UNITS
		.iter()
		.find(|(singular, _, _, _)| format!("{singular}^1") == base_units)
	{
		return Some(singular);
	}
	None
}

/// used for implicit unit addition, e.g. 5'5 -> 5'5"
pub(crate) const IMPLICIT_UNIT_MAP: &[(&str, &str)] = &[("'", "\""), ("foot", "inches")];

#[cfg(test)]
mod tests {
	use super::*;

	fn test_str(s: &str, ctx: &mut crate::Context) {
		if s.is_empty() || s == "'" || s == "\"" {
			return;
		}
		//eprintln!("Testing '{s}'");
		crate::evaluate(s, ctx).unwrap();
	}

	fn test_group(group: &[UnitTuple]) {
		let mut ctx = crate::Context::new();
		ctx.set_exchange_rate_handler_v1(crate::test_utils::dummy_currency_handler);
		for (s, p, _, _) in group {
			test_str(s, &mut ctx);
			test_str(p, &mut ctx);
		}
	}

	#[test]
	fn test_all_units() {
		for &group in ALL_UNIT_DEFS {
			test_group(group);
		}
	}

	#[test]
	fn currencies_sorted() {
		let currencies = CURRENCY_IDENTIFIERS.to_vec();
		let mut sorted = currencies.clone();
		sorted.sort_unstable();
		assert_eq!(currencies, sorted, "currencies are not sorted");
	}

	#[test]
	fn lowercase_currency() {
		assert!(query_unit("usd", true, true).is_none());
		assert!(query_unit("usd", true, false).is_some());
		assert!(query_unit("USD", true, false).is_some());
	}
}
