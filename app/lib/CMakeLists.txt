# [[[header: clingo-xor-app
set(ide_header_group "Header Files")
set(header-group-clingo-xor-app
    "${CMAKE_CURRENT_SOURCE_DIR}/clingo-xor-app/app.hh")
source_group("${ide_header_group}\\clingo-xor-app" FILES ${header-group-clingo-xor-app})
set(header
    ${header-group-clingo-xor-app})
# ]]]
# [[[source: src
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_library(libclingo-xor-app STATIC)
target_sources(libclingo-xor-app PRIVATE ${header} ${source})
target_include_directories(libclingo-xor-app PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

if (NOT CLINGOXOR_BUILD_STATIC AND CLINGOXOR_BUILD_SHARED AND NOT clingoxor_build_interface)
    set_target_properties(libclingo-xor-app PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_link_libraries(libclingo-xor-app PUBLIC libclingo-xor)

set_target_properties(libclingo-xor-app PROPERTIES
    OUTPUT_NAME clingo-xor-app
    LIBRARY_OUTPUT_NAME clingo-xor-app
    FOLDER lib)

if (CLINGOXOR_BUILD_TESTS)
    add_library(libclingo-xor-app_t STATIC ${header} ${source})
    target_include_directories(libclingo-xor-app_t PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
    target_link_libraries(libclingo-xor-app_t PUBLIC libclingo-xor_t)
    set_target_properties(libclingo-xor-app_t PROPERTIES
        OUTPUT_NAME clingo-xor-app_t
        LIBRARY_OUTPUT_NAME clingo-xor-app_t
        FOLDER lib)
endif()
