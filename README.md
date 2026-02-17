# Cloth Lab Architecture and Technical Overview (EN)

## 1. Project Summary

This project is a real-time cloth simulation and rasterization demo built with C++17, OpenGL 3.3 Core, GLFW, GLEW, GLM, and Dear ImGui.

It combines:
- A mass-spring cloth solver (with stability improvements)
- A forward raster renderer (Phong-style direct lighting)
- Shadow mapping (directional light depth pass + PCF)
- Interactive camera and mouse-based cloth dragging
- Runtime tuning UI via Dear ImGui

The codebase is organized around a small set of focused modules (`Camera`, `Shader`, `Mesh`, `PhysicsSolver`) and one integration entry point (`src/app_main.cpp`).

## 2. Directory and Module Layout

- `include/`
- `include/Camera.h`: camera model and interaction API
- `include/Shader.h`: GLSL program wrapper and uniform setters
- `include/Mesh.h`: renderable mesh abstraction (dynamic and static)
- `include/PhysicsSolver.h`: cloth simulation API and state
- `include/ObjLoader.h`: OBJ loader interface

- `src/`
- `src/app_main.cpp`: application bootstrap, render loop, UI, scene, input, passes
- `src/Camera.cpp`: camera math and controls
- `src/Shader.cpp`: shader file I/O, compile/link, uniform binding
- `src/Mesh.cpp`: GPU buffers, dynamic vertex update, normal recompute
- `src/PhysicsSolver.cpp`: simulation update, substeps, constraints, dragging
- `src/ObjLoader.cpp`: minimal OBJ parsing (`v/vn/f`)

- `shaders/`
- `shaders/vertex.glsl`: main vertex transform + light-space projection
- `shaders/fragment.glsl`: directional + point light, specular, shadow sampling
- `shaders/shadow_depth_vertex.glsl`: depth-only shadow pass vertex shader
- `shaders/shadow_depth_fragment.glsl`: depth-only shadow pass fragment shader

- `thirdparty/imgui/`
- Dear ImGui sources and GLFW/OpenGL3 backends

- `assets/`
- Scene/model assets copied to build output by CMake

- `CMakeLists.txt`
- Dependency discovery + executable target + ImGui source integration

## 3. Build and Runtime Dependencies

Core dependencies:
- OpenGL
- GLFW
- GLEW
- GLM
- Dear ImGui (vendored under `thirdparty/imgui`)

CMake target:
- `cloth_rasterizer`

Build responsibilities:
- Compile app modules and ImGui sources
- Include ImGui backend headers
- Copy `shaders/` and `assets/` to build directory

## 4. High-Level Runtime Pipeline

Per frame, the pipeline is:

1. Poll input and update toggles (`P`, `R`, `F1`, `H`)
2. Start ImGui frame and evaluate UI controls
3. Convert mouse to world ray if cloth-dragging is active
4. Step cloth simulation (unless paused)
5. Update cloth mesh dynamic vertex buffer
6. Render shadow depth pass into depth texture
7. Render main pass with lighting + shadow lookup
8. Render ImGui draw data as overlay
9. Swap buffers

This architecture clearly separates simulation update from render passes while sharing geometry through the `Mesh` abstraction.

## 5. Rendering Architecture

### 5.1 Main Pass Shading

The main pass uses a classic direct-lighting model with:
- Directional light
- Point light with attenuation
- Ambient term
- Specular term (shininess + specular strength)
- Shadow attenuation from the directional light depth map

Shader inputs include:
- `uModel`, `uView`, `uProj`, `uLightSpace`
- material controls (`uBaseColor`, `uSpecularStrength`, `uShininess`)
- light controls (`uLightDir`, `uPointLight*`, `uAmbientStrength`)
- camera position (`uCameraPos`)
- shadow map sampler (`uShadowMap`)

### 5.2 Shadow Mapping

