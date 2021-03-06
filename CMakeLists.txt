cmake_minimum_required(VERSION 3.4)

project(GOOFIT
    VERSION 2.1.3
    LANGUAGES CXX)

set(GOOFIT_TAG "dev")
#set(GOOFIT_TAG "alpha")
#set(GOOFIT_TAG "beta")
#set(GOOFIT_TAG "release")

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# Allow IDE's to group targets into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Get the git command
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GOOFIT_SUBMODULE "Check submodules during build" ON)
    if(GOOFIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE GOOFIT_GIT_VERSION
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

else()
    set(GOOFIT_GIT_VERSION "unknown")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/cmake/FindThrust.cmake"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/CLI11/CMakeLists.txt"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/Eigen/CMakeLists.txt"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/cuda_support/README.md"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/FeatureDetector/CMakeLists.txt"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/MCBooster/CMakeLists.txt"
OR NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/thrust/README.md")
    message(FATAL_ERROR "The submodules were not downloaded! GOOFIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Add support for better CUDA behaviour in CMake < 3.9
if(CMAKE_VERSION VERSION_LESS 3.9)
    set(CMAKE_MODULE_PATH  "${PROJECT_SOURCE_DIR}/extern/cuda_support" ${CMAKE_MODULE_PATH})
endif()

# Add clang-tidy if available
if(CMAKE_VERSION VERSION_GREATER 3.6)
    option(GOOFIT_TIDY_FIX "Perform fixes for Clang-Tidy - changes source inplace" OFF)
    find_program(
        CLANG_TIDY_EXE
        NAMES "clang-tidy"
        DOC "Path to clang-tidy executable"
    )

    if(CLANG_TIDY_EXE)
        if(GOOFIT_TIDY_FIX)
            set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-fix")
        else()
            set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}")
        endif()
    endif()
endif()

# Add Sanatizers
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/extern/sanitizers/cmake" ${CMAKE_MODULE_PATH})
find_package(Sanitizers)

set(GOOFIT_MAXPAR "1800" CACHE STRING "The number of parameters to statically support, can be increased but should not be too large.")

set(GOOFIT_CUDA_OR_GROUPSIZE "128" CACHE STRING "Overrides the default group distribution for Thrust's transform_reduce")
set(GOOFIT_CUDA_OR_GRAINSIZE "7" CACHE STRING "Overrides the default grain size for Thrust's transform_reduce")

configure_file (
    "${PROJECT_SOURCE_DIR}/include/goofit/detail/ThrustOverrideConfig.h.in"
    "${PROJECT_BINARY_DIR}/include/goofit/detail/ThrustOverrideConfig.h"
)

### C++ settings ###
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unknown-pragmas -Wno-long-long -Wno-attributes -Wno-sign-compare -Wno-unused-parameter")
    # Helpful but irritating: -Wzero-as-null-pointer-constant -Wsuggest-override
    # no-sign-compare can be removed, but will take some work to clean up
    # Same is true for no-unused-parameter
    # if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Printout time taken to compile
option(GOOFIT_TIME_COMPILE "Print time to compile during compilation" OFF)
if(GOOFIT_TIME_COMPILE)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "time")
endif()

# Code coverage
if(CMAKE_BUILD_TYPE STREQUAL Coverage)
    include(CodeCoverage)
    setup_target_for_coverage(GooFit_coverage ctest coverage)
endif()

### Options ###
set(DEVICE_LISTING CUDA OMP CPP TBB Auto)
set(HOST_LISTING OMP CPP TBB Auto)
mark_as_advanced(DEVICE_LISTING HOST_LISTING)

set(GOOFIT_DEVICE Auto CACHE STRING "The compute device, options are ${DEVICE_LISTING}")
set(GOOFIT_HOST Auto CACHE STRING "The compute device, options are ${HOST_LISTING}")

# Requires CMake 3.3 or greater
if(NOT ${GOOFIT_DEVICE} IN_LIST DEVICE_LISTING)
    message(FATAL_ERROR "You must select a device from ${DEVICE_LISTING}, not ${GOOFIT_DEVICE}")
