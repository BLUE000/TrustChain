project(TrustChainDummyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

add_executable(dummy_app WIN32 main.cpp)

target_include_directories(dummy_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(dummy_app PRIVATE
    trustchain
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)
