/* Configure script for libxslt, specific for Windows with Scripting Host.
 * 
 * This script will configure the libxslt build process and create necessary files.
 * Run it with an 'help', or an invalid option and it will tell you what options
 * it accepts.
 *
 * March 2002, Igor Zlatkovic <igor@zlatkovic.com>
 */

/* The source directory, relative to the one where this file resides. */
var baseDir = "..";
var srcDirXslt = baseDir + "\\libxslt";
var srcDirExslt = baseDir + "\\libexslt";
var srcDirUtils = baseDir + "\\xsltproc";
/* The directory where we put the binaries after compilation. */
var binDir = "binaries";
/* Base name of what we are building. */
var baseNameXslt = "libxslt";
var baseNameExslt = "libexslt";
/* Configure file which contains the version and the output file where
   we can store our build configuration. */
var configFile = baseDir + "\\configure.ac";
var versionFile = ".\\config.msvc";
/* Input and output files regarding the lib(e)xml features. The second
   output file is there for the compatibility reasons, otherwise it
   is identical to the first. */
var optsFileInXslt = srcDirXslt + "\\xsltconfig.h.in";
var optsFileXslt = srcDirXslt + "\\xsltconfig.h";
var optsFileInExslt = srcDirExslt + "\\exsltconfig.h.in";
var optsFileExslt = srcDirExslt + "\\exsltconfig.h";
/* Version strings for the binary distribution. Will be filled later 
   in the code. */
var verMajorXslt;
var verMinorXslt;
var verMicroXslt;
var verMajorExslt;
var verMinorExslt;
var verMicroExslt;
var verLibxmlReq;
var verCvs;
var useCvsVer = true;
/* Libxslt features. */
var withTrio = false;
var withXsltDebug = true;
var withMemDebug = false;
var withDebugger = true;
var withIconv = true;
var withZlib = false;
var withCrypto = true;
var withModules = false;
var withProfiler = true;
var withPython = false;
/* Win32 build options. */
var dirSep = "\\";
var compiler = "msvc";
var cruntime = "/MD";
var vcmanifest = false;
var buildDebug = 0;
var buildStatic = 0;
var buildPrefix = ".";
var buildBinPrefix = "";
var buildIncPrefix = "";
var buildLibPrefix = "";
var buildSoPrefix = "";
var buildInclude = ".";
var buildLib = ".";
/* Local stuff */
var error = 0;

/* Helper function, transforms the option variable into the 'Enabled'
   or 'Disabled' string. */
function boolToStr(opt)
{
	if (opt == false)
		return "no";
	else if (opt == true)
		return "yes";
	error = 1;
	return "*** undefined ***";
}

/* Helper function, transforms the argument string into the boolean
   value. */
function strToBool(opt)
{
	if (opt == "0" || opt == "no")
		return false;
	else if (opt == "1" || opt == "yes")
		return true;
	error = 1;
	return false;
}

