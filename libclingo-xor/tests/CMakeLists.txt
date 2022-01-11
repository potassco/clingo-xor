# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/catch.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/parsing.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/solving.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/util.cc")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

add_executable(test_clingo-xor ${source})
target_link_libraries(test_clingo-xor PRIVATE libclingo-xor-app_t)
target_include_directories(test_clingo-xor PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
set_target_properties(test_clingo-xor PROPERTIES FOLDER test)
if(MSVC)
    target_compile_definitions(test_clingo-xor PRIVATE _SCL_SECURE_NO_WARNINGS)
endif()

add_test(NAME test_clingo-xor COMMAND test_clingo-xor)
