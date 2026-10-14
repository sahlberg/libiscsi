#
# CMake build for libiscsi.
#
# This is an alternative to the autotools build and is kept deliberately
# close to it: the same feature tests, the same warning flags, the same
# library version and the same set of exported symbols.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build
#   cmake --install build
#
cmake_minimum_required(VERSION 3.13)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported: they would overwrite the "
        "config.h and Makefile of the autotools build.\n"
        "Use a build directory instead, for example:\n"
        "  cmake -B build && cmake --build build")
endif()

#
# Take the version from the same place that configure.ac takes it from so
# that the two build systems can never disagree.
#
set(LIBISCSI_VERSION "1.20.3")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/RPM/libiscsi.spec.in")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/RPM/libiscsi.spec.in"
         _spec_version REGEX "^[ \t]*Version:[ \t]*[0-9]")
    list(GET _spec_version 0 _spec_version)
    string(REGEX REPLACE "^[ \t]*Version:[ \t]*([^ \t]+).*$" "\\1"
           _spec_version "${_spec_version}")
    if(_spec_version MATCHES "^[0-9]+(\\.[0-9]+)*$")
        set(LIBISCSI_VERSION "${_spec_version}")
    endif()
endif()

project(libiscsi VERSION ${LIBISCSI_VERSION} LANGUAGES C)

#
# Shared library version. Mirrors -version-info $(SOCURRENT):$(SOREVISON):$(SOAGE)
# in lib/Makefile.am, which libtool turns into libiscsi.so.11.0.2 with an
# soname of libiscsi.so.11.
#
set(LIBISCSI_SOCURRENT 11)
set(LIBISCSI_SOREVISION 2)
set(LIBISCSI_SOAGE 0)
math(EXPR LIBISCSI_SOVERSION "${LIBISCSI_SOCURRENT} - ${LIBISCSI_SOAGE}")
set(LIBISCSI_LIBVERSION
    "${LIBISCSI_SOVERSION}.${LIBISCSI_SOAGE}.${LIBISCSI_SOREVISION}")

include(GNUInstallDirs)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CMakePackageConfigHelpers)

#
# Options. These mirror the ./configure switches of the autotools build.
#
option(BUILD_SHARED_LIBS "Build libiscsi as a shared library" ON)
option(ENABLE_WERROR "Build with -Werror (as the autotools build does)" ON)
option(ENABLE_MANPAGES "Regenerate the manpages from the docbook sources" OFF)
option(ENABLE_TEST_TOOL "Build test-tool (to test a remote server)" ON)
option(ENABLE_TESTS "Build the test programs" ON)
option(ENABLE_EXAMPLES "Build the examples" ON)
option(ENABLE_ISER "Build iSER (RDMA) support when the headers are present" ON)

set(WITH_GNUTLS "auto" CACHE STRING "Use gnutls to compute MD5 (auto/yes/no)")
set_property(CACHE WITH_GNUTLS PROPERTY STRINGS auto yes no)
set(WITH_LIBGCRYPT "auto" CACHE STRING
    "Use libgcrypt to compute MD5 (auto/yes/no)")
set_property(CACHE WITH_LIBGCRYPT PROPERTY STRINGS auto yes no)

#
# Platform quirks, matching the "case $host in" block of configure.ac.
#
set(_enable_write_strings ON)
set(LIBISCSI_PLATFORM_LIBS "")

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(_enable_write_strings OFF)
    check_library_exists(socket socket "" HAVE_LIBSOCKET)
    check_library_exists(nsl gethostbyname "" HAVE_LIBNSL)
    if(NOT HAVE_LIBSOCKET OR NOT HAVE_LIBNSL)
        message(FATAL_ERROR "Can not find required library (socket/nsl)")
    endif()
    list(APPEND LIBISCSI_PLATFORM_LIBS socket nsl)
endif()

if(MSVC)
    # Do not warn about the standard C and BSD socket functions.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
                            _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

if(MINGW)
    # C99 printf formats such as %zu and %llu.
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

if(WIN32)
    # Not check_library_exists(): the winsock functions are __stdcall on
    # 32-bit Windows, which a link test against a made-up prototype misses.
    list(APPEND LIBISCSI_PLATFORM_LIBS ws2_32)
endif()

#
# Warning flags, matching WARN_CFLAGS in configure.ac.
#
set(LIBISCSI_WARN_FLAGS "")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    list(APPEND LIBISCSI_WARN_FLAGS
        -Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align
        -Wcast-qual -Wvla
        -Wno-unknown-warning-option -Wno-stringop-truncation
        -Wno-unused-parameter)
    if(ENABLE_WERROR)
        list(APPEND LIBISCSI_WARN_FLAGS -Werror)
    endif()
    if(_enable_write_strings)
        list(APPEND LIBISCSI_WARN_FLAGS -Wwrite-strings)
    endif()
