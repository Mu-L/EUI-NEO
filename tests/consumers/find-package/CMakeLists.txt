cmake_minimum_required(VERSION 3.14)
project(EuiNeoFindPackageConsumer LANGUAGES CXX)

enable_testing()

find_package(EuiNeo CONFIG REQUIRED)
message(STATUS "EuiNeo shared build: ${EUI_NEO_BUILD_SHARED}")

add_executable(eui_find_package_consumer
    main.cpp
    public_header_companion.cpp
)
target_link_libraries(eui_find_package_consumer PRIVATE eui::neo)

if(EUI_NEO_ENABLE_MODULES)
    foreach(required_module IN ITEMS keyboard serial)
        if(NOT TARGET eui::module_${required_module})
            message(FATAL_ERROR "Installed package is missing eui::module_${required_module}")
        endif()
    endforeach()
    target_link_libraries(eui_find_package_consumer PRIVATE
        eui::module_keyboard
        eui::module_serial)
endif()

add_test(NAME eui_find_package_consumer COMMAND eui_find_package_consumer)

if(EUI_NEO_RENDER_BACKEND STREQUAL "vulkan")
    eui_compile_shadertoy(eui_find_package_consumer
        SOURCE "consumer.frag"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer.frag.spv")
endif()