Implementation details:
- Dedicated depth framebuffer + depth texture (`kShadowWidth/Height`)
- First pass renders scene depth from directional light view
- Second pass projects fragment world position into light space
- 5x5 PCF softening in fragment shader
- Slope-aware depth bias to reduce acne
- Polygon offset enabled in depth pass to reduce z-fighting artifacts

### 5.3 Mesh and GPU Data Flow

`Mesh` supports:
- Dynamic cloth mesh mode (positions updated every frame)
- Static scene mesh mode (uploaded once)

For dynamic cloth:
- CPU-side positions updated from solver
- per-frame normal recomputation
- VBO refreshed with `GL_DYNAMIC_DRAW`

For static objects:
- one-time VBO/EBO upload with `GL_STATIC_DRAW`

## 6. Physics Solver Architecture

### 6.1 Model

The cloth is a particle-spring system with:
- grid particles (`rows * cols`)
- fixed pins at two top corners
- spring sets:
- structural springs
- shear springs
- bend springs

State vectors:
- particle positions
- velocities
- fixed-mask
- spring list with rest lengths

### 6.2 Stability Strategy

The solver is not a naive single-step explicit update. It uses several robustness layers:

- Time-step clamping (`<= 1/30 s`)
- Fixed-size substeps (target `<= 1/240 s` each)
- Spring damping along spring direction
- Velocity capping (`m_maxSpeed`)
- Strain limiting (`m_maxStretchRatio`) as a post-integrate constraint
- Ground collision clamp (`y >= -1.2`) with restitution damping
- NaN/Inf detection and automatic reset fallback

This greatly delays or prevents blow-ups under interactive parameter tuning.

### 6.3 Runtime Interactivity

Exposed runtime parameters:
- stiffness
- damping
- gravity scale
- wind strength

All are clamped on assignment to bounded ranges.

### 6.4 Cloth Dragging

The solver supports direct manipulation via ray picking:
- find nearest non-fixed particle around a ray (`beginDrag`)
- lock dragged particle to ray target each step
- update target from current camera ray (`updateDragFromRay`)
- release (`endDrag`)

This is integrated with UI mouse capture logic to avoid conflict with sliders.

## 7. Camera and Input System

Camera features:
- WASD translation
- right mouse drag for yaw/pitch
- scroll wheel FOV zoom
- dynamic aspect ratio update on resize

Input routing details:
- UI capture (`ImGui::GetIO().WantCaptureMouse`) blocks camera/drag mouse actions
- right mouse is reserved for camera look
- left mouse outside UI is used for cloth dragging

## 8. Scene Composition

Current scene is procedurally assembled from a shared unit-cube mesh with different transforms:
- floor plane
- back wall
- side wall
- cloth support bar

Each object has per-object color/specular/shininess material parameters.

## 9. Dear ImGui Integration

ImGui is fully integrated in the main loop:
- context creation + style setup
- `ImGui_ImplGlfw` + `ImGui_ImplOpenGL3` backends
- per-frame NewFrame/Render lifecycle
- clean shutdown at exit

Panel capabilities:
- live sliders for simulation parameters
- control legend for hotkeys
- state display (paused/running)

Special handling:
- if scene is in wireframe mode, polygon mode is temporarily switched to fill while drawing ImGui, then restored.

## 10. Controls and UX

- `W/A/S/D`: move camera
- Right mouse drag: look around
- Mouse wheel: zoom (FOV)
- Left mouse drag (outside UI): drag cloth particles
- `P`: pause/resume simulation
- `R`: reset simulation and parameters
- `F1`: wireframe toggle
- `H`: UI panel toggle
- `ESC`: quit

## 11. Current Limitations

1. Cloth model is mass-spring, not FEM/PBD/XPBD production-grade cloth
2. No self-collision or cloth-object collision against arbitrary meshes
3. No broadphase or spatial acceleration structures
4. CPU-only simulation and normal generation (no GPU compute)
5. OBJ loader is minimal and not currently used by the active scene path
6. No texture/material system (PBR, normal maps, etc.)
7. Shadow mapping is single-map directional only (no CSM)

