# ─── Optional ONNX Runtime backend ───────────────────────────────
# Enable with:  cmake -DLINGOLENS_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path ..

option(LINGOLENS_ENABLE_ONNX "Build the ONNX Runtime backend" OFF)

if(LINGOLENS_ENABLE_ONNX)
    if(NOT DEFINED ONNXRUNTIME_ROOT)
        message(FATAL_ERROR
            "ONNXRUNTIME_ROOT must be set when LINGOLENS_ENABLE_ONNX is ON.\n"
            "Example: cmake -DONNXRUNTIME_ROOT=/usr/local/onnxruntime ..")
    endif()

    add_library(lingolens_onnx
        onnx/onnx_backends.cpp
    )

    target_include_directories(lingolens_onnx PUBLIC
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ONNXRUNTIME_ROOT}/include
    )

    target_link_directories(lingolens_onnx PUBLIC
        ${ONNXRUNTIME_ROOT}/lib
    )

    target_link_libraries(lingolens_onnx PUBLIC
        lingolens_core
        onnxruntime
    )

    target_compile_definitions(lingolens_onnx PUBLIC LINGOLENS_HAS_ONNX=1)

    message(STATUS "ONNX Runtime backend: ON  (${ONNXRUNTIME_ROOT})")
else()
    message(STATUS "ONNX Runtime backend: OFF (use -DLINGOLENS_ENABLE_ONNX=ON to enable)")
endif()