endif()
if(NOT ${GOOFIT_HOST} IN_LIST HOST_LISTING)
    message(FATAL_ERROR "You must select a host from ${HOST_LISTING}, not ${HOST_DEVICE}")
endif()

include(PatchOpenMPApple)

# Add CCache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_PROGRAM}") # CMake 3.9+
endif()

if(CMAKE_VERSION VERSION_LESS 3.8)
    set(NEW_CUDA OFF)
else()
    set(NEW_CUDA ON)
endif()

# Auto device selection
if(GOOFIT_DEVICE STREQUAL Auto)
    if(NEW_CUDA)
        include(CheckLanguage)
        check_language(CUDA)
    else()
        find_package(CUDA 7.0)
    endif()
    if(CUDA_FOUND OR CMAKE_CUDA_COMPILER)
        set(GOOFIT_DEVICE CUDA)
    else()
        find_package(OpenMP QUIET)
        if(OpenMP_CXX_FOUND OR OpenMP_FOUND)
            set(GOOFIT_DEVICE OMP)
        else()
            set(GOOFIT_DEVICE CPP)
        endif()
    endif()
    message(STATUS "Auto device selection: ${GOOFIT_DEVICE}")
endif()

# Auto host selection based on device
if(GOOFIT_HOST STREQUAL Auto)
    if(GOOFIT_DEVICE STREQUAL OMP)
        set(GOOFIT_HOST OMP)
    elseif(GOOFIT_DEVICE STREQUAL TBB)
        set(GOOFIT_HOST TBB)
    else()
        set(GOOFIT_HOST CPP)
    endif()
endif()

# Checks for invalid combinations
if(${GOOFIT_DEVICE} STREQUAL TBB AND ${GOOFIT_HOST} STREQUAL OMP)
    message(FATAL_ERROR "You must set TBB as both host and device (OMP will still be required)")
endif()
if(${GOOFIT_DEVICE} STREQUAL OMP AND ${GOOFIT_HOST} STREQUAL TBB)
    message(FATAL_ERROR "TBB cannot be a host backend for OMP")
endif()
if(${GOOFIT_DEVICE} STREQUAL CUDA AND ${GOOFIT_HOST} STREQUAL OMP)
    message(FATAL_ERROR "OMP cannot be a host backend for CUDA")
endif()

option(GOOFIT_SEPARATE_COMP "Enable separate compilation of PDFs" ON)

# This is the common library that holds definitions, etc.
add_library(GooFit_Common INTERFACE)
add_library(GooFit::Common ALIAS GooFit_Common)

target_include_directories(GooFit_Common INTERFACE include)
target_include_directories(GooFit_Common INTERFACE "${PROJECT_BINARY_DIR}/include")

if(GOOFIT_SEPARATE_COMP)
    target_compile_definitions(GooFit_Common INTERFACE "-DSEPARABLE")
endif()

target_compile_definitions(GooFit_Common INTERFACE "-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_${GOOFIT_DEVICE}")
target_compile_definitions(GooFit_Common INTERFACE "-DMCBOOSTER_BACKEND=${GOOFIT_DEVICE}")
target_compile_definitions(GooFit_Common INTERFACE "-DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_${GOOFIT_HOST}")
# target_compile_definitions(GooFit_Common INTERFACE _USE_MATH_DEFINES)

option(GOOFIT_DEBUG "Print debugging messages" OFF)
option(GOOFIT_TRACE "Print messages to trace the behavior of GooFit" OFF)
if(GOOFIT_DEBUG)
    target_compile_definitions(GooFit_Common INTERFACE "-DGOOFIT_DEBUG_FLAG=1")
endif()
if(GOOFIT_TRACE)
    target_compile_definitions(GooFit_Common INTERFACE "-DGOOFIT_TRACE_FLAG=1")
endif()

