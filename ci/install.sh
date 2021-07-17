#!/bin/bash

case "$(uname)" in
    Linux)
	packages=(
	    libcunit1-dev
	    libgcrypt20-dev
	    librdmacm-dev
	    xsltproc
	)
	for p in "${packages[@]}"; do
	    sudo sh -c "apt-get install -y $p"
	done
	;;
    Darwin)
	;;
esac