/* Displays the details about how to use this script. */
function usage()
{
	var txt;
	txt = "Usage:\n";
	txt += "  cscript " + WScript.ScriptName + " <options>\n";
	txt += "  cscript " + WScript.ScriptName + " help\n\n";
	txt += "Options can be specified in the form <option>=<value>, where the value is\n";
	txt += "either 'yes' or 'no'.\n\n";
	txt += "XSLT processor options, default value given in parentheses:\n\n";
	txt += "  trio:       Enable TRIO string manipulator (" + (withTrio? "yes" : "no")  + ")\n";
	txt += "  xslt_debug: Enable XSLT debbugging module (" + (withXsltDebug? "yes" : "no")  + ")\n";
	txt += "  mem_debug:  Enable memory debugger (" + (withMemDebug? "yes" : "no")  + ")\n";
	txt += "  debugger:   Enable external debugger support (" + (withDebugger? "yes" : "no")  + ")\n";
	txt += "  iconv:      Use iconv library (" + (withIconv? "yes" : "no")  + ")\n";
	txt += "  zlib:       Use zlib library (" + (withZlib? "yes" : "no") + ")\n";
	txt += "  crypto:     Enable Crypto support (" + (withCrypto? "yes" : "no") + ")\n";
	txt += "  modules:    Enable Module support (" + (withModules? "yes" : "no") + ")\n";
	txt += "  profiler:   Enable Profiler support (" + (withProfiler? "yes" : "no") + ")\n";
	txt += "  python:     Build Python bindings (" + (withPython? "yes" : "no")  + ")\n";
	txt += "\nWin32 build options, default value given in parentheses:\n\n";
	txt += "  compiler:   Compiler to be used [msvc|mingw] (" + compiler + ")\n";
	txt += "  cruntime:   C-runtime compiler option (only msvc) (" + cruntime + ")\n";
	txt += "  vcmanifest: Embed VC manifest (only msvc) (" + (vcmanifest? "yes" : "no") + ")\n";
	txt += "  debug:      Build unoptimised debug executables (" + (buildDebug? "yes" : "no")  + ")\n";
	txt += "  static:     Link xsltproc statically to libxslt (" + (buildStatic? "yes" : "no")  + ")\n";
	txt += "              Note: automatically enabled if cruntime is not /MD or /MDd\n";
	txt += "  prefix:     Base directory for the installation (" + buildPrefix + ")\n";
	txt += "  bindir:     Directory where xsltproc and friends should be installed\n";
	txt += "              (" + buildBinPrefix + ")\n";
	txt += "  incdir:     Directory where headers should be installed\n";
	txt += "              (" + buildIncPrefix + ")\n";
	txt += "  libdir:     Directory where static and import libraries should be\n";
	txt += "              installed (" + buildLibPrefix + ")\n";
	txt += "  sodir:      Directory where shared libraries should be installed\n"; 
	txt += "              (" + buildSoPrefix + ")\n";
	txt += "  include:    Additional search path for the compiler, particularily\n";
	txt += "              where libxml headers can be found (" + buildInclude + ")\n";
	txt += "  lib:        Additional search path for the linker, particularily\n";
	txt += "              where libxml library can be found (" + buildLib + ")\n";
	WScript.Echo(txt);
}

/* Discovers the version we are working with by reading the apropriate
   configuration file. Despite its name, this also writes the configuration
   file included by our makefile. */
function discoverVersion()
{
	var fso, cf, vf, ln, s, m;
	fso = new ActiveXObject("Scripting.FileSystemObject");
	verCvs = "";
	cf = fso.OpenTextFile(configFile, 1);
	if (compiler == "msvc")
		versionFile = ".\\config.msvc";
	else if (compiler == "mingw")
		versionFile = ".\\config.mingw";
	vf = fso.CreateTextFile(versionFile, true);
	vf.WriteLine("# " + versionFile);
	vf.WriteLine("# This file is generated automatically by " + WScript.ScriptName + ".");
	vf.WriteBlankLines(1);
	while (cf.AtEndOfStream != true) {
		ln = cf.ReadLine();
		s = new String(ln);
		if (m = s.match(/^m4_define\(\[MAJOR_VERSION\], \[(.*)\]\)/)) {
			vf.WriteLine("LIBXSLT_MAJOR_VERSION=" + m[1]);
			verMajorXslt = m[1];
		} else if(m = s.match(/^m4_define\(\[MINOR_VERSION\], \[(.*)\]\)/)) {
			vf.WriteLine("LIBXSLT_MINOR_VERSION=" + m[1]);
			verMinorXslt = m[1];
		} else if(m = s.match(/^m4_define\(\[MICRO_VERSION\], \[(.*)\]\)/)) {
			vf.WriteLine("LIBXSLT_MICRO_VERSION=" + m[1]);
			verMicroXslt = m[1];
		} else if (s.search(/^LIBEXSLT_MAJOR_VERSION=/) != -1) {
			vf.WriteLine(s);
			verMajorExslt = s.substring(s.indexOf("=") + 1, s.length);
		} else if(s.search(/^LIBEXSLT_MINOR_VERSION=/) != -1) {
			vf.WriteLine(s);
			verMinorExslt = s.substring(s.indexOf("=") + 1, s.length);
		} else if(s.search(/^LIBEXSLT_MICRO_VERSION=/) != -1) {
			vf.WriteLine(s);
			verMicroExslt = s.substring(s.indexOf("=") + 1, s.length);
		} else if(s.search(/^LIBXML_REQUIRED_VERSION=/) != -1) {
			vf.WriteLine(s);
			verLibxmlReq = s.substring(s.indexOf("=") + 1, s.length);
		}
	}
	cf.Close();
	vf.WriteLine("WITH_TRIO=" + (withTrio? "1" : "0"));
	vf.WriteLine("WITH_DEBUG=" + (withXsltDebug? "1" : "0"));
	vf.WriteLine("WITH_DEBUGGER=" + (withDebugger? "1" : "0"));
	vf.WriteLine("WITH_ICONV=" + (withIconv? "1" : "0"));
	vf.WriteLine("WITH_ZLIB=" + (withZlib? "1" : "0"));
	vf.WriteLine("WITH_CRYPTO=" + (withCrypto? "1" : "0"));
	vf.WriteLine("WITH_MODULES=" + (withModules? "1" : "0"));
	vf.WriteLine("WITH_PROFILER=" + (withProfiler? "1" : "0"));
	vf.WriteLine("WITH_PYTHON=" + (withPython? "1" : "0"));
	vf.WriteLine("DEBUG=" + (buildDebug? "1" : "0"));
	vf.WriteLine("STATIC=" + (buildStatic? "1" : "0"));
	vf.WriteLine("PREFIX=" + buildPrefix);
	vf.WriteLine("BINPREFIX=" + buildBinPrefix);
	vf.WriteLine("INCPREFIX=" + buildIncPrefix);
	vf.WriteLine("LIBPREFIX=" + buildLibPrefix);
	vf.WriteLine("SOPREFIX=" + buildSoPrefix);
	if (compiler == "msvc") {
		vf.WriteLine("INCLUDE=$(INCLUDE);" + buildInclude);
		vf.WriteLine("LIB=$(LIB);" + buildLib);
		vf.WriteLine("CRUNTIME=" + cruntime);
		vf.WriteLine("VCMANIFEST=" + (vcmanifest? "1" : "0"));
	} else if (compiler == "mingw") {
		vf.WriteLine("INCLUDE+=;" + buildInclude);
		vf.WriteLine("LIB+=;" + buildLib);
	}
	vf.Close();
}

