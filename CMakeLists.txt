cmake_minimum_required(VERSION 3.0)
project(sauklaue)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Warnings
if (MSVC)
	add_compile_options(/W4 /WX)
else()
	add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-deprecated-declarations)
endif()

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)
link_libraries(Qt5::Widgets)
#find_package(Qt5X11Extras CONFIG REQUIRED)
#link_libraries(Qt5::X11Extras)

include (FindPkgConfig)

# Find the cairomm library
pkg_check_modules(cairomm REQUIRED "cairomm-1.0")
include_directories(${cairomm_INCLUDE_DIRS})
link_libraries(${cairomm_LDFLAGS})

# Find the Xlib library
pkg_search_module(Xlib REQUIRED x11)
include_directories(${Xlib_INCLUDE_DIRS})
link_libraries(${Xlib_LDFLAGS})
pkg_search_module(Xinput REQUIRED xi)
include_directories(${Xinput_INCLUDE_DIRS})
link_libraries(${Xinput_LDFLAGS})

# Compile the Cap'n Proto file
find_package(CapnProto)
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS src/file4.capnp)
set_property(SOURCE ${CAPNP_HDRS} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${CAPNP_SRCS} PROPERTY SKIP_AUTOGEN ON)
link_libraries(CapnProto::capnp)

# Find the Poppler library
pkg_search_module(PopplerGlib REQUIRED poppler-glib)
include_directories(${PopplerGlib_INCLUDE_DIRS})
link_libraries(${PopplerGlib_LDFLAGS})

# Find the KConfig library
find_package(KF5Config)
link_libraries(KF5::ConfigCore KF5::ConfigGui)
kconfig_add_kcfg_files(CONFIG_SRCS src/config.kcfgc)

# Find the KConfigWidgets library
find_package(KF5ConfigWidgets)
link_libraries(KF5::ConfigWidgets)

# Find the KGuiAddons library
find_package(KF5GuiAddons)
link_libraries(KF5::GuiAddons)

set(sauklaue_SRC
	src/document.cpp
	src/serializer.cpp
	src/main.cpp
	src/mainwindow.cpp
	src/pagewidget.cpp
	src/commands.cpp
	src/renderer.cpp
	src/tablet.cpp
	src/settings.cpp
	src/settings-dialog.cpp
)

add_executable(sauklaue ${sauklaue_SRC} ${CAPNP_SRCS} ${CONFIG_SRCS})

# Install the executable
install(TARGETS sauklaue DESTINATION bin)

# Mime type, desktop files, icons
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	install(FILES desktop/sauklaue.xml DESTINATION share/mime/packages)
	install(FILES desktop/sauklaue.desktop DESTINATION share/applications)
	install(FILES icons/sauklaue.svg DESTINATION share/icons/hicolor/scalable/apps)
	install(FILES icons/sauklaue.svg DESTINATION share/icons/hicolor/scalable/mimetypes RENAME application-x-sauklaue.svg)
endif()
