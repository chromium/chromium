#!/usr/bin/tclsh
#
# This script appends additional token codes to the end of the
# parse.h file that lemon generates.  These extra token codes are
# not used by the parser.  But they are used by the tokenizer and/or
# the code generator.
#
#
set in [open [lindex $argv 0] rb]
set max 0
while {![eof $in]} {
  set line [gets $in]
  if {[regexp {^#define TK_} $line]} {
    puts $line
    set x [lindex $line 2]
    if {$x>$max} {set max $x}
  }
}
close $in

# The following are the extra token codes to be added.  SPACE and 
# ILLEGAL *must* be the last two token codes and they must be in that order.
#
set extras {
  TRUEFALSE
  ISNOT
  FUNCTION
  COLUMN
  AGG_FUNCTION
  AGG_COLUMN
  UMINUS
  UPLUS
  TRUTH
  REGISTER
  VECTOR
  SELECT_COLUMN
  IF_NULL_ROW
  ASTERISK
  SPAN
  END_OF_FILE
  UNCLOSED_STRING
  SPACE
  ILLEGAL
}
if {[lrange $extras end-1 end]!="SPACE ILLEGAL"} {
  error "SPACE and ILLEGAL must be the last two token codes and they\
         must be in that order"
}
foreach x $extras {
  incr max
  puts [format "#define TK_%-29s %4d" $x $max]
}

# Some additional #defines related to token codes.
#
puts "\n/* The token codes above must all fit in 8 bits */"
puts [format "#define %-20s %-6s" TKFLG_MASK 0xff]
puts "\n/* Flags that can be added to a token code when it is not"
puts "** being stored in a u8: */"
foreach {fg val comment} {
  TKFLG_DONTFOLD  0x100  {/* Omit constant folding optimizations */}
} {
  puts [format "#define %-20s %-6s %s" $fg $val $comment]
}
