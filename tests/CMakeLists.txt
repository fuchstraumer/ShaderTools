function(add_lodestone_unit_test NAME)
    add_executable(${NAME} ${ARGN} TestHarness.cpp TestHarness.hpp)

    set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
    set_target_properties(${NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

    # headers only: velox::math is header-only, and linking the library would drag in Dawn and GLFW.
    # include/math is on the path because TestHarness.cpp and MathTests.cpp both say "Math.hpp"
    # unqualified, the way the library's own sources do.
    target_include_directories(${NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/client/include")

    target_link_libraries(${NAME} PRIVATE lodestone slang lodestone::client_internal)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${NAME} PRIVATE "$<$<CONFIG:Debug>:-fcolor-diagnostics;-fansi-escape-codes;-fstandalone-debug;-fno-limit-debug-info;-fno-omit-frame-pointer;-fsanitize=address,undefined>")
        target_link_options(${NAME} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
    endif()
    add_test(NAME ${NAME} COMMAND ${NAME})
    copy_slang_dlls_to_target(${NAME})
endfunction()

add_lodestone_unit_test(CookTest CookTest.cpp)
add_lodestone_unit_test(SizeExpressionTest SizeExpressionTests.cpp)
