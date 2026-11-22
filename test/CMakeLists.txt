find_package(Catch2 3 CONFIG REQUIRED)

option(YK_GIF20_CI "for CI" OFF)

set(CMAKE_COLOR_DIAGNOSTICS ${YK_GIF20_CI})

function(yk_gif20_add_test test_name)
  set(target_name yk_gif20_${test_name}_test)
  add_executable(${target_name} ${test_name}.cpp)
  target_compile_features(${target_name} PRIVATE cxx_std_20)
  target_link_libraries(${target_name} PRIVATE yk::gif20 Catch2::Catch2WithMain)
  add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# Every header compiles on its own: one translation unit per header, including it twice
file(
    GLOB_RECURSE YK_GIF20_PARTS
    RELATIVE ${PROJECT_SOURCE_DIR}/include
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.hpp
)
set(YK_GIF20_PART_SOURCES)
foreach(part IN LISTS YK_GIF20_PARTS)
  string(MAKE_C_IDENTIFIER ${part} part_name)
  set(source ${CMAKE_CURRENT_BINARY_DIR}/self_contained/${part_name}.cpp)
  file(CONFIGURE OUTPUT ${source} CONTENT "#include <${part}>\n#include <${part}>\n")
  list(APPEND YK_GIF20_PART_SOURCES ${source})
endforeach()
if(YK_GIF20_PART_SOURCES)
  add_library(yk_gif20_self_contained OBJECT ${YK_GIF20_PART_SOURCES})
  target_link_libraries(yk_gif20_self_contained PRIVATE yk::gif20)
endif()

yk_gif20_add_test(decode)
yk_gif20_add_test(encode)
yk_gif20_add_test(compile_time)
