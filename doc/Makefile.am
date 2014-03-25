XSLTPROC = /usr/bin/xsltproc

# Manpages
man1_MANS = iscsi-inq.1 iscsi-ls.1 iscsi-swp.1 iscsi-test-cu.1

EXTRA_DIST = iscsi-inq.1 iscsi-inq.xml \
	     iscsi-ls.1 iscsi-ls.xml \
	     iscsi-swp.1 iscsi-swp.xml \
	     iscsi-test-cu.1 iscsi-test-cu.xml

.xml.1:
if BUILD_MANPAGES
	$(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
else
	echo "Building without manpages. Use pre-compiled version of" $<
endif

