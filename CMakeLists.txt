cmake_minimum_required(VERSION 3.11)

project(libdns VERSION 0.1)

if (NOT OPENSSL_FOUND)
  find_package(OpenSSL REQUIRED)
endif()
if (OPENSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIR} ${OPENSSL_SSL_LIBRARIES}")
endif()

if (NOT NLOHMANN_JSON_FOUND)
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  add_subdirectory(lib/json)
endif()

option(WITH_TESTS "make with tests" ON)
option(WITH_STATIC "link statically" OFF)
option(WITH_IPV6 "use IPv6 server address" OFF)

configure_file(config.h.in include/${PROJECT_NAME}/config.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIBRARIES ${OPENSSL_SSL_LIBRARIES} nlohmann_json::nlohmann_json)
SET(INCLUDES include ${PROJECT_BINARY_DIR}/include PRIVATE ${OPENSSL_INCLUDE_DIR})
SET(SOURCES client.cc)

if (WITH_STATIC) # check: readelf -d build/tdscript
  add_library(${PROJECT_NAME} STATIC ${SOURCES})
  set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set(LIBRARIES ${LIBRARIES} -static)
else()
  add_library(${PROJECT_NAME} ${SOURCES} test/query.cc)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})

add_executable(${PROJECT_NAME}_example ${SOURCES} example/main.cc)
target_include_directories(${PROJECT_NAME}_example PRIVATE ${INCLUDES})
target_link_libraries(${PROJECT_NAME}_example PRIVATE ${LIBRARIES})

if (WITH_TESTS)
  enable_testing()

  function(add_ctest test_file)
    get_filename_component(test_target_name_dir ${test_file} DIRECTORY)
    get_filename_component(test_target_name_fn ${test_file} NAME_WE)
    string(REGEX REPLACE "[\\/]" "_" test_target_name
            "${PROJECT_NAME}_${test_target_name_dir}_${test_target_name_fn}")

    add_executable(${test_target_name} ${SOURCES} ${test_file})
    target_link_libraries(${test_target_name} ${LIBRARIES})
    target_include_directories(${test_target_name} PRIVATE ${INCLUDES})

    add_test(NAME ${test_target_name} COMMAND ${test_target_name})
  endfunction(add_ctest)

  add_ctest(test/query.cc)
endif (WITH_TESTS)
