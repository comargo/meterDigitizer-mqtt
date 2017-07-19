cmake_minimum_required(VERSION 3.5)
project(meterDigitizer-mqtt)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(LibMosquitto REQUIRED)
find_package(jsoncpp REQUIRED)
add_subdirectory(tinytemplate)


add_executable(${PROJECT_NAME}
    main.cpp
    helper.cpp
    string_split_join.hpp)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_lambdas cxx_override cxx_range_for cxx_generalized_initializers)
target_link_libraries (${PROJECT_NAME} Threads::Threads mosquitto tinytemplate jsoncpp_lib)

#Add libconfig++
pkg_check_modules(LIBCONFIGXX REQUIRED libconfig++)
target_link_libraries(${PROJECT_NAME} ${LIBCONFIGXX_LIBRARY_DIRS} ${LIBCONFIGXX_LIBRARIES})
target_include_directories(${PROJECT_NAME}  PRIVATE ${LIBCONFIGXX_INCLUDE_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE ${LIBCONFIGXX_CFLAGS_OTHER})

#Install
configure_file(meterDigitizer-mqtt@.service.in ${CMAKE_CURRENT_BINARY_DIR}/meterDigitizer-mqtt@.service)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR} COMPONENT bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/meterDigitizer-mqtt@.service DESTINATION /lib/systemd/system/)
