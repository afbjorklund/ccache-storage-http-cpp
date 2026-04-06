cmake_minimum_required(VERSION 3.16)
project(ccache-storage-redis-cpp VERSION 0.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(hiredis QUIET CONFIG)
if(NOT hiredis_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
endif()
find_package(hiredis_ssl QUIET CONFIG)
if(NOT hiredis_ssl_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(hiredis_ssl OPTIONAL IMPORTED_TARGET hiredis_ssl)
endif()

find_package(libuv QUIET CONFIG)
if(NOT libuv_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBUV REQUIRED IMPORTED_TARGET libuv)
endif()

configure_file(src/version.hpp.in version.hpp)

set(
  SOURCES
  src/config.cpp
  src/ipc_server.cpp
  src/logger.cpp
  src/main.cpp
  src/storage_client.cpp
)

add_executable(ccache-storage-redis ${SOURCES})
target_include_directories(ccache-storage-redis PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(TARGET hiredis::hiredis)
  target_link_libraries(ccache-storage-redis PRIVATE hiredis::hiredis)
else()
  target_link_libraries(ccache-storage-redis PRIVATE PkgConfig::hiredis)
endif()
if(TARGET hiredis::hiredis_ssl)
  target_compile_definitions(ccache-storage-redis PRIVATE HAVE_HIREDIS_SSL)
  target_link_libraries(ccache-storage-redis PRIVATE hiredis::hiredis_ssl)
elseif(hiredis_ssl_FOUND)
  target_compile_definitions(ccache-storage-redis PRIVATE HAVE_HIREDIS_SSL)
  target_link_libraries(ccache-storage-redis PRIVATE PkgConfig::hiredis_ssl)
endif()

if(TARGET libuv::uv_a)
  target_link_libraries(ccache-storage-redis PRIVATE libuv::uv_a)
elseif(TARGET libuv::libuv)
  target_link_libraries(ccache-storage-redis PRIVATE libuv::libuv)
else()
  target_link_libraries(ccache-storage-redis PRIVATE PkgConfig::LIBUV)
endif()

if(WIN32)
  target_compile_definitions(ccache-storage-redis PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOGDI NOMINMAX)
  if(MSVC)
    target_compile_options(ccache-storage-redis PRIVATE /W2)
  else()
    target_compile_options(ccache-storage-redis PRIVATE -Wall -Wextra)
  endif()
else()
  target_compile_options(ccache-storage-redis PRIVATE -Wall -Wextra)
endif()

install(TARGETS ccache-storage-redis RUNTIME DESTINATION bin)
install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink ccache-storage-redis \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/ccache-storage-redis+unix)")
if(hiredis_ssl_FOUND)
  install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink ccache-storage-redis \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/ccache-storage-rediss)")
endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES src/*.cpp src/*.hpp)
  add_custom_target(clang-format COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES} COMMENT "Running clang-format on all source files" VERBATIM)
endif()
