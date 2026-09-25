add_executable(lingolens_harness main.cpp)

target_link_libraries(lingolens_harness PRIVATE lingolens_core)

# The harness includes both core headers and the ppm_io.h helper
target_include_directories(lingolens_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
)