/* Configures libxslt. This one will generate xsltconfig.h from xsltconfig.h.in
   taking what the user passed on the command line into account. */
function configureXslt()
{
	var fso, ofi, of, ln, s;
	fso = new ActiveXObject("Scripting.FileSystemObject");
	ofi = fso.OpenTextFile(optsFileInXslt, 1);
	of = fso.CreateTextFile(optsFileXslt, true);
	while (ofi.AtEndOfStream != true) {
		ln = ofi.ReadLine();
		s = new String(ln);
		if (s.search(/\@VERSION\@/) != -1) {
			of.WriteLine(s.replace(/\@VERSION\@/, 
				verMajorXslt + "." + verMinorXslt + "." + verMicroXslt));
		} else if (s.search(/\@LIBXSLT_VERSION_NUMBER\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_VERSION_NUMBER\@/, 
				verMajorXslt*10000 + verMinorXslt*100 + verMicroXslt*1));
		} else if (s.search(/\@LIBXSLT_VERSION_EXTRA\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_VERSION_EXTRA\@/, verCvs));
		} else if (s.search(/\@WITH_TRIO\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_TRIO\@/, withTrio? "1" : "0"));
		} else if (s.search(/\@WITH_XSLT_DEBUG\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_XSLT_DEBUG\@/, withXsltDebug? "1" : "0"));
		} else if (s.search(/\@WITH_DEBUGGER\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_DEBUGGER\@/, withDebugger? "1" : "0"));
		} else if (s.search(/\@WITH_MODULES\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_MODULES\@/, withModules? "1" : "0"));
		} else if (s.search(/\@WITH_PROFILER\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_PROFILER\@/, withProfiler? "1" : "0"));
		} else if (s.search(/\@LIBXSLT_DEFAULT_PLUGINS_PATH\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_DEFAULT_PLUGINS_PATH\@/, "NULL"));
		} else
			of.WriteLine(ln);
	}
	ofi.Close();
	of.Close();
}

/* Configures libexslt. This one will generate exsltconfig.h from exsltconfig.h.in
   taking what the user passed on the command line into account. */
