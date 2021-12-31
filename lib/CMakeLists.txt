cmake_minimum_required(VERSION 3.11)

project(libdns_lib VERSION 0.1)

if (NOT EXISTS "${LIBRESSL_ROOT_DIR}/libssl.a")
  set(LIBRESSL_ROOT_DIR "${PROJECT_BINARY_DIR}/ssl/lib" PARENT_SCOPE)

  file(COPY ${PROJECT_SOURCE_DIR}/ssl DESTINATION ${PROJECT_BINARY_DIR})
  file(RENAME ${PROJECT_BINARY_DIR}/ssl ${PROJECT_BINARY_DIR}/ssl-build)
  execute_process(
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/ssl-build"
    COMMAND "./configure" "--prefix=${PROJECT_BINARY_DIR}/ssl"
    RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "ssl cmake configure error")
  endif()

  execute_process(
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/ssl-build"
    COMMAND "make"
    RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "ssl make error")
  endif()

  execute_process(
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/ssl-build"
    COMMAND "make" "install"
    RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "ssl install error")
  endif()
endif()