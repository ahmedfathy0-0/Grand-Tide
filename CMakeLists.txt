cmake_minimum_required(VERSION 3.10)
project(GFX-LAB VERSION 0.1.0 LANGUAGES C CXX)

# Linux compatibility
if(UNIX AND NOT APPLE)
        set(OpenGL_GL_PREFERENCE GLVND)
        find_package(OpenGL REQUIRED)
endif()

# These are the options we select for building GLFW as a library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)        # Don't build Documentation
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)       # Don't build Tests
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)    # Don't build Examples
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)           # Don't build Installation Information
set(GLFW_USE_HYBRID_HPG ON CACHE BOOL "" FORCE)     # Add variables to use High Performance Graphics Card if available
add_subdirectory(vendor/glfw)                       # Build the GLFW project to use later as a library

# Assimp build configuration
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE) # Prevent Assimp from adding 'd' to the lib name in debug mode
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/assimp)

# A variable with all the source files of GLAD
set(GLAD_SOURCE vendor/glad/src/gl.c)
# A variables with all the source files of Dear ImGui
set(IMGUI_SOURCES
        vendor/imgui/imgui.cpp
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui_impl/imgui_impl_glfw.cpp
        vendor/imgui/imgui_impl/imgui_impl_opengl3.cpp
        )

# Combine all vendor source files together into a single variable
set(VENDOR_SOURCES ${GLAD_SOURCE} ${IMGUI_SOURCES})

# A variable with all our source files that are common between executable targets (examples)
set(COMMON_SOURCES
        source/common/application.hpp
        source/common/application.cpp
        source/common/input/keyboard.hpp
        source/common/input/mouse.hpp

        source/common/asset-loader.cpp
        source/common/asset-loader.hpp
        source/common/deserialize-utils.hpp

        source/common/shader/shader.hpp
        source/common/shader/shader.cpp

        source/common/mesh/vertex.hpp
        source/common/mesh/mesh.hpp
        source/common/mesh/mesh-utils.hpp
        source/common/mesh/mesh-utils.cpp
        source/common/mesh/model.hpp
        source/common/mesh/model.cpp

        source/common/texture/sampler.hpp
        source/common/texture/sampler.cpp
        source/common/texture/texture2d.hpp
        source/common/texture/texture-utils.hpp
        source/common/texture/texture-utils.cpp
        source/common/texture/screenshot.hpp
        source/common/texture/screenshot.cpp

        source/common/material/pipeline-state.hpp
        source/common/material/pipeline-state.cpp
        source/common/material/material.hpp
        source/common/material/material.cpp
        source/common/material/lit-material.hpp
        source/common/material/lit-material.cpp

        source/common/ecs/component.hpp
        source/common/ecs/transform.hpp
        source/common/ecs/transform.cpp
        source/common/ecs/entity.hpp
        source/common/ecs/entity.cpp
        source/common/ecs/world.hpp
        source/common/ecs/world.cpp

        source/common/components/light.hpp
        source/common/components/light.cpp
        source/common/components/camera.hpp
        source/common/components/camera.cpp
        source/common/components/mesh-renderer.hpp
        source/common/components/mesh-renderer.cpp
        source/common/components/free-camera-controller.hpp
        source/common/components/free-camera-controller.cpp
        source/common/components/movement.hpp
        source/common/components/movement.cpp
        source/common/components/player-controller.hpp
        source/common/components/player-controller.cpp
        source/common/components/health.hpp
        source/common/components/health.cpp
        source/common/components/inventory.hpp
        source/common/components/inventory.cpp
        source/common/components/resource.hpp
        source/common/components/resource.cpp
        source/common/components/shark-component.hpp
        source/common/components/shark-component.cpp
        source/common/components/octopus-component.hpp
        source/common/components/octopus-component.cpp
        source/common/components/component-deserializer.hpp

        source/common/ecs/event-manager.hpp
        source/common/ecs/event-manager.cpp

        source/common/systems/forward-renderer.hpp
        source/common/systems/forward-renderer.cpp
        source/common/systems/free-camera-controller.hpp
        source/common/systems/player-controller-system.hpp

        source/common/systems/movement.hpp
        source/common/systems/survival-system.hpp
        source/common/systems/survival-system.cpp
        source/common/systems/animation-system.hpp
        source/common/systems/animation-system.cpp
        source/common/systems/shark-system.hpp
        source/common/systems/shark-system.cpp
        source/common/systems/octopus-system.hpp
        source/common/systems/octopus-system.cpp
)

set(STATES_SOURCES
        source/states/play-state.hpp
        source/states/menu-state.hpp
        source/states/shader-test-state.hpp
        source/states/mesh-test-state.hpp
        source/states/transform-test-state.hpp
        source/states/pipeline-test-state.hpp
        source/states/texture-test-state.hpp
        source/states/sampler-test-state.hpp
        source/states/material-test-state.hpp
        source/states/entity-test-state.hpp
        source/states/renderer-test-state.hpp
)

# Create the executable target first, then attach properties in target scope
add_executable(GAME_APPLICATION)

target_sources(GAME_APPLICATION PRIVATE
        source/main.cpp
        ${STATES_SOURCES}
        ${COMMON_SOURCES}
        ${VENDOR_SOURCES}
)

set_source_files_properties(vendor/imgui/imgui_impl/imgui_impl_opengl3.cpp PROPERTIES
        COMPILE_DEFINITIONS IMGUI_IMPL_OPENGL_LOADER_GLAD2
)

target_compile_features(GAME_APPLICATION PRIVATE cxx_std_17)
set_target_properties(GAME_APPLICATION PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin"
)

target_include_directories(GAME_APPLICATION PRIVATE
        source/common
        vendor/glfw/include
        vendor/glad/include
        vendor/glm
        vendor/imgui
        vendor/utils
        vendor/assimp/include
)

target_link_libraries(GAME_APPLICATION PRIVATE glfw assimp)

if(UNIX AND NOT APPLE)
        target_link_libraries(GAME_APPLICATION PRIVATE OpenGL::GL)
endif()