endif()

#
# ---------------------------------------------------------------------------
# Feature tests
# ---------------------------------------------------------------------------
#
foreach(_hdr
        arpa/inet.h dispatch/dispatch.h dlfcn.h inttypes.h netinet/in.h
        netinet/tcp.h poll.h stdatomic.h stdint.h stdio.h stdlib.h strings.h
        string.h sys/select.h sys/socket.h sys/stat.h sys/time.h sys/types.h
        sys/uio.h unistd.h)
    string(TOUPPER "HAVE_${_hdr}" _var)
    string(REGEX REPLACE "[/.]" "_" _var "${_var}")
    check_include_file(${_hdr} ${_var})
endforeach()

if(HAVE_STDLIB_H AND HAVE_STDIO_H AND HAVE_STRING_H)
    set(STDC_HEADERS 1)
endif()

if(WIN32)
    set(CMAKE_REQUIRED_INCLUDES windows.h)
    check_include_file(winsock.h HAVE_WINSOCK_H)
    check_include_file(winsock2.h HAVE_WINSOCK2_H)
    check_include_file(ws2tcpip.h HAVE_WS2TCPIP_H)
    unset(CMAKE_REQUIRED_INCLUDES)
endif()

check_symbol_exists(tzset "time.h" HAVE_TZSET)

#
# Type of the fifth argument of select(). MinGW uses struct __ms_timeval.
#
set(SELECT_TIMEVAL_TYPE "void")
set(_select_defs "")
foreach(_h HAVE_SYS_SELECT_H HAVE_WINSOCK2_H HAVE_WS2TCPIP_H)
    if(${_h})
        list(APPEND _select_defs "-D${_h}=1")
    endif()
