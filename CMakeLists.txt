cmake_minimum_required(VERSION 3.20)
project(jspp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler sources
set(JSPP_SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/typechecker/typechecker.cpp
    src/codegen/codegen.cpp
)

# Compiler headers (for IDE visibility)
set(JSPP_HEADERS
    src/lexer/token.hpp
    src/lexer/lexer.hpp
    src/parser/ast.hpp
    src/parser/parser.hpp
    src/typechecker/types.hpp
    src/typechecker/scope.hpp
    src/typechecker/typechecker.hpp
    src/codegen/codegen.hpp
    src/codegen/cpp_emitter.hpp
    src/common/error.hpp
)

add_executable(jspp ${JSPP_SOURCES} ${JSPP_HEADERS})
target_include_directories(jspp PRIVATE src)

add_executable(jspp-ship WIN32 ship/ship.cpp)
target_include_directories(jspp-ship SYSTEM PRIVATE third_party/sokol third_party/sokol/util)

add_custom_target(jspp-ship-selftest
    COMMAND $<TARGET_FILE:jspp-ship> --self-test --self-test-report ${CMAKE_BINARY_DIR}/jspp-ship-selftest.txt
    DEPENDS jspp-ship
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Warning flags
if(MSVC)
    target_compile_options(jspp PRIVATE /W4)
    target_compile_options(jspp-ship PRIVATE /W4)
else()
    target_compile_options(jspp PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(jspp-ship PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(jspp-ship PRIVATE opengl32 gdi32 user32 shell32 dxguid)
endif()

if(MINGW)
    target_link_options(jspp PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(jspp-ship PRIVATE -static-libgcc -static-libstdc++)

    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_WINPTHREAD_DLL "${MINGW_BIN_DIR}/libwinpthread-1.dll")
    if(EXISTS "${MINGW_WINPTHREAD_DLL}")
        foreach(target jspp jspp-ship)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MINGW_WINPTHREAD_DLL}"
                    $<TARGET_FILE_DIR:${target}>
                VERBATIM
            )
        endforeach()
        install(FILES "${MINGW_WINPTHREAD_DLL}" DESTINATION bin)
    endif()
endif()

# Install
install(TARGETS jspp jspp-ship DESTINATION bin)

# On MinGW, override default link libraries to avoid missing winspool etc.
if(MINGW)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-lkernel32 -luser32 -lshell32")
endif()