# Adding backtrace (optional)
# Some systems need execinfo explicitly linked
# Standard CMake module
find_package(Backtrace)
add_library(backtrace INTERFACE)
if(Backtrace_FOUND)
    # Assuming no extra flags
    target_include_directories(backtrace INTERFACE ${Backtrace_INCLUDE_DIR})
    target_link_libraries(backtrace INTERFACE ${Backtrace_LIBRARIES})
endif()
configure_file(
    "${PROJECT_SOURCE_DIR}/include/goofit/detail/Backtrace.h.in"
    "${PROJECT_BINARY_DIR}/include/goofit/detail/Backtrace.h"
)


set(GOOFIT_ARCH Auto CACHE STRING "The GPU Archetecture, can be Auto, All, Common, a number, or a name")


option(GOOFIT_MPI "Turn on MPI for goofit" OFF)


if(GOOFIT_MPI)
    find_package(MPI REQUIRED)

    # Added in CMake 3.9
    if(NOT TARGET MPI::MPI_CXX)
        add_library(MPI_MPI_CXX INTERFACE)
        add_library(MPI::MPI_CXX ALIAS MPI_MPI_CXX)
        target_compile_definitions(MPI::MPI_CXX INTERFACE "-DGOOFIT_MPI")
        target_add_compile_options(MPI::MPI_CXX INTERFACE ${MPI_CXX_COMPILE_FLAGS})
        target_include_directories(MPI::MPI_CXX INTERFACE ${MPI_CXX_INCLUDE_PATH})
        list(APPEND CMAKE_EXE_LINKER_FLAGS ${MPI_CXX_LINK_FLAGS})
        target_link_libraries(MPI::MPI_CXX INTERFACE ${MPI_CXX_LIBRARIES})
    endif()

    target_link_libraries(GooFit_Common INTERFACE MPI::MPL_CXX)

    message(STATUS "MPI found. Use the following to run your program")
    message(STATUS "${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} PROCS ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")
    message(STATUS "where PROCS is the number of processors on which to execute the program, EXECUTABLE is the MPI program, and ARGS are the arguments to pass to the MPI program.")
endif()

if(GOOFIT_DEVICE STREQUAL CUDA)
    # The old FindCUDA method
    if(NOT NEW_CUDA)
        if(NOT CUDA_FOUND)
            find_package(CUDA 7.0 REQUIRED)
        endif()
        message(STATUS "Found CUDA ${CUDA_VERSION_STRING} at ${CUDA_TOOLKIT_ROOT_DIR}")

        if(NOT CUDA_VERSION VERSION_LESS 9.0)
            message(WARNING "You have a version of CUDA that is currently not supported by Eigen, build will fail")
        endif()

        if(GOOFIT_MPI)
            list(APPEND CUDA_NVCC_FLAGS ${MPI_CXX_COMPILE_FLAGS})
        endif()

        list(APPEND CUDA_NVCC_FLAGS -std=c++11 --expt-relaxed-constexpr)

        if(GOOFIT_SEPARATE_COMP)
            set(CUDA_SEPARABLE_COMPILATION ON)
        else()
            set(CUDA_SEPARABLE_COMPILATION OFF)
        endif()

        # Only needed to make FindCUDA provide nice flags
        set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)

        cuda_select_nvcc_arch_flags(ARCH_FLAGS ${GOOFIT_ARCH})
        list(APPEND CUDA_NVCC_FLAGS ${ARCH_FLAGS})
        message(STATUS "Compiling for GPU arch: ${ARCH_FLAGS}")

        set(THRUST_INCLUDE_DIR "${CUDA_TOOLKIT_ROOT_DIR}/include")

    # The new language support
    else()
        enable_language(CUDA)

        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 7.0)
            message(FATAL_ERROR "CUDA 7.0 or better required!")
        elseif(NOT CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 9.0)
            message(WARNING "You have a version of CUDA that is currently not supported by Eigen, build will fail")
        else()
            message(STATUS "Found CUDA ${CMAKE_CUDA_COMPILER_VERSION} at ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}")
        endif()


        set(CUDA_TOOLKIT_ROOT_DIR "${CMAKE_CUDA_COMPILER}")
        get_filename_component(CUDA_TOOLKIT_ROOT_DIR "${CUDA_TOOLKIT_ROOT_DIR}" DIRECTORY)
        get_filename_component(CUDA_TOOLKIT_ROOT_DIR "${CUDA_TOOLKIT_ROOT_DIR}" DIRECTORY)
        find_package(CUDA REQUIRED)
        include(CUDA)

        set(cuda_lang "$<COMPILE_LANGUAGE:CUDA>")

        set(CMAKE_CUDA_STANDARD 11)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        set(CMAKE_CUDA_EXTENSIONS OFF)

        string(APPEND CMAKE_CUDA_FLAGS " --expt-relaxed-constexpr")

        cuda_detect_arch(${GOOFIT_ARCH} NVCC_FLAGS ARCH_FLAGS)
        string(APPEND CMAKE_CUDA_FLAGS " ${ARCH_FLAGS}")
        message(STATUS "Compiling for GPU arch: ${ARCH_FLAGS}")

        set(THRUST_INCLUDE_DIR "${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}")
    endif()