function configureExslt()
{
	var fso, ofi, of, ln, s;
	fso = new ActiveXObject("Scripting.FileSystemObject");
	ofi = fso.OpenTextFile(optsFileInExslt, 1);
	of = fso.CreateTextFile(optsFileExslt, true);
	while (ofi.AtEndOfStream != true) {
		ln = ofi.ReadLine();
		s = new String(ln);
		if (s.search(/\@VERSION\@/) != -1) {
			of.WriteLine(s.replace(/\@VERSION\@/, 
				verMajorExslt + "." + verMinorExslt + "." + verMicroExslt));
		} else if (s.search(/\@LIBEXSLT_VERSION_NUMBER\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBEXSLT_VERSION_NUMBER\@/, 
				verMajorExslt*10000 + verMinorExslt*100 + verMicroExslt*1));
		} else if (s.search(/\@LIBEXSLT_VERSION_EXTRA\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBEXSLT_VERSION_EXTRA\@/, verCvs));
		} else if (s.search(/\@WITH_CRYPTO\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_CRYPTO\@/, withCrypto? "1" : "0"));
		} else if (s.search(/\@WITH_MODULES\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_MODULES\@/, withModules? "1" : "0"));
		} else
			of.WriteLine(ln);
	}
	ofi.Close();
	of.Close();
}

/* Configures Python bindings. Otherwise identical to the above */
function configureLibxsltPy()
{
	var pyOptsFileIn = baseDir + "\\python\\setup.py.in";
	var pyOptsFile = baseDir + "\\python\\setup.py";
	var fso, ofi, of, ln, s;
	fso = new ActiveXObject("Scripting.FileSystemObject");
	ofi = fso.OpenTextFile(pyOptsFileIn, 1);
	of = fso.CreateTextFile(pyOptsFile, true);
	while (ofi.AtEndOfStream != true) {
		ln = ofi.ReadLine();
		s = new String(ln);
		if (s.search(/\@VERSION\@/) != -1) {
			of.WriteLine(s.replace(/\@VERSION\@/, 
				verMajorXslt + "." + verMinorXslt + "." + verMicroXslt));
		} else if (s.search(/\@prefix\@/) != -1) {
			of.WriteLine(s.replace(/\@prefix\@/, buildPrefix));
		} else if (s.search(/\@LIBXSLT_VERSION_NUMBER\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_VERSION_NUMBER\@/, 
				verMajorXslt*10000 + verMinorXslt*100 + verMicroXslt*1));
		} else if (s.search(/\@LIBXSLT_VERSION_EXTRA\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_VERSION_EXTRA\@/, verCvs));
		} else if (s.search(/\@WITH_TRIO\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_TRIO\@/, withTrio? "1" : "0"));
		} else if (s.search(/\@WITH_XSLT_DEBUG\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_XSLT_DEBUG\@/, withXsltDebug? "1" : "0"));
		} else if (s.search(/\@WITH_DEBUGGER\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_DEBUGGER\@/, withDebugger? "1" : "0"));
		} else if (s.search(/\@WITH_MODULES\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_MODULES\@/, withModules? "1" : "0"));
		} else if (s.search(/\@WITH_PROFILER\@/) != -1) {
			of.WriteLine(s.replace(/\@WITH_PROFILER\@/, withProfiler? "1" : "0"));
		} else if (s.search(/\@LIBXSLT_DEFAULT_PLUGINS_PATH\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXSLT_DEFAULT_PLUGINS_PATH\@/, "NULL"));
		} else if (s.search(/\@LIBXML_REQUIRED_VERSION\@/) != -1) {
			of.WriteLine(s.replace(/\@LIBXML_REQUIRED_VERSION\@/, verLibxmlReq));
		} else
			of.WriteLine(ln);
	}
	ofi.Close();
	of.Close();
}

/* Creates the readme file for the binary distribution of 'bname', for the
   version 'ver' in the file 'file'. This one is called from the Makefile when
   generating a binary distribution. The parameters are passed by make. */
