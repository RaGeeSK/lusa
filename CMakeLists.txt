cmake_minimum_required(VERSION 3.21)

project(LusaKey VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
endif()

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(LusaKey
    src/main.cpp
    src/appcontroller.cpp
    src/appcontroller.h
    src/vaultentry.cpp
    src/vaultentry.h
    src/vaultlistmodel.cpp
    src/vaultlistmodel.h
    src/vaultservice.cpp
    src/vaultservice.h
    src/crypto/cryptoservice.cpp
    src/crypto/cryptoservice.h
)

qt_add_resources(LusaKey resources
    PREFIX "/"
    FILES
        qml/Main.qml
)

target_link_libraries(LusaKey
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

if(WIN32)
    target_link_libraries(LusaKey PRIVATE bcrypt)
else()
    target_link_libraries(LusaKey PRIVATE OpenSSL::Crypto)
endif()

set_target_properties(LusaKey PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS LusaKey
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_finalize_executable(LusaKey)
