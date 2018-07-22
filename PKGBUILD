# Maintainer: thorsten w. <p@thorsten-wissmann.de>
pkgname=herbstluftwm-halflife-git
_pkgname=herbstluftwm
pkgver=0.7.0.r59.gb8ebf08
pkgrel=1
epoch=1
pkgdesc="Manual tiling, cwd aware window manager for X"
arch=('i686' 'x86_64')
url="http://herbstluftwm.org"
license=('BSD')
depends=( 'glib2>=2.24' 'libx11' 'libxinerama')
optdepends=(
        'bash: needed by most scripts'
        'xterm: used by the default autostart'
        'dmenu: needed by some scripts'
        'dzen2: needed by panel.sh'
        'dzen2-xft-xpm-xinerama-git: view icons as tags'
    )
makedepends=('git' 'asciidoc')
provides=($_pkgname)
conflicts=($_pkgname)
source=("$pkgname::git://github.com/countingsort/$_pkgname#branch=halflifewm")
md5sums=( SKIP )

pkgver() {
  cd ${pkgname}
  git describe --tags --long | sed -r 's,^[^0-9]*,,;s,([^-]*-g),r\1,;s,[-_],.,g'
}

build() {
  cd ${pkgname}
  make PREFIX=/usr
}

package() {
  cd ${pkgname}
  make PREFIX=/usr DESTDIR="$pkgdir" install
  install -D -m644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
