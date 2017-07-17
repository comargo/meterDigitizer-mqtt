# - Try to find libmosquitto and libmosquittopp
# Once done, this will define
#
#  LibMosquitto_FOUND - system has libmosquitto
#  LibMosquitto_INCLUDE_DIRS - the libmosquitto include directories
#  LibMosquitto_LIBRARIES - link these to use libmosquitto
#
# And adds library
#  mosquitto
include(LibFindMacros)

find_path(LibMosquitto_INCLUDE_DIR NAMES mosquitto.h)
find_library(LibMosquitto_LIBRARY mosquitto)
set(LibMosquitto_PROCESS_INCLUDES LibMosquitto_INCLUDE_DIR)
set(LibMosquitto_PROCESS_LIBS LibMosquitto_LIBRARY)
libfind_process(LibMosquitto)

if(LibMosquitto_FOUND)
        add_library(mosquitto SHARED IMPORTED GLOBAL)
        set_target_properties(mosquitto PROPERTIES
                IMPORTED_LOCATION ${LibMosquitto_LIBRARIES}
                INTERFACE_INCLUDE_DIRECTORIES ${LibMosquitto_INCLUDE_DIRS}
                )
endif()
