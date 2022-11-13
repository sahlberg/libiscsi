#!/bin/bash

case "$(uname)" in
    Linux)
	packages=(
	    autoconf
	    automake
	    docbook-xsl
	    libcunit1-dev
	    libgcrypt20-dev
	    librdmacm-dev
	    libtool
	    xsltproc
	)
	for p in "${packages[@]}"; do
	    sudo sh -c "apt-get install -y $p"
	done
	;;
    Darwin)
	;;
    MSYS*|MINGW*)
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-ada
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-fortran
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-libgfortran
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-objc
	pacman --noconfirm --remove mingw-w64-x86_64-libgccjit
	pacman --noconfirm --sync --refresh
	pacman --noconfirm --sync --needed autoconf
	pacman --noconfirm --sync --needed automake
	pacman --noconfirm --sync --needed diffutils
	pacman --noconfirm --sync --needed docbook-xsl
	pacman --noconfirm --sync --needed libtool
	pacman --noconfirm --sync --needed make
	pacman --noconfirm --sync --needed mingw-w64-x86_64-cunit
	pacman --noconfirm --sync --needed mingw-w64-x86_64-gcc
	;;
esac
