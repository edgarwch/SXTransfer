add_library(ftp "")
if(WIN32)
    add_definitions("-DCURL_STATICLIB")
endif()
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
target_sources(ftp
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ftpclient.cpp
  
    ${CMAKE_CURRENT_LIST_DIR}/ftpclient.h
  )
target_include_directories(ftp
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
  )