endforeach()
foreach(_type "struct timeval" "struct __ms_timeval")
    string(MAKE_C_IDENTIFIER "HAVE_SELECT_${_type}" _var)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_REQUIRED_FLAGS "-Werror")
    endif()
    set(CMAKE_REQUIRED_DEFINITIONS ${_select_defs})
    set(CMAKE_REQUIRED_LIBRARIES ${LIBISCSI_PLATFORM_LIBS})
    check_c_source_compiles("
#if defined(HAVE_WINSOCK2_H) && defined(HAVE_WS2TCPIP_H)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#include <stddef.h>
int main(void) { return select(0, NULL, NULL, NULL, (${_type} *)NULL); }
" ${_var})
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(${_var})
        set(SELECT_TIMEVAL_TYPE "${_type}")
        break()
    endif()
endforeach()
message(STATUS "Type of the fifth argument of select(): ${SELECT_TIMEVAL_TYPE}")

check_c_source_compiles("
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
int main(void) { struct sockaddr_in sock; sock.sin_len = sizeof(sock); return 0; }
" HAVE_SOCK_SIN_LEN)

check_c_source_compiles("
#include <stdint.h>
#include <sys/filio.h>
int main(void) { int foo = FIONREAD; return foo; }
" NEED_SYS_FILIO_H)

check_c_source_compiles("
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
int main(void) { struct sockaddr_in6 sock; return (int)sizeof(sock); }
" HAVE_SOCKADDR_IN6)

check_c_source_compiles("
#include <unistd.h>
#include <scsi/sg.h>
int main(void) { int sgio = SG_IO; return sgio; }
" HAVE_SG_IO)

check_struct_has_member("struct timezone" tz_dsttime "sys/time.h"
                        HAVE_TIMEZONE_TZ_DSTTIME)

#
# clock_gettime() lives in librt on older glibc.
#
check_symbol_exists(clock_gettime "time.h" HAVE_CLOCK_GETTIME)
if(NOT HAVE_CLOCK_GETTIME)
    check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME_IN_LIBRT)
    if(HAVE_CLOCK_GETTIME_IN_LIBRT)
        set(HAVE_CLOCK_GETTIME 1)
        list(APPEND LIBISCSI_PLATFORM_LIBS rt)
    endif()
endif()

#
# pthread / multithreading support.
#
# Windows always uses the native thread API in lib/multithreading.c, even
# with MinGW where winpthreads would also be available.
#
if(WIN32)
    set(HAVE_MULTITHREADING 1)
else()
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads)

    check_c_source_compiles("
#include <pthread.h>
int main(void) { pthread_t thread1, thread2; (void)thread1; (void)thread2; return 0; }
" HAVE_PTHREAD)

    if(HAVE_PTHREAD)
        set(HAVE_MULTITHREADING 1)
    endif()

    check_library_exists(pthread pthread_spin_lock "" HAVE_LIBPTHREAD)

    check_symbol_exists(pthread_threadid_np "pthread.h" HAVE_PTHREAD_THREADID_NP)
endif()

#
# gnutls / libgcrypt, used to compute the CHAP MD5 digest. If neither is
# available we fall back on the bundled md5.c.
#
# Prefer pkg-config, which also finds libraries installed outside the default
# search path (e.g. /usr/local), and fall back on a header and link test.
#
# LIBISCSI_PC_REQUIRES_PRIVATE and LIBISCSI_CRYPTO_LIBS_PRIVATE end up in
# libiscsi.pc, LIBISCSI_PC_GNUTLS/LIBISCSI_PC_GCRYPT in libiscsi-config.cmake.
#
find_package(PkgConfig QUIET)
set(LIBISCSI_CRYPTO_LIBS "")
set(LIBISCSI_PC_REQUIRES_PRIVATE "")
set(LIBISCSI_CRYPTO_LIBS_PRIVATE "")
set(LIBISCSI_PC_GNUTLS OFF)
set(LIBISCSI_PC_GCRYPT OFF)

if(NOT WITH_GNUTLS STREQUAL "no")
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBISCSI_GNUTLS QUIET IMPORTED_TARGET gnutls)
    endif()
    if(LIBISCSI_GNUTLS_FOUND)
        set(HAVE_LIBGNUTLS ON)
        set(LIBISCSI_PC_GNUTLS ON)
        list(APPEND LIBISCSI_CRYPTO_LIBS PkgConfig::LIBISCSI_GNUTLS)
        list(APPEND LIBISCSI_PC_REQUIRES_PRIVATE gnutls)
    else()
        check_include_file(gnutls/crypto.h HAVE_GNUTLS_CRYPTO_H)
        if(HAVE_GNUTLS_CRYPTO_H)
            check_library_exists(gnutls gnutls_hash_init "" HAVE_LIBGNUTLS)
        endif()
        if(HAVE_LIBGNUTLS)
            list(APPEND LIBISCSI_CRYPTO_LIBS gnutls)
            list(APPEND LIBISCSI_CRYPTO_LIBS_PRIVATE -lgnutls)
        endif()
    endif()
    if(WITH_GNUTLS STREQUAL "yes" AND NOT HAVE_LIBGNUTLS)
        message(FATAL_ERROR "gnutls not found")
    endif()
endif()
if(HAVE_LIBGNUTLS)
    set(WITH_LIBGCRYPT "no")
endif()

if(NOT WITH_LIBGCRYPT STREQUAL "no")
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBISCSI_GCRYPT QUIET IMPORTED_TARGET libgcrypt)
    endif()
    if(LIBISCSI_GCRYPT_FOUND)
        set(HAVE_LIBGCRYPT ON)
        set(LIBISCSI_PC_GCRYPT ON)
        list(APPEND LIBISCSI_CRYPTO_LIBS PkgConfig::LIBISCSI_GCRYPT)
        list(APPEND LIBISCSI_PC_REQUIRES_PRIVATE libgcrypt)
    else()
        # libgcrypt only ships a .pc file since 1.9.
        check_include_file(gcrypt.h HAVE_GCRYPT_H)
        if(HAVE_GCRYPT_H)
            check_library_exists(gcrypt gcry_control "" HAVE_LIBGCRYPT)
        endif()
        if(HAVE_LIBGCRYPT)
            list(APPEND LIBISCSI_CRYPTO_LIBS gcrypt)
            list(APPEND LIBISCSI_CRYPTO_LIBS_PRIVATE -lgcrypt)
        endif()
    endif()
    if(WITH_LIBGCRYPT STREQUAL "yes" AND NOT HAVE_LIBGCRYPT)
        message(FATAL_ERROR "libgcrypt not found")
    endif()
endif()

set(NEED_MD5 OFF)
if(NOT HAVE_LIBGNUTLS AND NOT HAVE_LIBGCRYPT)
    set(NEED_MD5 ON)
endif()

#
# iSER (RDMA) support.
#
if(ENABLE_ISER)
    set(CMAKE_REQUIRED_LIBRARIES ibverbs rdmacm)
    check_c_source_compiles("
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
int main(void) { int ibv = IBV_WC_SEND; return ibv; }
" HAVE_LINUX_ISER)
    unset(CMAKE_REQUIRED_LIBRARIES)

    if(HAVE_LINUX_ISER)
        check_c_source_compiles("
#include <rdma/rdma_cma.h>
int main(void) { return RDMA_OPTION_ID_ACK_TIMEOUT; }
" HAVE_RDMA_ACK_TIMEOUT)
    endif()
endif()

#
# CUnit, needed by test-tool.
#
set(CMAKE_REQUIRED_LIBRARIES cunit)
check_c_source_compiles("
#include <CUnit/CUnit.h>
int main(void) { return 0; }
" HAVE_CUNIT)
if(HAVE_CUNIT)
    check_struct_has_member("struct CU_SuiteInfo" pSetUpFunc
                            "CUnit/CUnit.h;CUnit/TestDB.h"
                            HAVE_CU_SUITEINFO_PSETUPFUNC)
endif()
unset(CMAKE_REQUIRED_LIBRARIES)

configure_file(cmake/config.h.cmake "${PROJECT_BINARY_DIR}/config.h")

#
# ---------------------------------------------------------------------------
# Helpers shared by all subdirectories
# ---------------------------------------------------------------------------
#

# Apply the include paths, the config.h define and the warning flags that
# every object in the tree is built with.
function(libiscsi_apply_build_settings target)
    target_include_directories(${target} PRIVATE
        "${PROJECT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/include")
    if(WIN32)
        target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/win32")
    endif()
    target_compile_definitions(${target} PRIVATE HAVE_CONFIG_H)
    if(LIBISCSI_WARN_FLAGS)
        target_compile_options(${target} PRIVATE ${LIBISCSI_WARN_FLAGS})
    endif()
endfunction()

# The utilities, the tests and most examples use getopt_long(), which MSVC
# does not have. Only the library and iscsiclient are built there.
check_include_file(getopt.h HAVE_GETOPT_H)
if(NOT HAVE_GETOPT_H)
    message(STATUS "No getopt.h: not building the utilities, test-tool "
                   "and tests, and only the iscsiclient example")
    set(ENABLE_TEST_TOOL OFF)
    set(ENABLE_TESTS OFF)
endif()

add_subdirectory(lib)
add_subdirectory(doc)
if(HAVE_GETOPT_H)
    add_subdirectory(utils)
endif()

if(ENABLE_TEST_TOOL)
    add_subdirectory(test-tool)
endif()
if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()
if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

#
# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
#
install(FILES include/iscsi.h include/scsi-lowlevel.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/iscsi")

# pkg-config file, generated from the same template the autotools build uses.
# Keep libdir/includedir relative to the prefix where we can, so that the
# installed .pc file stays relocatable just like the autotools one.
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
set(VERSION "${PROJECT_VERSION}")
set(LIBS_PRIVATE ${LIBISCSI_CRYPTO_LIBS_PRIVATE})
if(HAVE_LIBPTHREAD)
    list(APPEND LIBS_PRIVATE -lpthread)
endif()
if(HAVE_LINUX_ISER)
    list(APPEND LIBS_PRIVATE -libverbs -lrdmacm)
endif()
string(REPLACE ";" " " LIBS_PRIVATE "${LIBS_PRIVATE}")
string(REPLACE ";" " " PC_REQUIRES_PRIVATE "${LIBISCSI_PC_REQUIRES_PRIVATE}")
configure_file(libiscsi.pc.in "${PROJECT_BINARY_DIR}/libiscsi.pc" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/libiscsi.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# CMake package files, so that dependent projects can find_package(libiscsi).
set(LIBISCSI_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/libiscsi")
install(EXPORT libiscsi-targets
        FILE libiscsi-targets.cmake
        NAMESPACE libiscsi::
        DESTINATION "${LIBISCSI_CMAKE_DIR}")
configure_package_config_file(
    cmake/libiscsi-config.cmake.in
    "${PROJECT_BINARY_DIR}/libiscsi-config.cmake"
    INSTALL_DESTINATION "${LIBISCSI_CMAKE_DIR}")
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/libiscsi-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
        "${PROJECT_BINARY_DIR}/libiscsi-config.cmake"
        "${PROJECT_BINARY_DIR}/libiscsi-config-version.cmake"
        DESTINATION "${LIBISCSI_CMAKE_DIR}")

#
# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
#
message(STATUS "")
message(STATUS "libiscsi ${PROJECT_VERSION} configuration summary")
message(STATUS "  install prefix ....... ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  shared library ....... ${BUILD_SHARED_LIBS}")
message(STATUS "  -Werror .............. ${ENABLE_WERROR}")
message(STATUS "  multithreading ....... ${HAVE_MULTITHREADING}")
message(STATUS "  iSER support ......... ${HAVE_LINUX_ISER}")
if(HAVE_LIBGNUTLS)
    message(STATUS "  MD5 provider ......... gnutls")
elseif(HAVE_LIBGCRYPT)
    message(STATUS "  MD5 provider ......... libgcrypt")
else()
    message(STATUS "  MD5 provider ......... bundled md5.c")
endif()
message(STATUS "  build utilities ...... ${HAVE_GETOPT_H}")
message(STATUS "  build test-tool ...... ${ENABLE_TEST_TOOL} (cunit: ${HAVE_CUNIT})")
message(STATUS "  build tests .......... ${ENABLE_TESTS}")
message(STATUS "  build examples ....... ${ENABLE_EXAMPLES}")
message(STATUS "")
