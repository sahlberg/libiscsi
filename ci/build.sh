#!/bin/bash

configure_options=(
    --enable-manpages
    --enable-test-tool
    --enable-tests
    --enable-examples
)

case "$(uname)" in
    MSYS*|MINGW*)
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
