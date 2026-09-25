add_library(lingolens_core
    src/router.cpp
    src/style_analyzer.cpp
    src/inpainter.cpp
    src/homography.cpp
    src/tracker.cpp
    src/text_renderer.cpp
    src/mock_backends.cpp
    src/pipeline.cpp
)

target_include_directories(lingolens_core PUBLIC include)
