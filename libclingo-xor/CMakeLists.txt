# [[[header: .
set(ide_header_group "Header Files")
set(header-group
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo-xor.h")
source_group("${ide_header_group}" FILES ${header-group})
set(header
    ${header-group})
# ]]]
# [[[source: src
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/src/clingo-xor.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parsing.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parsing.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/problem.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/problem.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/solving.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/solving.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util.hh")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

if (PYCLINGOXOR_ENABLE STREQUAL "pip")
    set(clingoxor_build_interface ON)
else()
    set(clingoxor_build_interface OFF)
endif()

set(clingoxor_public_scope_ PUBLIC)
set(clingoxor_private_scope_ PRIVATE)
if (CLINGOXOR_BUILD_SHARED)
    set(clingoxor_lib_type SHARED)
    if (clingoxor_build_interface)
        set(clingoxor_lib_type INTERFACE)
        set(clingoxor_public_scope_ INTERFACE)
        set(clingoxor_private_scope_ INTERFACE)
    endif()
else()
    set(clingoxor_lib_type STATIC)
endif()

add_library(libclingo-xor ${clingoxor_lib_type})
target_sources(libclingo-xor ${clingoxor_private_scope_} ${header} ${source})
target_include_directories(libclingo-xor ${clingoxor_public_scope_} "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_definitions(libclingo-xor ${clingoxor_private_scope_} CLINGOXOR_BUILD_LIBRARY)
if (CLINGOXOR_PROFILE) 
    target_compile_definitions(libclingo-xor ${clingoxor_public_scope_} "CLINGOXOR_PROFILE")
    target_link_libraries(libclingo-xor ${clingoxor_public_scope_} Gperftools::Gperftools)
endif()
if (CLINGOXOR_CROSSCHECK)
    target_compile_definitions(libclingo-xor ${clingoxor_public_scope_} "CLINGOXOR_CROSSCHECK")
endif()

if (NOT CLINGOXOR_BUILD_STATIC AND CLINGOXOR_BUILD_SHARED AND NOT clingoxor_build_interface)
    set_target_properties(libclingo-xor PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_link_libraries(libclingo-xor ${clingoxor_public_scope_} libclingo)
if (NOT CLINGOXOR_BUILD_SHARED)
    target_compile_definitions(libclingo-xor ${clingoxor_public_scope_}  CLINGOXOR_NO_VISIBILITY)
elseif(NOT clingoxor_build_interface)
    set_target_properties(libclingo-xor PROPERTIES PUBLIC_HEADER clingo-xor.h)
    install(TARGETS libclingo-xor
        EXPORT ClingoXORTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT ClingoXORTargets
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClingoXOR)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/ClingoXORConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoXORConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClingoXOR)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoXORConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoXORConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ClingoXORConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClingoXOR)
endif()
if (NOT clingoxor_build_interface)
    set_target_properties(libclingo-xor PROPERTIES
        OUTPUT_NAME clingo-xor
        LIBRARY_OUTPUT_NAME clingo-xor
        FOLDER lib)
endif()

if (CLINGOXOR_BUILD_TESTS)
    # NOTE: we build statically without hiding symbols
    add_library(libclingo-xor_t STATIC ${header} ${source})
    target_include_directories(libclingo-xor_t PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
    target_compile_definitions(libclingo-xor_t PUBLIC CLINGOXOR_NO_VISIBILITY)
    target_link_libraries(libclingo-xor_t PUBLIC libclingo)
    target_compile_definitions(libclingo-xor_t PUBLIC "CLINGOXOR_CROSSCHECK")
    set_target_properties(libclingo-xor_t PROPERTIES
        OUTPUT_NAME clingo-xor_t
        LIBRARY_OUTPUT_NAME clingo-xor_t
        FOLDER lib)
    add_subdirectory(tests)
endif()
