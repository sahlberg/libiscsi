Libiscsi is a client-side library to implement the iSCSI protocol that can be
used to access the resources of an iSCSI target.

The library is fully asynchronous with regards to iSCSI commands and SCSI
tasks, but a synchronous layer is also provided for ease of use for simpler
applications.

The utils directory contains a handful of useful iSCSI utilities such as
logging in to and enumerating all targets on a portal and all devices of a
target.

The examples directory contains example implementation of how to access both
the synchronous and asynchronous APIs of libiscsi.

The test-tool directory contains an iSCSI conformance test suite. To include
the test suite in the compilation, install libcunit1-dev first.

Libiscsi is a work in progress.  It aims to become a fully asynchronous
library for iSCSI functionality, including all features required to establish
and maintain an iSCSI session, as well as a low-level SCSI library to create
SCSI CDBs and parse/unmarshall data-in structures.


Installation
============
./autogen.sh
./configure
make
sudo make install


Build RPM
=========
To build RPMs run the following script from the libiscsi root directory
./packaging/RPM/makerpms.sh

iSCSI URL Format
================
iSCSI devices are specified by a URL format of the following form :
    iscsi://[<username>[%<password>]@]<host>[:<port>]/<target-iqn>/<lun>[?<argument>[&<argument>]*]
or
    iser://[<username>[%<password>]@]<host>[:<port>]/<target-iqn>/<lun>[?<argument>[&<argument>]*]

Arguments:
Username and password for bidirectional CHAP authentication:
target_user=<account>
target_password=<password>
header_digest=<crc32c|none>
data_digest=<crc32c|none>
auth=<md5|sha1|sha-256|sha3-256>
Transport:
iser


Example:
    iscsi://server/iqn.ronnie.test/1


MULTITHREADING
==============
Multithreading is supported both on Linux, using pthreads, and Windows, using native API.
By default libicsi will start with multithreading disabled and you will need
to activate once connected to the LUN.
There are examples of multithreading in the examples directory.

CHAP Authentication
===================
CHAP authentication can be specified two ways. Either via the URL itself
or through environment variables.

Note that when setting it via the URL, be careful so that username/password
will not be visible in logfiles or the process list.

URL
---
CHAP authentication via URL is specified by providing <username>%<password>@
in the server part of the URL:

Example:
    iscsi://ronnie%password@server/iqn.ronnie.test/1

Environment variables
---------------------
Setting the CHAP authentication via environment variables:
    LIBISCSI_CHAP_USERNAME=ronnie
    LIBISCSI_CHAP_PASSWORD=password

Example:
   LIBISCSI_CHAP_PASSWORD=password iscsi-inq iscsi://ronnie@10.1.1.27/iqn.ronnie.test/1

Bidirectional CHAP Authentication
=================================

Bidirectional CHAP is when you not only authenticate the initiator to the
target but also authenticate the target back to the initiator.  This is only
available if you also first specify normal authentication as per the previous
section.

Bidirectional CHAP can be set either via URL arguments or via environment
variables. If specifying it via URL arguments, be careful so that you do
not leak the username/password via logfiles or the process list or similar.

URL
---
URL arguments contain the '&' character so make sure to escape them properly
if you pass them in via a commandline.

Example:
    iscsi://127.0.0.1/iqn.ronnie.test/1?target_user=target\&target_password=target

Environment variables
---------------------
Setting the CHAP authentication via environment variables:
    LIBISCSI_CHAP_TARGET_USERNAME=target
    LIBISCSI_CHAP_TARGET_PASSWORD=password


IPv6 support
============

Libiscsi supports IPv6, either as names resolving into IPv6 addresses or when
IPv6 addresses are explicitely set in the URL.  When specifying IPv6 addresses
in the URL, they have to be specified in [...] bracket form.

Example:
  iscsi://[fec0:2727::3]:3260/iqn.ronnie.test/1


Header Digest
=============

Libiscsi supports HeaderDigest.  By default, libiscsi will offer None,CRC32C
and let the target pick whether Header digest is to be used or not.  This can
be overridden by an application by calling iscsi_set_header_digest() if the
application wants to force a specific setting.


Data Digest
===========

Libiscsi supports DataDigest.  By default, libiscsi will offer None so that
Data digest will not be used, no matter what the target setting is.  This can
be overridden by an application by calling iscsi_set_data_digest() if the
application wants to force a specific setting.


Patches
=======

