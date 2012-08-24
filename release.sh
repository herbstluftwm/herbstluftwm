#!/bin/bash

VERSION="$1"

if [ -z "$1" ] || [ "$1" = -h ] ; then
    echo "$0 VERSIONMAJOR.VERSIONMINOR"
    echo "  Releases the specified version (tagging and tarball creation)"
    exit 0
fi

if git status --porcelain | grep '^ M' ; then
    echo "You have unstaged changes. Fix them or use git stash" >&2
    exit 1
fi

VERSION_MAJOR="${VERSION%.*}"
VERSION_MINOR="${VERSION#*.}"


echo "==> Release commit"
echo ":: Patching version.mk"
sed -i "s/^VERSION_MAJOR.*$/VERSION_MAJOR = $VERSION_MAJOR/" version.mk
sed -i "s/^VERSION_MINOR.*$/VERSION_MINOR = $VERSION_MINOR/" version.mk

echo ":: Patching NEWS"
date=$(date +%Y-%m-%d)
newheader="Release $VERSION on $date"
newunderline="$(echo $newheader|sed 's/./-/g')"
headerexp="^Next Release: [^ ]*$"
# this requires news sed
sed -i -e "/$headerexp/,+1s/^[-]*$/$newunderline/" NEWS
sed -i -e "s/$headerexp/$newheader/" NEWS

echo ":: Commiting changes"
git add NEWS version.mk
git commit -m "Release $VERSION"
echo ":: Tagging commit"
git tag -s v$VERSION -m "Release $VERSION"

echo "==> Tarball"
echo ":: Tarball creation"
make tar
tarball=herbstluftwm-$VERSION.tar.gz
md5sum=$(md5sum $tarball| head -c 13 )
echo ":: Patching www/index.txt"
line=$(printf "| %-7s | $date | $md5sum...%15s| link:tarballs/%s[tar.gz]" \
                $VERSION                  ' '                 "$tarball" )
linerexp="// do not remove this: next version line will be added here"
sed -i "s#^$linerexp\$#$line\n$linerexp#" www/index.txt
echo ":: Commiting changes"
git add www/index.txt
git commit -m "www: Add $VERSION tarball"

echo
echo "Still to do:"
echo "1. Add the following line to the MD5SUMS file on the mirror:"
md5sum $tarball
echo "2. Make www files and install them on the remote"
echo "3. Push the changes to all public remotes (including --tags)"

