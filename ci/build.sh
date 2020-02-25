#!/bin/sh

./autogen.sh &&
    ./configure --enable-manpages --enable-test-tool --enable-tests \
		--enable-examples &&
    make &&
    sudo make install
