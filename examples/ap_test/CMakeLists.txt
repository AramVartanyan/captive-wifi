# captive-wifi 0.4.0 hardware test: the portal idle limit and the access point only mode.
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")   # the repository root is the component
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(captive_wifi_ap_test)
