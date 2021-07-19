#!/bin/bash

configure_options=(
    --enable-manpages
    --enable-test-tool
    --enable-tests
    --enable-examples
)

case "$(uname)" in
    MSYS*|MINGW*)
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-ada
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-fortran
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-libgfortran
	pacman --noconfirm --remove mingw-w64-x86_64-gcc-objc
	pacman --noconfirm --sync --refresh
	pacman --noconfirm --sync --needed diffutils
	pacman --noconfirm --sync --needed make
	pacman --noconfirm --sync --needed mingw-w64-x86_64-gcc
	pacman --noconfirm --sync --needed mingw-w64-x86_64-cunit
	export PATH="/mingw64/bin:$PATH"
	configure_options+=(--disable-shared)
	;;
esac

./autogen.sh &&
    ./configure "${configure_options[@]}" &&
    make &&
    case "$(uname)" in
	MSYS*|MINGW*)
	    ;;
	*)
	    sudo make install
	    ;;
    esac
