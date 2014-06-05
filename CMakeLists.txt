cmake_minimum_required(VERSION 2.8)

project(negotiation_test)

find_package(PkgConfig)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.2)

add_executable(negotiation_test main.c)

set_property(TARGET negotiation_test
  APPEND_STRING PROPERTY COMPILE_FLAGS " -Wall -Werror "
)
target_include_directories (negotiation_test PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(negotiation_test ${GSTREAMER_LIBRARIES})