function genReadme(bname, ver, file)
{
	var fso, f;
	fso = new ActiveXObject("Scripting.FileSystemObject");
	f = fso.CreateTextFile(file, true);
	f.WriteLine("  " + bname + " " + ver);
	f.WriteLine("  --------------");
	f.WriteBlankLines(1);
	f.WriteLine("  This is " + bname + ", version " + ver + ", binary package for the native Win32/IA32");
	f.WriteLine("platform.");
	f.WriteBlankLines(1);
	f.WriteLine("  The files in this package do not require any special installation");
	f.WriteLine("steps. Extract the contents of the archive whereever you wish and");
	f.WriteLine("make sure that your tools which use " + bname + " can find it.");
	f.WriteBlankLines(1);
	f.WriteLine("  For example, if you want to run the supplied utilities from the command");
	f.WriteLine("line, you can, if you wish, add the 'bin' subdirectory to the PATH");
	f.WriteLine("environment variable.");
	f.WriteLine("  If you want to make programmes in C which use " + bname + ", you'll");
	f.WriteLine("likely know how to use the contents of this package. If you don't, please");
	f.WriteLine("refer to your compiler's documentation."); 
	f.WriteBlankLines(1);
	f.WriteLine("  If there is something you cannot keep for yourself, such as a problem,");
	f.WriteLine("a cheer of joy, a comment or a suggestion, feel free to contact me using");
	f.WriteLine("the address below.");
	f.WriteBlankLines(1);
	f.WriteLine("                              Igor Zlatkovic (igor@zlatkovic.com)");
	f.Close();
}

/*
 * main(),
 * Execution begins here.
 */

/* Parse the command-line arguments. */
for (i = 0; (i < WScript.Arguments.length) && (error == 0); i++) {
	var arg, opt;
	arg = WScript.Arguments(i);
	opt = arg.substring(0, arg.indexOf("="));
	if (opt.length == 0)
		opt = arg.substring(0, arg.indexOf(":"));
	if (opt.length > 0) {
		if (opt == "xslt_debug")
			withXsltDebug = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "trio")
			withTrio = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "mem_debug")
			withMemDebug = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "debugger")
			withDebugger = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "debug")
			buildDebug = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "iconv")
			withIconv = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "zlib")
			withZlib  = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "crypto")
			withCrypto = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "modules")
			withModules = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "profiler")
			withProfiler = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "python")
			withPython = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "compiler")
			compiler = arg.substring(opt.length + 1, arg.length);
 		else if (opt == "cruntime")
 			cruntime = arg.substring(opt.length + 1, arg.length);
		else if (opt == "vcmanifest")
			vcmanifest = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "static")
			buildStatic = strToBool(arg.substring(opt.length + 1, arg.length));
		else if (opt == "prefix")
			buildPrefix = arg.substring(opt.length + 1, arg.length);
		else if (opt == "bindir")
			buildBinPrefix = arg.substring(opt.length + 1, arg.length);
		else if (opt == "libdir")
			buildLibPrefix = arg.substring(opt.length + 1, arg.length);
		else if (opt == "sodir")
			buildSoPrefix = arg.substring(opt.length + 1, arg.length);
		else if (opt == "incdir")
			buildIncPrefix = arg.substring(opt.length + 1, arg.length);
		else if (opt == "include")
			buildInclude = arg.substring(opt.length + 1, arg.length);
		else if (opt == "lib")
			buildLib = arg.substring(opt.length + 1, arg.length);
		else if (opt == "release")
			useCvsVer = false;
		else
			error = 1;
	} else if (i == 0) {
		if (arg == "genreadme") {
			// This command comes from the Makefile and will not be checked
			// for errors, because Makefile will always supply right parameters.
			genReadme(WScript.Arguments(1), WScript.Arguments(2), WScript.Arguments(3));
			WScript.Quit(0);
		} else if (arg == "help") {
			usage();
			WScript.Quit(0);
		}
	} else
		error = 1;
}
// If we have an error here, it is because the user supplied bad parameters.
if (error != 0) {
	usage();
	WScript.Quit(error);
}

// if user choses to link the c-runtime library statically into libxslt
// with /MT and friends, then we need to enable static linking for xsltproc
if (cruntime == "/MT" || cruntime == "/MTd" ||
		cruntime == "/ML" || cruntime == "/MLd") {
	buildStatic = 1;
}

if (buildStatic == 1 && withModules == 1) {
	WScript.Echo("Warning: Disabling plugin support.");
	WScript.Echo("");  
  WScript.Echo("Modules cannot be enabled when a statically linked cruntime has");
	WScript.Echo("been selected, or when xsltproc.exe is linked statically to libxslt.");
	WScript.Echo("");  
	withModules=0;
}

