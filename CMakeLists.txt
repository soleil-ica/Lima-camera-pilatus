# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

find_package(yat CONFIG REQUIRED)
find_package(tiff CONFIG REQUIRED)

# --------------------------------------------------------------------------
# Collect sources and includes
# --------------------------------------------------------------------------

file(GLOB_RECURSE sources src/*.cpp)
set(includedirs
    include/
)

# --------------------------------------------------------------------------
# Target
# --------------------------------------------------------------------------

add_library(limapilatus ${sources})
target_include_directories(limapilatus 
    PUBLIC 
        ${includedirs}
)
target_link_libraries(limapilatus 
    PUBLIC 
        limacore
        yat::yat
        tiff::tiff
)

limatools_set_library_soversion(limapilatus "VERSION")

# --------------------------------------------------------------------------
# Compile definitions
# --------------------------------------------------------------------------

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PILATUS_VERSION_RAW)
string(STRIP "${PILATUS_VERSION_RAW}" PILATUS_VERSION)

target_compile_definitions(limapilatus
    PUBLIC
        PILATUS_NAME=Pilatus
        PILATUS_VERSION=${PILATUS_VERSION}
        PILATUS_ENABLED
)

message(STATUS "Camera enabled: Pilatus ${PILATUS_VERSION}")

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------

install(
    TARGETS limapilatus 
    LIBRARY DESTINATION lib
)
