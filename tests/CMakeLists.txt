include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

include(CTest)
include(Catch)

# One test executable per domain, so test case names only need to be unique
# within a domain.
function(quanta_add_tests domain)
    add_executable(${domain}_tests ${ARGN})
    target_link_libraries(${domain}_tests PRIVATE quanta::quanta Catch2::Catch2WithMain)
    target_compile_options(${domain}_tests PRIVATE "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wshadow>")
    catch_discover_tests(${domain}_tests TEST_PREFIX "${domain}.")
endfunction()

quanta_add_tests(thermo thermo/thermo_test.cpp)
quanta_add_tests(frequency frequency/frequency_test.cpp)
quanta_add_tests(electro electro/electro_test.cpp electro/decibel_test.cpp)
quanta_add_tests(spatial spatial/spatial_test.cpp)

# The formatters must also build with exceptions disabled, as on embedded
# targets. Catch2 requires exceptions, so these are standalone targets. MSVC
# spells the option differently and still declares __cpp_exceptions, so the
# check is limited to compilers taking -fno-exceptions.
if(NOT MSVC)
    foreach(domain thermo frequency spatial)
        add_executable(${domain}_no_exceptions_test ${domain}/no_exceptions_test.cpp)
        target_link_libraries(${domain}_no_exceptions_test PRIVATE quanta::quanta)
        target_compile_options(${domain}_no_exceptions_test PRIVATE -fno-exceptions -Wall -Wextra -Wpedantic -Wshadow)
        add_test(NAME ${domain}.no_exceptions COMMAND ${domain}_no_exceptions_test)
    endforeach()
endif()

# All domains together in one translation unit.
quanta_add_tests(combined combined_test.cpp)
