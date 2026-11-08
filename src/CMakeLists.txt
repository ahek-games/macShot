cmake_minimum_required(VERSION 3.16)
project(macshot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Network DBus)

add_executable(macshot main.cpp)
target_link_libraries(macshot PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    Qt6::DBus
)

# 1. Install executable to /usr/bin/macshot
install(TARGETS macshot
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. Install desktop launcher to /usr/share/applications/macshot.desktop
install(FILES macshot.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# 3. Configure Debian (.deb) Package Generation via CPack
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "macshot")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "developer@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "macOS-style screenshot and annotation utility for Ubuntu")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Automatically detect required Qt6 shared libraries + desktop portal packages
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-desktop-portal, xdg-desktop-portal-gnome | xdg-desktop-portal-gtk, gsettings-desktop-schemas")

include(CPack)