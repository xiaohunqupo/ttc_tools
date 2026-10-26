cmake_minimum_required(VERSION 3.16)

project(ttc_tools
	VERSION 0.3.0
	DESCRIPTION "Split TrueType Collections (.ttc) into standalone fonts"
	LANGUAGES CXX)

# The library is header only: there is nothing to compile, generate or link, so
# the only build products are the sample and the test.
add_library(ttc_tools INTERFACE)
add_library(ttc_tools::ttc_tools ALIAS ttc_tools)
target_include_directories(ttc_tools INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_compile_features(ttc_tools INTERFACE cxx_std_11)

option(TTC_TOOLS_BUILD_SAMPLE "Build the command line front end" ON)
option(TTC_TOOLS_BUILD_TESTS "Build the regression test" ON)

# Warnings are the same for every target we own.
function(ttc_tools_warnings target)
	if (MSVC)
		target_compile_options(${target} PRIVATE /W4)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endfunction()

if (TTC_TOOLS_BUILD_SAMPLE)
	add_executable(ttc_tools_cli Project/ttc_tools.cpp)
	target_link_libraries(ttc_tools_cli PRIVATE ttc_tools)
	set_target_properties(ttc_tools_cli PROPERTIES OUTPUT_NAME ttc_tools)
	ttc_tools_warnings(ttc_tools_cli)
endif()

if (TTC_TOOLS_BUILD_TESTS)
	enable_testing()
	add_executable(ttc_tests test/test_ttc.cpp)
	target_link_libraries(ttc_tests PRIVATE ttc_tools)
	ttc_tools_warnings(ttc_tests)

	# Run from the repository root so that fonts/*.ttc is picked up when present.
	add_test(NAME ttc_tests COMMAND ttc_tests
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Install rules only matter when this repository is consumed as a subproject.
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS ttc_tools EXPORT ttc_toolsTargets)
install(EXPORT ttc_toolsTargets
	NAMESPACE ttc_tools::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ttc_tools)
