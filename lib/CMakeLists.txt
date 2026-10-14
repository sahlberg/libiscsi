#
# libiscsi itself.
#
# As in the autotools build there are two libraries:
#   iscsipriv - a static convenience library with every symbol visible,
#               used by test-tool which pokes at private symbols
#   iscsi     - the installed library, which exports only the symbols
#               listed in libiscsi.syms.in
#
set(ISCSI_SOURCES
    connect.c
    crc32c.c
    discovery.c
    init.c
    login.c
    nop.c
    pdu.c
    iscsi-command.c
    multithreading.c
    scsi-lowlevel.c
    socket.c
    sync.c
    task_mgmt.c
    logging.c
    utils.c
    sha1.c
    sha224-256.c
    sha3.c)

if(WIN32)
    list(APPEND ISCSI_SOURCES "${PROJECT_SOURCE_DIR}/win32/win32_compat.c")
endif()

if(NEED_MD5)
    list(APPEND ISCSI_SOURCES md5.c)
endif()

if(HAVE_LINUX_ISER)
    list(APPEND ISCSI_SOURCES iser.c)
endif()

set(ISCSI_LINK_LIBS ${LIBISCSI_CRYPTO_LIBS} ${LIBISCSI_PLATFORM_LIBS})
if(HAVE_LIBPTHREAD AND CMAKE_THREAD_LIBS_INIT)
    list(APPEND ISCSI_LINK_LIBS Threads::Threads)
endif()
if(HAVE_LINUX_ISER)
    list(APPEND ISCSI_LINK_LIBS ibverbs rdmacm)
endif()

#
# Compile the sources once and share the objects between the two libraries.
# PIC is forced on so the objects can go into the shared library as well.
#
add_library(iscsi_objects OBJECT ${ISCSI_SOURCES})
set_target_properties(iscsi_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
libiscsi_apply_build_settings(iscsi_objects)

#
# The convenience library. Not installed, exactly like noinst_LTLIBRARIES.
#
add_library(iscsipriv STATIC $<TARGET_OBJECTS:iscsi_objects>)
libiscsi_apply_build_settings(iscsipriv)
target_include_directories(iscsipriv PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(iscsipriv PUBLIC ${ISCSI_LINK_LIBS})

#
# The installed library.
#
add_library(iscsi $<TARGET_OBJECTS:iscsi_objects>)
add_library(libiscsi::iscsi ALIAS iscsi)
libiscsi_apply_build_settings(iscsi)
target_link_libraries(iscsi PRIVATE ${ISCSI_LINK_LIBS})
target_include_directories(iscsi PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(iscsi PROPERTIES
    OUTPUT_NAME iscsi
    VERSION "${LIBISCSI_LIBVERSION}"
    SOVERSION "${LIBISCSI_SOVERSION}")

#
# Restrict the exported symbols to the ones listed in libiscsi.syms.in, the
# same way that libtool's -export-symbols does for the autotools build.
#
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/libiscsi.syms.in" _syms_text)
if(WIN32)
    string(APPEND _syms_text "win32_poll\n")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libiscsi.syms" "${_syms_text}")

string(REGEX REPLACE "[\r\n]+" ";" _syms_list "${_syms_text}")
list(REMOVE_ITEM _syms_list "")

if(BUILD_SHARED_LIBS AND WIN32)
    # Some prototypes are marked __declspec(dllexport), which turns off
    # MinGW's export of everything else, so list the exports in a .def file.
    set(_export_file "${CMAKE_CURRENT_BINARY_DIR}/libiscsi.def")
    set(_export_text "EXPORTS\n")
    foreach(_sym IN LISTS _syms_list)
        string(APPEND _export_text "\t${_sym}\n")
    endforeach()
    file(WRITE "${_export_file}" "${_export_text}")
    target_sources(iscsi PRIVATE "${_export_file}")
elseif(BUILD_SHARED_LIBS)
    if(APPLE)
        set(_export_file "${CMAKE_CURRENT_BINARY_DIR}/libiscsi.exported_symbols")
        set(_export_text "")
        foreach(_sym IN LISTS _syms_list)
            string(APPEND _export_text "_${_sym}\n")
        endforeach()
        file(WRITE "${_export_file}" "${_export_text}")
        target_link_options(iscsi PRIVATE
            "-Wl,-exported_symbols_list,${_export_file}")
        set_property(TARGET iscsi APPEND PROPERTY LINK_DEPENDS "${_export_file}")
    else()
        # Check that the linker understands version scripts before using one.
        set(_probe "${CMAKE_CURRENT_BINARY_DIR}/version-script-probe.map")
        file(WRITE "${_probe}" "{ global: main; local: *; };\n")
        set(CMAKE_REQUIRED_FLAGS "-Wl,--version-script,${_probe}")
        check_c_source_compiles("int main(void) { return 0; }"
                                HAVE_LD_VERSION_SCRIPT)
        unset(CMAKE_REQUIRED_FLAGS)

        if(HAVE_LD_VERSION_SCRIPT)
            set(_export_file "${CMAKE_CURRENT_BINARY_DIR}/libiscsi.map")
            set(_export_text "{\nglobal:\n")
            foreach(_sym IN LISTS _syms_list)
                string(APPEND _export_text "\t${_sym};\n")
            endforeach()
            string(APPEND _export_text "local: *;\n};\n")
            file(WRITE "${_export_file}" "${_export_text}")
            target_link_options(iscsi PRIVATE
                "-Wl,--version-script,${_export_file}")
            set_property(TARGET iscsi APPEND PROPERTY
                         LINK_DEPENDS "${_export_file}")
        else()
            message(WARNING
                "The linker does not support --version-script, so libiscsi "
                "will export all of its symbols.")
        endif()
    endif()
endif()

install(TARGETS iscsi
        EXPORT libiscsi-targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
