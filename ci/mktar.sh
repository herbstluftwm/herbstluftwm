#!/usr/bin/env bash

set -e

# create a source tarball including pre-built documentation
gitroot=${gitroot:-$(git rev-parse --show-toplevel)}
builddir=${builddir:-"${gitroot}/build/"}
version=${version:-$(< "${gitroot}/VERSION")}
tarname=${tarname:-herbstluftwm-$version}

::() {
    echo -e "\e[1;33m:: \e[0;32m$*\e[0m" >&2
    "$@"
}

# we first create a .tar file and then gzip it because otherwise
# we can't add additional files to the tar

# take the git source files
:: git archive --prefix="$tarname/" -o "${tarname}".tar HEAD

# add compiled documentation
:: tar --transform="flags=r;s,${builddir}/,${tarname}/,"  \
    --owner=0 --group=0 \
    -uvf "${tarname}.tar" "${builddir}"/doc/*.{html,json,[1-9]}

:: gzip "${tarname}.tar"

echo To sign the tarball, run: gpg --detach-sign "${tarname}.tar.gz" >&2