The patches subdirectory contains patches to make some external packages
iSCSI-aware and make them use libiscsi.  Currently we have SG3-UTILS and MTX.
Patches for other packages would be welcome.


ISCSI-TEST-CU
=============
iscsi-test-cu is a CUnit based test tool for scsi and iscsi.

iscsi-test-cu depends on the CUnit library and will only build if libcunit can
be found during configure.

The configure script will check if a suitable libcunit is available and only
build the test tool if it can find libcunit.
This test is done toward the end of the configure phase and should result
in a line similar to :

checking whether libcunit is available... yes

Test family/suite/test
----------------------
Tests are divided up in families, suites and tests and are specified as
  --test=<family>.<suite>.<test>

A <family> is a logical collection of test suites to cover a broad set
of functionality. Example families are 'SCSI' for performing all tests for
SCSI commands and 'iSCSI' that contain tests for the iSCSI layer.

To run all tests in the SCSI family you can just specify
  --test=SCSI or --test=SCSI.*.*

The next layer of tests are the suites. Within a family there are a collection
of suites that perform test to cover a specific area of functionality.
For example, to run all SCSI tests that cover the Read10 opcode you would
specify it as
  --test=SCSI.Read10 or --test=SCSI.Read10.*

Finally at the lowest level you have the individual tests. These tests perform
specific topic in a suite.
For example, we have tests for the Read10 opcode that verifies that the target
implements the DPO/FUA flags properly.
To run those tests you would specify
  --test=SCSI.Read10.DpoFua

Test discovery
--------------
To discover which tests exist you can use the command 
  iscsi-test-cu --list

Examples
--------
Run the DpoFua test for Read10
  iscsi-test-cu --test=SCSI.Read10.DpoFua iscsi://127.0.0.1/iqn.example.test/1

Run all Read10 tests
  iscsi-test-cu --test=SCSI.Read10 iscsi://127.0.0.1/iqn.example.test/1

Run all SCSI tests for all opcodes
  iscsi-test-cu --test=SCSI iscsi://127.0.0.1/iqn.example.test/1

Initiator names used by the test suite
--------------------------------------
Most tests only require a single login to the target, but some tests,
for example the it nexus loss tests may need to login two separate sessions.
By default the initiator names use for the logins will be
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test";
for the primary connection for all tests, and
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test-2"
for the secondary connection for the test that needs two sessions.
These names can be controlled by using the arguments
--initiator-name and --initiator-name-2

The tests are all self-documented to describe what they test and how they test.
Use -V with a test to print the documentation for a test.

XML/JUNIT
=========
iscsi-test-cu can produce machine-readable test results for consumption by your
CI server. Use the --xml option with any test suite(s), and a file called
CUnitAutomated-Results.xml will be written to your current working directory.
These results can be converted to JUnit format using this script:

https://raw.githubusercontent.com/cyrusimap/cyrus-imapd/master/cunit/cunit-to-junit.pl

See also:

CUnit-List.xsl
--------------
https://code.google.com/p/warc-tools/source/browse/trunk/utest/outputs/CUnit-List.xsl

CUnit-Run.xsl
-------------
https://code.google.com/p/mdflib/source/browse/trunk/cunit/CUnit-Run.xsl


Linux SG_IO devices
-------------------
When used on Linux, the test tool also supports talking directly to local SG_IO
devices. Accessing SG_IO devices often require that you are root:

  sudo iscsi-test-cu --test LINUX.Read10.Simple /dev/sg1


Unit Tests
----------
The tests directory contains test scripts and programs to verify the
functionality of libiscsi itself. These tests require that you have STGT
version 1.0.58 or later installed to use as a taget to test against.
To run the tests:
  cd tests
  make test


SUPPORTED PLATFORMS
===================

libiscsi is pure POSIX and should with some tweaks run on any host that
provides a POSIX-like environment.

Libiscsi has been tested on:
* Linux (32 and 64 bit)
* Cygwin
* FreeBSD
* Windows (Win7-VisualStudio10)
* OpenSolaris
* Solaris 11 : Use "gmake" to build.
* OS X

RELEASE TARBALLS
================

Release tarballs are available at https://github.com/sahlberg/libiscsi/tags.

CONTRIBUTING
============

If you want to contribute, please do.  For sending me patches you can either
do that by sending a pull request to my github repo or you can send them in an
email directly to me at ronniesahlberg@gmail.com


MAILINGLIST
===========

A libiscsi mailing list is available at
http://groups.google.com/group/libiscsi. Announcements of new versions of
libiscsi will be posted to this list.
