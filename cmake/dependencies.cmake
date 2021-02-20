## get project dependencies
# Xlib
find_package(PkgConfig)

pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XRANDR REQUIRED xrandr)
pkg_check_modules(XINERAMA xinerama)
pkg_check_modules(XEXT REQUIRED xext)

# for transparency support
pkg_check_modules(XRENDER REQUIRED xrender)

# for xft support:
pkg_check_modules(XFT REQUIRED xft)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# vim: et:ts=4:sw=4
