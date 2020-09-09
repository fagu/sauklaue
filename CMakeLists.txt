cmake_minimum_required(VERSION 3.0)
project(sauklaue)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to create code from Qt designer ui files
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)

include (FindPkgConfig)

pkg_check_modules(cairomm REQUIRED "cairomm-1.0")
include_directories("${cairomm_INCLUDE_DIRS}")

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/file1.proto src/file3.proto)
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS EXPORT_MACRO DLL_EXPORT src.proto)

find_package(CapnProto)
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS src/file4.capnp)
# include_directories(${CMAKE_CURRENT_BINARY_DIR})
link_libraries(CapnProto::capnp)

set(sauklaue_core_SRC
  src/document.cpp
  src/serializer.cpp
)

set(sauklaue_SRC
  src/main.cpp
  src/mainwindow.cpp
  src/pagewidget.cpp
  src/actions.cpp
  src/renderer.cpp
)

set(sauklaue_conc_SRC
  src/concatenate.cpp
)

# Tell CMake to create the helloworld executable
add_executable(sauklaue ${sauklaue_SRC} ${sauklaue_core_SRC} ${PROTO_SRCS} ${PROTO_HDRS} ${CAPNP_SRCS})
# add_executable(sauklaue-conc ${sauklaue_conc_SRC} ${sauklaue_core_SRC} ${PROTO_SRCS} ${PROTO_HDRS})

target_link_libraries(sauklaue Qt5::Widgets ${cairomm_LDFLAGS} ${Protobuf_LIBRARIES})
# target_link_libraries(sauklaue-conc Qt5::Widgets ${cairomm_LDFLAGS} ${Protobuf_LIBRARIES})

# Install the executable
install(TARGETS sauklaue DESTINATION bin)
