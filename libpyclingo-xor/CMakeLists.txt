# [[[source: .
set(ide_source_group "Source Files")
set(source-group
    "${CMAKE_CURRENT_SOURCE_DIR}/_clingoxor.c")
source_group("${ide_source_group}" FILES ${source-group})
set(source
    ${source-group})
# ]]]

if (NOT PYCLINGOXOR_INSTALL_DIR AND Python_EXECUTABLE)
    if (PYCLINGOXOR_INSTALL STREQUAL "user")
        execute_process(COMMAND ${Python_EXECUTABLE} ${CLINGOXOR_SOURCE_DIR}/cmake/python-site.py site OUTPUT_VARIABLE PYCLINGOXOR_INSTALL_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    elseif(PYCLINGOXOR_INSTALL STREQUAL "prefix")
        execute_process(COMMAND ${Python_EXECUTABLE} ${CLINGOXOR_SOURCE_DIR}/cmake/python-site.py prefix "${CMAKE_INSTALL_PREFIX}" OUTPUT_VARIABLE PYCLINGOXOR_INSTALL_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        execute_process(COMMAND ${Python_EXECUTABLE} ${CLINGOXOR_SOURCE_DIR}/cmake/python-site.py prefix OUTPUT_VARIABLE PYCLINGOXOR_INSTALL_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()

if (NOT PYCLINGOXOR_SUFFIX)
    if (Python_EXECUTABLE)
        execute_process(COMMAND ${Python_EXECUTABLE} ${CLINGOXOR_SOURCE_DIR}/cmake/python-site.py suffix OUTPUT_VARIABLE PYCLINGOXOR_SUFFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        if (CYGWIN)
            set(PYCLINGOXOR_SUFFIX ".dll")
        elseif (UNIX)
            set(PYCLINGOXOR_SUFFIX ".so")
        else()
            set(PYCLINGOXOR_SUFFIX ".pyd")
        endif()
    endif()
endif()

# Note: For the pip package, we actually not just want to load this library but
# also link agaist it. This is mainly for Windows to ensure that the import
# library is installed.
if (PYCLINGOXOR_ENABLE STREQUAL "pip")
    add_library(pyclingo-xor SHARED ${source})
else()
    add_library(pyclingo-xor MODULE ${source})
endif()
target_link_libraries(pyclingo-xor PRIVATE libclingo-xor Python::Module)
target_compile_definitions(pyclingo-xor PRIVATE _CFFI_NO_LIMITED_API)
set_target_properties(pyclingo-xor PROPERTIES
    FOLDER lib
    OUTPUT_NAME _clingoxor
    SUFFIX ${PYCLINGOXOR_SUFFIX}
    PREFIX "")
if (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(pyclingo-xor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python)
endif()
if (CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set_target_properties(pyclingo-xor PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/python)
endif()
if (CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set_target_properties(pyclingo-xor PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/python)
endif()

if (PYCLINGOXOR_INSTALL_DIR)
    file(TO_CMAKE_PATH "${PYCLINGOXOR_INSTALL_DIR}" _PYCLINGOXOR_INSTALL_DIR)
    install(TARGETS pyclingo-xor
        RUNTIME DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor
        LIBRARY DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor
        ARCHIVE DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/libpyclingo-xor/clingoxor/
        DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor
        FILES_MATCHING
        PATTERN "*.py"
        PATTERN "*.typed"
        PATTERN "tests" EXCLUDE)
    if (NOT PYCLINGOXOR_ENABLE STREQUAL "pip")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/INSTALLER "pip\n")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/METADATA "Metadata-Version: 1.1\nName: clingo-xor\nVersion: ${CLINGOXOR_VERSION}\n")
        install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/INSTALLER
            ${CMAKE_CURRENT_BINARY_DIR}/METADATA
            DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor-${CLINGOXOR_VERSION}.dist-info)
    else()
        install(FILES ${CMAKE_SOURCE_DIR}/libclingo-xor/clingo-xor.h
            DESTINATION ${_PYCLINGOXOR_INSTALL_DIR}/clingoxor)
    endif()
endif()
