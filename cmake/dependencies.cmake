include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(
    lzw20
    GIT_REPOSITORY https://github.com/yaito3014/lzw20
    GIT_TAG v0.2.0
    GIT_SHALLOW ON
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.15.0
    GIT_SHALLOW ON
)

macro(yk_gif20_provide_dependency method package_name)
  if("${package_name}" STREQUAL "lzw20")
    # gif20's install rules export yk::gif20, which links yk::lzw20, so lzw20's
    # export set must be in this build too
    set(YK_LZW20_INSTALL ON)
    FetchContent_MakeAvailable(lzw20)
    set(lzw20_FOUND TRUE)
  elseif("${package_name}" STREQUAL "Catch2")
    FetchContent_MakeAvailable(Catch2)
    set(Catch2_FOUND TRUE)
  endif()
endmacro()

cmake_language(
    SET_DEPENDENCY_PROVIDER yk_gif20_provide_dependency
    SUPPORTED_METHODS FIND_PACKAGE
)
