
# This file is used by EnableLanguage in cmGlobalGenerator to
# determine that that selected RC compiler can actually compile
# and link the most basic of programs.   If not, a fatal error
# is set and cmake stops processing commands and will not generate
# any makefiles or projects.

# For now there is no way to do a try compile on just a .rc file
# so just do nothing in here.
SET(CMAKE_RC_COMPILER_WORKS 1 CACHE INTERNAL "")