endif()

if(NOT DEFINED cuda_lang)
    set(cuda_lang 0)
endif()

# Config helpers
set(rel_mode "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>")
set(cxx_lang "$<COMPILE_LANGUAGE:CXX>")
set(cxx_lang_rel "$<AND:${cxx_lang},${rel_mode}>")
set(cuda_lang_rel "$<AND:${cuda_lang},${rel_mode}>")

set(GOOFIT_OPTI "-march=native" CACHE STRING "compiler flags for optimized builds")
string(REPLACE ";" "," GOOFIT_OPTI_CUDA "${GOOFIT_OPTI}")
target_compile_options(GooFit_Common INTERFACE
    $<${cxx_lang_rel}:$<BUILD_INTERFACE:${GOOFIT_OPTI}>>
    $<${cuda_lang_rel}:$<BUILD_INTERFACE:-Xcompiler=${GOOFIT_OPTI_CUDA}>>
    )

find_package(Thrust 1.8 QUIET)
if(NOT THRUST_FOUND)
    set(THRUST_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/extern/thrust")
    find_package(Thrust 1.8 REQUIRED)
endif()
target_include_directories(GooFit_Common SYSTEM INTERFACE "${THRUST_INCLUDE_DIRS}")
message(STATUS "Found Thrust ${THRUST_VERSION} at ${THRUST_INCLUDE_DIRS}")

if(GOOFIT_DEVICE STREQUAL OMP OR GOOFIT_HOST STREQUAL OMP OR GOOFIT_DEVICE STREQUAL TBB OR GOOFIT_HOST STREQUAL TBB)
    find_package(OpenMP REQUIRED)
    if(NOT TARGET OpenMP::OpenMP_CXX)
        add_library(OpenMP_TARGET INTERFACE)
        add_library(OpenMP::OpenMP_CXX ALIAS OpenMP_TARGET)
        target_compile_options(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
        find_package(Threads REQUIRED)
        target_link_libraries(OpenMP_TARGET INTERFACE Threads::Threads)
        target_link_libraries(OpenMP_TARGET INTERFACE ${OpenMP_CXX_FLAGS})
    endif()

    target_link_libraries(GooFit_Common INTERFACE OpenMP::OpenMP_CXX)
endif()

if(GOOFIT_DEVICE STREQUAL TBB OR GOOFIT_HOST STREQUAL TBB)
    find_package(TBB COMPONENTS tbbmalloc tbbmalloc_proxy tbb_preview)
    target_include_directories(goofit_mt SYSTEM INTERFACE "${TBB_INCLUDE_DIRS}")
    target_link_libraries(GooFit_Common INTERFACE ${TBB_LIBRARIES})
endif()


# Include directories are not picked up by FindCUDA
option(GOOFIT_CERNROOT "Look for the ROOT library (turned off for Python)" ON)
if(GOOFIT_CERNROOT)
    find_package(ROOT 6 COMPONENTS Minuit)
    if(ROOT_FOUND)
        message(STATUS "Found ROOT ${ROOT_VERSION} at ${ROOT_INCLUDE_DIRS}")
        if(ROOT_VERSION STREQUAL "6.10/00" )
            message(WARNING "You have ROOT Version ${ROOT_VERSION}, which is known to have issues compiling with GooFit!")
        endif()
        target_compile_definitions(GooFit_Common INTERFACE "-DROOT_FOUND")
        target_link_libraries(GooFit_Common INTERFACE ROOT::ROOT ROOT::Minuit)
    endif()
endif()
mark_as_advanced(GOOFIT_CERNROOT)

# If either ROOT not found or Minuit2 not included
if(NOT ROOT_Minuit2_LIBRARY)
    add_subdirectory("extern/Minuit2")
    set_target_properties(Minuit2 PROPERTIES FOLDER extern)
    set_target_properties(Math PROPERTIES FOLDER extern)
    add_library(ROOT::Minuit2 ALIAS Minuit2)
endif()
target_link_libraries(GooFit_Common INTERFACE ROOT::Minuit2)

# Adding simple libraries
add_subdirectory("extern/CLI11")
mark_as_advanced(CLI_CXX_STD CLI_EXAMPLES CLI_SINGLE_FILE CLI_SINGLE_FILE_TESTS CLI_TESTING)
target_link_libraries(GooFit_Common INTERFACE CLI11::CLI11)

add_subdirectory("extern/FeatureDetector")
set_target_properties(FeatureDetector PROPERTIES FOLDER extern)
target_link_libraries(GooFit_Common INTERFACE FeatureDetector)

target_compile_options(FeatureDetector PRIVATE
    $<BUILD_INTERFACE:${GOOFIT_OPTI}>
    )

## Format
add_subdirectory("extern/fmt")
set_target_properties(fmt PROPERTIES FOLDER extern)
mark_as_advanced(FMT_CPPFORMAT FMT_DOC FMT_INSTALL FMT_PEDANTIC FMT_TEST FMT_USE_CPP11)
target_link_libraries(GooFit_Common INTERFACE fmt)


add_library(rang INTERFACE)
target_include_directories(rang SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/extern/rang/include")
target_link_libraries(GooFit_Common INTERFACE rang)


add_library(Eigen INTERFACE)
target_include_directories(Eigen SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/extern/Eigen")
target_link_libraries(GooFit_Common INTERFACE Eigen)

# This if(ROUND_FOUND) wrapper in theory should not be needed
# But for some reason FALSE is not "falsy" to the define command
if(ROOT_FOUND)
    set(GOOFIT_ROOT_FOUND ROOT_FOUND)
endif()
set(GOOFIT_ARCH_FLAGS ARCH_FLAGS)

option(GOOFIT_SPLASH "Show a unicode splash or a small plain text one" ON)

# Output the current GooFit version
configure_file (
    "${PROJECT_SOURCE_DIR}/include/goofit/Version.h.in"
    "${PROJECT_BINARY_DIR}/include/goofit/Version.h"
)

if(CMAKE_VERSION VERSION_LESS 3.9)
    message(STATUS "CMake <3.9 will only enable IPO for Intel compilers on Linux")
    set(SUPPORTS_IPO TRUE)
else()
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT SUPPORTS_IPO)
    message(STATUS "Compiler supports IPO: ${SUPPORTS_IPO}")
endif()

function(goofit_add_library GNAME)
    set(options STATIC SHARED MODULE)
    set(oneValueArgs "")
    set(multiValueArgs "")
    cmake_parse_arguments(
        GOOFIT_LIB
        "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
        )
    if(GOOFIT_LIB_SHARED)
        set(GOOFIT_LIB_TYPE SHARED)
    elseif(GOOFIT_LIB_MODULE)
        set(GOOFIT_LIB_TYPE MODULE)
    else()
        set(GOOFIT_LIB_TYPE STATIC)
    endif()

    if(GOOFIT_DEVICE STREQUAL CUDA)
        if(NOT NEW_CUDA)
            foreach(N ${GOOFIT_LIB_UNPARSED_ARGUMENTS})
                get_filename_component(NEXT ${N} EXT)
                if(NEXT STREQUAL ".cc" OR NEXT STREQUAL ".cpp" OR NEXT STREQUAL ".cxx")
                    set_source_files_properties(${N} PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ)
                endif()
            endforeach()

            cuda_add_library(${GNAME} ${GOOFIT_LIB_TYPE} ${GOOFIT_LIB_UNPARSED_ARGUMENTS} OPTIONS "-Xcompiler=-fPIC")
        else()
            add_library(${GNAME} ${GOOFIT_LIB_TYPE} ${GOOFIT_LIB_UNPARSED_ARGUMENTS})
            set_target_properties(${GNAME} PROPERTIES
                CUDA_SEPARABLE_COMPILATION ON)
        endif()
    else()
        foreach(N ${GOOFIT_LIB_UNPARSED_ARGUMENTS})
            get_filename_component(NEXT ${N} EXT)
            if(NEXT STREQUAL ".cu")
                set_source_files_properties(${N} PROPERTIES LANGUAGE CXX)
            endif()
        endforeach()
        add_library(${GNAME} ${GOOFIT_LIB_TYPE} ${GOOFIT_LIB_UNPARSED_ARGUMENTS})
        add_sanitizers(${GNAME})
        target_compile_options(${GNAME} PUBLIC -x c++)
    endif()
    target_link_libraries(${GNAME} PUBLIC GooFit::Common)
    if(NOT ${GNAME} STREQUAL PDFs)
        set_target_properties(${GNAME} PROPERTIES FOLDER core)
    endif()

    set_target_properties(${GNAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${SUPPORTS_IPO})

    source_group("Source Files" REGULAR_EXPRESSION ".*\\.c[ucp]p?")
    source_group("Basic\\Header Files" REGULAR_EXPRESSION ".*PDFs/basic/.*\\.h")
    source_group("Basic\\Source Files" REGULAR_EXPRESSION ".*PDFs/basic/.*\\.c[ucp]p?")
    source_group("Combine\\Header Files" REGULAR_EXPRESSION ".*PDFs/combine/.*\\.h")
    source_group("Combine\\Source Files" REGULAR_EXPRESSION ".*PDFs/combine/.*\\.c[ucp]p?")
    source_group("Physics\\Header Files" REGULAR_EXPRESSION ".*PDFs/physics/.*\\.h")
    source_group("Physics\\Source Files" REGULAR_EXPRESSION ".*PDFs/physics/.*\\.c[ucp]p?")

    if(CLANG_TIDY_EXE)
        set_target_properties(
            ${GNAME} PROPERTIES
            CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
        )
    endif()
endfunction()

function(goofit_add_executable NAMEEXE)
    if(GOOFIT_DEVICE STREQUAL CUDA)
        if(NOT NEW_CUDA)
            set(CUDA_SEPARABLE_COMPILATION OFF)
            cuda_add_executable(${NAMEEXE} ${ARGN})
            set(CUDA_SEPARABLE_COMPILATION ON)
        else()
            add_executable(${NAMEEXE} ${ARGN})
        endif()
    else()
        foreach(N ${ARGN})
            get_filename_component(NEXT ${N} EXT)
            if(NEXT STREQUAL ".cu")
                set_source_files_properties(${N} PROPERTIES LANGUAGE CXX)
            endif()
        endforeach()
        add_executable(${NAMEEXE} ${ARGN})
        add_sanitizers(${NAMEEXE})
        target_compile_options(${NAMEEXE} PUBLIC -x c++)
    endif()
    target_link_libraries(${NAMEEXE} PUBLIC GooFit::GooFit)
    set_target_properties(${NAMEEXE} PROPERTIES FOLDER projects)
    source_group("Source Files" REGULAR_EXPRESSION ".*\\.c[uc]")

    set_target_properties(${GNAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${SUPPORTS_IPO})

    if(CLANG_TIDY_EXE)
        set_target_properties(
            ${GNAME} PROPERTIES
            CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
        )
    endif()
endfunction()

function(goofit_add_link)
    if(MSVC) # Not officially supported, but needed to even configure on Windows
        set(COMM copy)
    else()
        set(COMM create_symlink)
    endif()

    foreach(NAMELINK ${ARGN})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NAMELINK}")

            execute_process(
                COMMAND ${CMAKE_COMMAND} -E ${COMM}
                "${CMAKE_CURRENT_SOURCE_DIR}/${NAMELINK}"
                "${CMAKE_CURRENT_BINARY_DIR}/${NAMELINK}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                )

        else()
            message(WARNING "${NAMELINK} does not exist. Not making link.")
        endif()
    endforeach()
endfunction()


function(goofit_add_directory)
    file(GLOB directory_listing RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
    set(skip_files CMakeLists.txt CMakeFiles Makefile makefile .gitignore .git)

    foreach(NAMELINK ${directory_listing})
        if(NOT ${NAMELINK} IN_LIST skip_files)
            goofit_add_link(${NAMELINK})
        endif()
    endforeach()
endfunction()


# Adding global property to collect extra info for compilation
set_property(GLOBAL PROPERTY GOOFIT_EXTRA_PDFS "")
macro(goofit_add_pdf)
    foreach(N ${ARGN})
        get_filename_component(N ${N} ABSOLUTE)
        message(STATUS "Adding PDF: ${N}")
        get_property(goo_extra GLOBAL PROPERTY GOOFIT_EXTRA_PDFS)
        set_property(GLOBAL PROPERTY GOOFIT_EXTRA_PDFS ${goo_extra} ${N})
    endforeach()
endmacro()

target_include_directories(GooFit_Common INTERFACE extern/MCBooster)
target_include_directories(GooFit_Common INTERFACE extern/generics)

add_subdirectory(src)

add_library(goofit_lib INTERFACE)
target_link_libraries(goofit_lib INTERFACE goofit_base PDFs GooFit::Common)
add_library(GooFit::GooFit ALIAS goofit_lib)


option(GOOFIT_EXAMPLES "Build the example programs" ON)
if(GOOFIT_EXAMPLES)
    add_subdirectory(examples)
endif()

if(EXISTS work)
    add_subdirectory(work)
endif()

option(GOOFIT_PACKAGES "Build any goofit_* packages found" ON)
if(GOOFIT_PACKAGES)
    file(GLOB list_of_packages RELATIVE ${PROJECT_SOURCE_DIR} goofit_*)
    foreach(d ${list_of_packages})
        add_subdirectory(${d})
    endforeach()
endif()

# This allows GOOFIT_PYTHON to be automatic
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/extern/pybind11/tools" ${CMAKE_MODULE_PATH})
find_package(PythonLibsNew)

if(PYTHONLIBS_FOUND)
    if(EXISTS ${PYTHON_LIBRARIES})
        option(GOOFIT_PYTHON "Python bindings for goofit" ON)
    else()
        message(STATUS "Found Python, but library location ${PYTHON_LIBRARIES} does not exist")
        option(GOOFIT_PYTHON "Python bindings for goofit" OFF)
    endif()
else()
    message(STATUS "Did not find the development files for Python, turning off automatic Python bindings")
    option(GOOFIT_PYTHON "Python bindings for goofit" OFF)
endif()

if(GOOFIT_PYTHON)
    add_subdirectory("extern/pybind11")
    if(PYTHONLIBS_FOUND AND EXISTS ${PYTHON_LIBRARIES})
        message(STATUS "Found Python at ${PYTHON_LIBRARIES}, building bindings")
    else()
        message(FATAL_ERROR "Did not find Python development libraries, make sure the development package for Python is installed or turn off Python bindings")
    endif()
    if(NEW_CUDA AND GOOFIT_DEVICE STREQUAL CUDA)
        cuda_convert_flags(module)
    endif()
    add_subdirectory(python)
endif()

option(GOOFIT_TESTS "Build the goofit tests" ON)
if(GOOFIT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
