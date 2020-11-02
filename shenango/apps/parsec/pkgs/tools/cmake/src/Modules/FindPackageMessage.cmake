# FIND_PACKAGE_MESSAGE(<name> "message for user" "find result details")
#
# This macro is intended to be used in FindXXX.cmake modules files.
# It will print a message once for each unique find result.
# This is useful for telling the user where a package was found.
# The first argument specifies the name (XXX) of the package.
# The second argument specifies the message to display.
# The third argument lists details about the find result so that
# if they change the message will be displayed again.
# The macro also obeys the QUIET argument to the find_package command.
#
# Example:
#
#  IF(X11_FOUND)
#    FIND_PACKAGE_MESSAGE(X11 "Found X11: ${X11_X11_LIB}"
#      "[${X11_X11_LIB}][${X11_INCLUDE_DIR}]")
#  ELSE(X11_FOUND)
#   ...
#  ENDIF(X11_FOUND)

FUNCTION(FIND_PACKAGE_MESSAGE pkg msg details)
  # Avoid printing a message repeatedly for the same find result.
  IF(NOT ${pkg}_FIND_QUIETLY)
    SET(DETAILS_VAR FIND_PACKAGE_MESSAGE_DETAILS_${pkg})
    IF(NOT "${details}" STREQUAL "${${DETAILS_VAR}}")
      # The message has not yet been printed.
      MESSAGE(STATUS "${msg}")

      # Save the find details in the cache to avoid printing the same
      # message again.
      SET("${DETAILS_VAR}" "${details}"
        CACHE INTERNAL "Details about finding ${pkg}")
    ENDIF(NOT "${details}" STREQUAL "${${DETAILS_VAR}}")
  ENDIF(NOT ${pkg}_FIND_QUIETLY)
ENDFUNCTION(FIND_PACKAGE_MESSAGE)