dirSep = "\\";
//if (compiler == "mingw")
//	dirSep = "/";
if (buildBinPrefix == "")
	buildBinPrefix = "$(PREFIX)" + dirSep + "bin";
if (buildIncPrefix == "")
	buildIncPrefix = "$(PREFIX)" + dirSep + "include";
if (buildLibPrefix == "")
	buildLibPrefix = "$(PREFIX)" + dirSep + "lib";
if (buildSoPrefix == "")
	buildSoPrefix = "$(PREFIX)" + dirSep + "bin";

// Discover the version.
discoverVersion();
if (error != 0) {
	WScript.Echo("Version discovery failed, aborting.");
	WScript.Quit(error);
}

var outVerString = baseNameXslt + " version: " + verMajorXslt + "." + verMinorXslt + "." + verMicroXslt;
if (verCvs && verCvs != "")
	outVerString += "-" + verCvs;
WScript.Echo(outVerString);
outVerString = baseNameExslt + " version: " + verMajorExslt + "." + verMinorExslt + "." + verMicroExslt;
if (verCvs && verCvs != "")
	outVerString += "-" + verCvs;
WScript.Echo(outVerString);

// Configure libxslt.
configureXslt();
if (error != 0) {
	WScript.Echo("Configuration failed, aborting.");
	WScript.Quit(error);
}

if (withPython == true) {
	configureLibxsltPy();
	if (error != 0) {
		WScript.Echo("Configuration failed, aborting.");
		WScript.Quit(error);
	}

}

// Configure libexslt.
configureExslt();
if (error != 0) {
	WScript.Echo("Configuration failed, aborting.");
	WScript.Quit(error);
}

// Create the Makefile.
var fso = new ActiveXObject("Scripting.FileSystemObject");
var makefile = ".\\Makefile.msvc";
if (compiler == "mingw")
	makefile = ".\\Makefile.mingw";
fso.CopyFile(makefile, ".\\Makefile", true);
WScript.Echo("Created Makefile.");
// Create the config.h.
var confighsrc = "..\\libxslt\\win32config.h";
var configh = "..\\config.h";
var f = fso.FileExists(configh);
if (f) {
	var t = fso.GetFile(configh);
	t.Attributes =0;
}
fso.CopyFile(confighsrc, configh, true);
WScript.Echo("Created config.h.");

// Display the final configuration.
var txtOut = "\nXSLT processor configuration\n";
txtOut += "----------------------------\n";
txtOut += "              Trio: " + boolToStr(withTrio) + "\n";
txtOut += "  Debugging module: " + boolToStr(withXsltDebug) + "\n";
txtOut += "  Memory debugging: " + boolToStr(withMemDebug) + "\n";
txtOut += "  Debugger support: " + boolToStr(withDebugger) + "\n";
txtOut += "         Use iconv: " + boolToStr(withIconv) + "\n";
txtOut += "         With zlib: " + boolToStr(withZlib) + "\n";
txtOut += "            Crypto: " + boolToStr(withCrypto) + "\n";
txtOut += "           Modules: " + boolToStr(withModules) + "\n";
txtOut += "          Profiler: " + boolToStr(withProfiler) + "\n";
txtOut += "   Python bindings: " + boolToStr(withPython) + "\n";
txtOut += "\n";
txtOut += "Win32 build configuration\n";
txtOut += "-------------------------\n";
txtOut += "          Compiler: " + compiler + "\n";
if (compiler == "msvc")
	txtOut += "  C-Runtime option: " + cruntime + "\n";
txtOut += "    Embed Manifest: " + boolToStr(vcmanifest) + "\n";
txtOut += "     Debug symbols: " + boolToStr(buildDebug) + "\n";
txtOut += "   Static xsltproc: " + boolToStr(buildStatic) + "\n";
txtOut += "    Install prefix: " + buildPrefix + "\n";
txtOut += "      Put tools in: " + buildBinPrefix + "\n";
txtOut += "    Put headers in: " + buildIncPrefix + "\n";
txtOut += "Put static libs in: " + buildLibPrefix + "\n";
txtOut += "Put shared libs in: " + buildSoPrefix + "\n";
txtOut += "      Include path: " + buildInclude + "\n";
txtOut += "          Lib path: " + buildLib + "\n";
WScript.Echo(txtOut);

// Done.
