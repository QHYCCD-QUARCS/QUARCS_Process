cmake_minimum_required(VERSION 3.5)

project(websocketclient)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_VERSION_REQ "5.7")
find_package(Qt5 COMPONENTS Core WebSockets REQUIRED)

set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/websocketclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quarcsmonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/led.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
 
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/websocketclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/quarcsmonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/led.h
)

set(CMAKE_AUTOMOC ON)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::WebSockets)

# 将控制脚本复制到编译目录中
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LedControl.sh ${CMAKE_CURRENT_BINARY_DIR}/LedControl.sh COPYONLY)
