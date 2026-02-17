cmake_minimum_required(VERSION 3.16)
project(ClothRasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

set(GLFW_TARGET "")
if (TARGET glfw::glfw)
    set(GLFW_TARGET glfw::glfw)
elseif (TARGET glfw)
    set(GLFW_TARGET glfw)
endif ()
if (GLFW_TARGET STREQUAL "")
    message(FATAL_ERROR "No GLFW target found (expected glfw::glfw or glfw)")
endif ()

set(GLM_TARGET "")
if (TARGET glm::glm)
    set(GLM_TARGET glm::glm)
elseif (TARGET glm)
    set(GLM_TARGET glm)
endif ()
if (GLM_TARGET STREQUAL "")
    message(FATAL_ERROR "No GLM target found (expected glm::glm or glm)")
endif ()

set(GLEW_TARGET "")
if (TARGET GLEW::GLEW)
    set(GLEW_TARGET GLEW::GLEW)
elseif (TARGET GLEW::glew)
    set(GLEW_TARGET GLEW::glew)
endif ()
if (GLEW_TARGET STREQUAL "")
    message(FATAL_ERROR "No GLEW target found (expected GLEW::GLEW or GLEW::glew)")
endif ()

add_executable(cloth_rasterizer
    src/app_main.cpp
    src/Camera.cpp
    src/Mesh.cpp
    src/ObjLoader.cpp
    src/Shader.cpp
    src/PhysicsSolver.cpp
    src/GpuPhysicsSolver.cpp
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/backends/imgui_impl_glfw.cpp
    thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(cloth_rasterizer PRIVATE include thirdparty/imgui thirdparty/imgui/backends)
target_link_libraries(cloth_rasterizer PRIVATE OpenGL::GL ${GLFW_TARGET} ${GLEW_TARGET} ${GLM_TARGET})

file(COPY shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
