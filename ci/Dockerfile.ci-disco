FROM ubuntu:disco-20190204

# Build deps
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install \
    ccache \
    clang-7 \
    cmake \
    curl \
    g++-8 \
    git \
    iwyu \
    lcov \
    libx11-dev \
    libxext-dev \
    libxinerama-dev \
    libxml2-utils \
    libxrandr-dev \
    ninja-build \
    pkg-config \
    python3.7 \
    tox \
    xdotool \
    xterm \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# Doc deps would be:
# (not installed for now because they're huge):
#    asciidoc \
#    docbook-xml \
#    docbook-xsl \
#    xsltproc
