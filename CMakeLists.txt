cmake_minimum_required(VERSION 3.14)

# ESP-IDF component support (must be before project() for script mode compatibility)
if(ESP_PLATFORM)
    idf_component_register(INCLUDE_DIRS "include")
    # Disabling the Kconfig option turns std::format support off. Kconfig emits
    # nothing for a disabled option, so this is passed explicitly, and
    # consumers need not include sdkconfig.h before quanta headers. Enabled,
    # nothing is defined, so the header's detection of <format> decides: the
    # toolchain of ESP-IDF 5.1 (GCC 12) has none. The option has its own name
    # so that sdkconfig.h, which defines it, never sets the header's
    # CONFIG_QUANTA_STD_FORMAT.
    if(NOT CONFIG_QUANTA_STD_FORMAT_SUPPORT)
        target_compile_definitions(${COMPONENT_LIB} INTERFACE CONFIG_QUANTA_STD_FORMAT=0)
    endif()
    return()
endif()

project(quanta
    VERSION 0.1.0
    DESCRIPTION "Type-safe units for temperature, frequency, electrical and spatial quantities, modeled after std::chrono"
    HOMEPAGE_URL "https://github.com/cleishm/quanta-cpp"
    LANGUAGES CXX
)

# PROJECT_IS_TOP_LEVEL / <project>_IS_TOP_LEVEL need CMake 3.21; compute it by hand
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(QUANTA_IS_TOP_LEVEL ON)
else()
    set(QUANTA_IS_TOP_LEVEL OFF)
endif()

option(QUANTA_BUILD_TESTS "Build tests" ${QUANTA_IS_TOP_LEVEL})

set(CMAKE_CXX_EXTENSIONS OFF)

# Main library target (header-only)
add_library(quanta INTERFACE)
add_library(quanta::quanta ALIAS quanta)

# The headers contain UTF-8 unit symbols; MSVC needs /utf-8 to read them correctly.
target_compile_options(quanta INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

target_include_directories(quanta INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(quanta INTERFACE cxx_std_20)

# Formatting (only when this is the top-level project)
if(QUANTA_IS_TOP_LEVEL)
    find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)

    if(CLANG_FORMAT_EXECUTABLE)
        # Get all source files
        file(GLOB_RECURSE ALL_SOURCE_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
        )

        # Target to check formatting
        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${ALL_SOURCE_FILES}
            COMMENT "Checking code formatting"
            VERBATIM
        )

        # Target to fix formatting
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
            COMMENT "Applying code formatting"
            VERBATIM
        )

        message(STATUS "clang-format found: format targets available")
    else()
        message(WARNING "clang-format not found: format targets unavailable")
    endif()
endif()

# Tests
if(QUANTA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
option(QUANTA_BUILD_DOCS "Build documentation" OFF)

if(QUANTA_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DOXYGEN_OUTPUT_DIRECTORY}
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        message(STATUS "Doxygen found: documentation can be built with 'make docs'")
    else()
        message(WARNING "Doxygen not found: documentation cannot be built")
    endif()
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS quanta
    EXPORT quantaTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT quantaTargets
    FILE quantaTargets.cmake
    NAMESPACE quanta::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quanta
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/quantaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/quantaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quanta
)

# Under semver, 0.x minor releases may break the API
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(QUANTA_VERSION_COMPATIBILITY SameMinorVersion)
else()
    set(QUANTA_VERSION_COMPATIBILITY SameMajorVersion)
endif()

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/quantaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${QUANTA_VERSION_COMPATIBILITY}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/quantaConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/quantaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quanta
)
