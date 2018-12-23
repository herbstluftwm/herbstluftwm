## get project dependencies
# Xlib
find_package(X11 REQUIRED)
# GLib (will be removed later)
include(FindPkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
