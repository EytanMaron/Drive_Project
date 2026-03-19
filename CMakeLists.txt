cmake_minimum_required(VERSION 3.14)
project(my_project)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Project Sources ---
file(GLOB_RECURSE ALL_SRC_FILES "src/*.cpp")
file(GLOB_RECURSE TEST_FILES "tests/*.cpp")

# --- Define Include Directories ---
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Commands
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Menu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Server
)



# --- Target 1: The Client Executable ---

set(CLIENT_SOURCES
    src/Client/ClientMain.cpp
    src/Client/TcpClient.cpp
    src/Menu/TcpClientShell.cpp
)

add_executable(client ${CLIENT_SOURCES})

target_include_directories(client PRIVATE 
    src/Client 
    src/Menu
)


# --- Target 2: The Server Executable ---

set(SERVER_SOURCES ${ALL_SRC_FILES})
list(FILTER SERVER_SOURCES EXCLUDE REGEX "src/Client/.*")
list(FILTER SERVER_SOURCES EXCLUDE REGEX "src/Menu/TcpClientShell.cpp")

add_executable(myApp ${SERVER_SOURCES})
find_package(Threads REQUIRED)
target_link_libraries(myApp Threads::Threads)
target_include_directories(myApp PRIVATE ${PROJECT_INCLUDE_DIRS})



# --- Target 3: The Tests Executable ---

set(TEST_SOURCES_NO_MAINS ${ALL_SRC_FILES})

list(FILTER TEST_SOURCES_NO_MAINS EXCLUDE REGEX "src/Server/ServerMain.cpp") 
list(FILTER TEST_SOURCES_NO_MAINS EXCLUDE REGEX "src/Client/ClientMain.cpp")

add_executable(myTests ${TEST_FILES} ${TEST_SOURCES_NO_MAINS})
target_link_libraries(myTests gtest_main gmock Threads::Threads)

target_include_directories(myTests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    ${googletest_SOURCE_DIR}/googletest/include
    ${googletest_SOURCE_DIR}/googlemock/include
)

include(GoogleTest)
gtest_discover_tests(myTests)
