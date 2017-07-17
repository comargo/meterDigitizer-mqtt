# - Try to find libmosquitto and libmosquittopp
# Once done, this will define
#
#  LibMosquitto++_FOUND - system has libmosquittopp
#  LibMosquitto++_INCLUDE_DIRS - the libmosquittopp include directories
#  LibMosquitto++_LIBRARIES - link these to use libmosquittopp
#
# And adds library
#  mosquittopp

include(LibFindMacros)

find_path(LibMosquitto++_INCLUDE_DIR NAMES mosquittopp.h)
find_library(LibMosquitto++_LIBRARY mosquittopp)

set(LibMosquitto++_PROCESS_INCLUDES LibMosquitto++_INCLUDE_DIR)
set(LibMosquitto++_PROCESS_LIBS LibMosquitto++_LIBRARY)
libfind_process(LibMosquitto++)

if(LibMosquitto++_FOUND)
        add_library(mosquittopp SHARED IMPORTED GLOBAL)
        set_target_properties(mosquittopp PROPERTIES
                IMPORTED_LOCATION ${LibMosquitto++_LIBRARIES}
                INTERFACE_INCLUDE_DIRECTORIES ${LibMosquitto++_INCLUDE_DIRS}
                )
endif()
