cmake_minimum_required(VERSION 3.11)

project(libdns VERSION 0.1)

option(WITH_TESTS "make with tests" ON)
option(WITH_STATIC "link statically" OFF)
option(WITH_IPV6 "use IPv6 server address" OFF)

if (WITH_STATIC)
  message(STATUS "${PROJECT_NAME} WITH STATIC")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

if (NOT LIBRESSL_ROOT_DIR)
  set(LIBRESSL_ROOT_DIR ${PROJECT_BINARY_DIR}/lib/ssl/lib)
endif()

add_subdirectory(lib)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
find_package(LibreSSL REQUIRED)

configure_file(config.h.in include/${PROJECT_NAME}/config.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIBRARIES Threads::Threads LibreSSL::SSL)
set(INCLUDES include ${PROJECT_BINARY_DIR}/include)
set(SOURCES client.cc)

if (WITH_STATIC) # check: readelf -d build/tdscript
  add_library(${PROJECT_NAME} STATIC ${SOURCES})
  set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set(LIBRARIES ${LIBRARIES} -static)
else()
  add_library(${PROJECT_NAME} ${SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})

add_executable(${PROJECT_NAME}_example ${SOURCES} example/main.cc)
target_include_directories(${PROJECT_NAME}_example PRIVATE ${INCLUDES})
target_link_libraries(${PROJECT_NAME}_example PRIVATE ${LIBRARIES})

if (WITH_TESTS)
  enable_testing()

  set(test_name "${PROJECT_NAME}_test_1_epoll")
  add_executable(${test_name} test/1_epoll.cc)
  add_test(NAME ${test_name} COMMAND ${test_name})

  set(test_name "${PROJECT_NAME}_test_2_ssl")
  add_executable(${test_name} test/2_ssl.cc)
  target_link_libraries(${test_name} PRIVATE ${LIBRARIES})
  add_test(NAME ${test_name} COMMAND ${test_name})

  function(add_ctest test_file)
    get_filename_component(test_name_fn ${test_file} NAME_WE)
    set(test_name "${PROJECT_NAME}_test_${test_name_fn}")

    add_executable(${test_name} ${SOURCES} ${test_file})
    target_link_libraries(${test_name} PRIVATE ${LIBRARIES})
    target_include_directories(${test_name} PRIVATE ${INCLUDES})

    add_test(NAME ${test_name} COMMAND ${test_name})
  endfunction(add_ctest)

  add_ctest(test/3_client.cc)
  add_ctest(test/4_query.cc)
  add_ctest(test/5_https_request.cc)
  add_ctest(test/6_request_binary_file.cc)
endif (WITH_TESTS)
