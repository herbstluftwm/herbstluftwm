#!/usr/bin/env bash

version="$1"

if [ -z "$1" ] || [ "$1" = -h ] ; then
    echo "$0 VERSIONMAJOR.VERSIONMINOR.VERSIONPATCH"
    echo "  Releases the specified version (tagging and tarball creation)"
    exit 0
fi

if git status --porcelain | grep '^ M' ; then
    echo "You have unstaged changes. Fix them or use git stash" >&2
    exit 1
fi

echo "==> Release commit"
echo ":: Patching VERSION"
echo "$version" > VERSION

echo ":: Patching NEWS"
date=$(date +%Y-%m-%d)
newheader="Release $version on $date"
newunderline="$(echo $newheader | sed 's/./-/g')"
headerexp="^Current git version$"
# this requires new sed
sed -i -e "/$headerexp/,+1s/^[-]*$/$newunderline/" \
       -e "s/$headerexp/$newheader/" NEWS

echo ":: Committing changes"
git add NEWS VERSION
git commit -m "Release $version"
echo ":: Tagging commit"
git tag -s "v$version" -m "Release $version"

echo "==> Tarball"
echo ":: Tarball creation"
make BUILDDIR=.build-doc-"$version" tar
tarball="herbstluftwm-$version.tar.gz"
md5sum=$(md5sum "$tarball" | head -c 13 )
echo ":: Patching www/download.txt"
line=$(printf "| %-7s | $date | $md5sum...%15s| link:tarballs/%s[tar.gz] |link:tarballs/%s.sig[sig]" \
                $version                  ' '                 "$tarball" "$tarball")
linerexp="// do not remove this: next version line will be added here"
sed -i "s#^$linerexp\$#$line\n$linerexp#" www/download.txt
echo ":: Committing changes"
git add www/download.txt
git commit -m "www: Add $version tarball"

echo
echo "Still to do:"
echo "1. Add the following line to the MD5SUMS file on the mirror:"
md5sum "$tarball"
echo "2. Make www files and install them on the remote"
echo "3. Push the changes to all public remotes (including --tags)"
