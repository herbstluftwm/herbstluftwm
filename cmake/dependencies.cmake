## get project dependencies
# Xlib
find_package(PkgConfig)

pkg_check_modules(X11 x11)
pkg_check_modules(XFT xft)
pkg_check_modules(XRANDR xrandr)
pkg_check_modules(XINERAMA xinerama)
pkg_check_modules(XEXT xext)

pkg_check_modules(FREETYPE freetype2)

# vim: et:ts=4:sw=4
