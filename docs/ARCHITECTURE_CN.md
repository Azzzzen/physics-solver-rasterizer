# Cloth Lab 架构与技术说明（中文）

## 1. 项目概览

该项目是一个基于 C++17 的实时布料模拟与光栅渲染 Demo，技术栈为 OpenGL 3.3 Core + GLFW + GLEW + GLM + Dear ImGui。

核心能力包含：
- 质点-弹簧布料解算器（已做稳定性增强）
- 前向渲染管线（Phong 风格直射光照）
- 阴影映射（方向光深度图 + PCF）
- 交互相机与鼠标拖拽布料
- ImGui 运行时参数调节面板

整体采用“小模块 + 单入口集成”结构：`Camera`、`Shader`、`Mesh`、`PhysicsSolver` 等模块在 `src/app_main.cpp` 中统一组装。

## 2. 目录与模块结构

- `include/`
- `include/Camera.h`：相机模型与交互接口
- `include/Shader.h`：GLSL 程序封装与 uniform 设置
- `include/Mesh.h`：可渲染网格抽象（动态/静态）
- `include/PhysicsSolver.h`：布料解算器 API 与状态
- `include/ObjLoader.h`：OBJ 读取接口

- `src/`
- `src/app_main.cpp`：程序入口、主循环、场景、输入、渲染 pass、UI
- `src/Camera.cpp`：相机矩阵与控制逻辑
- `src/Shader.cpp`：着色器读取/编译/链接/uniform 提交
- `src/Mesh.cpp`：VAO/VBO/EBO 管理，动态顶点更新，法线重算
- `src/PhysicsSolver.cpp`：解算步骤、子步进、约束、拖拽逻辑
- `src/ObjLoader.cpp`：简化版 OBJ 解析（`v/vn/f`）

- `shaders/`
- `shaders/vertex.glsl`：主 pass 顶点变换 + 光空间投影
- `shaders/fragment.glsl`：方向光+点光+高光+阴影采样
- `shaders/shadow_depth_vertex.glsl`：阴影深度 pass 顶点着色器
- `shaders/shadow_depth_fragment.glsl`：阴影深度 pass 片元着色器

- `thirdparty/imgui/`
- Dear ImGui 源码与 GLFW/OpenGL3 backend

- `assets/`
- 资源目录，构建时复制到输出目录

- `CMakeLists.txt`
- 依赖查找、目标构建、ImGui 源码纳入

## 3. 构建与依赖

核心依赖：
- OpenGL
- GLFW
- GLEW
- GLM
- Dear ImGui（以源码形式 vendored 在 `thirdparty/imgui`）

构建目标：
- `cloth_rasterizer`

CMake 负责：
- 编译业务代码与 ImGui/backends
- 配置 ImGui 头文件路径
- 复制 `shaders/` 与 `assets/` 到 build 输出目录

## 4. 运行时主流程（每帧）

1. 处理输入与热键（`P`/`R`/`F1`/`H`）
2. 开启 ImGui 新帧并绘制参数面板
3. 若发生布料拖拽，更新鼠标射线与拖拽目标
4. 执行布料模拟步进（暂停则跳过）
5. 用解算结果更新布料网格顶点
6. 执行阴影深度 pass（写入 depth texture）
7. 执行主渲染 pass（采样阴影 + 光照）
8. 渲染 ImGui 覆盖层
9. 交换前后缓冲

该流程把“解算”和“渲染”明确分离，同时通过 `Mesh` 作为共享数据通道。

## 5. 渲染架构

### 5.1 主 pass 光照

主 pass 使用 Phong 风格直接光照，包含：
- 方向光
- 点光源（距离衰减）
- 环境光
- 高光项（`shininess`、`specular strength`）
- 阴影衰减（由方向光 depth map 提供）

关键 uniform：
- 变换：`uModel/uView/uProj/uLightSpace`
- 材质：`uBaseColor/uSpecularStrength/uShininess`
- 光照：`uLightDir/uPointLight*/uAmbientStrength`
- 视点：`uCameraPos`
- 阴影采样：`uShadowMap`

### 5.2 阴影映射实现

细节：
- 单独深度 FBO + depth texture（`kShadowWidth/Height`）
- 第一遍：从光源视角写深度
- 第二遍：将片元投影到光空间，做深度比较
- PCF 5x5 平滑阴影边缘
- 根据法线与光方向做 bias，降低 acne
- 深度 pass 开启 polygon offset，减少 z-fighting 伪影

### 5.3 网格与 GPU 数据流

`Mesh` 同时支持：
- 动态网格（布料）
- 静态网格（场景）

动态布料路径：
- CPU 更新顶点位置
- 每帧重算法线
- VBO 以 `GL_DYNAMIC_DRAW` 更新

静态场景路径：
- 一次上传，`GL_STATIC_DRAW`

## 6. 解算器架构

### 6.1 物理模型

布料是规则网格质点-弹簧系统：
- `rows * cols` 个质点
- 顶部两个角点 pin 固定
- 三类弹簧：
- 结构弹簧（structural）
- 剪切弹簧（shear）
- 弯曲弹簧（bend）

核心状态：
- 位置 `m_positions`
- 速度 `m_velocities`
- 固定掩码 `m_fixed`
- 弹簧列表（带初始长度 `restLength`）

### 6.2 稳定性策略

当前实现不是“单步粗暴欧拉”，而是多层稳健化组合：

- 帧步长上限（`<= 1/30s`）
- 自动子步进（每子步目标 `<= 1/240s`）
- 沿弹簧方向阻尼（抑制高频振荡）
- 速度上限（`m_maxSpeed`）
- 后处理应变约束（`m_maxStretchRatio`）
- 地面碰撞夹紧 + 速度衰减反弹
- NaN/Inf 检测后自动 `reset`

这样可以显著降低参数调节时的数值爆炸概率。

### 6.3 运行时参数

可实时调参：
- `stiffness`
- `damping`
- `gravity scale`
- `wind strength`

所有参数设置都带范围钳制，避免极端值直接破坏系统。

### 6.4 鼠标拖拽布料

支持射线拾取与拖拽：
- `beginDrag`：从射线附近找到最近非固定质点
- `updateDragFromRay`：根据当前射线更新目标点
- `endDrag`：释放拖拽

被拖拽粒子在解算中作为“临时锁定点”处理，并与约束系统一致。

## 7. 相机与输入系统

相机能力：
- `W/A/S/D` 平移
- 右键拖动 yaw/pitch 旋转
- 滚轮调 FOV
- 窗口缩放时自动更新 aspect ratio

输入分流：
- `ImGui::GetIO().WantCaptureMouse` 为真时，鼠标优先给 UI
- 右键用于相机观察
- 左键在 UI 外用于拖拽布料

## 8. 场景组织

当前场景由共享 unit cube 通过不同变换拼装：
- 地面
- 后墙
- 侧墙
- 悬挂杆

每个对象具备独立材质参数（颜色/高光强度/高光指数）。

## 9. Dear ImGui 集成

ImGui 集成完整：
- 初始化 Context + Dark 样式
- GLFW/OpenGL3 backend 初始化
- 每帧 `NewFrame -> UI -> Render`
- 退出时完整 Shutdown

面板能力：
- 运行时滑条调参
- 快捷键说明
- 状态文本（Paused / Running）

线框模式兼容：
- 若场景启用 wireframe，渲染 ImGui 前临时切回 fill，渲染后恢复。

## 10. 操作说明

- `W/A/S/D`：相机平移
- 右键拖动：观察视角
- 滚轮：缩放（FOV）
- 左键拖动（UI 外）：拖拽布料点
- `P`：暂停/继续解算
- `R`：重置布料与参数
- `F1`：线框模式切换
- `H`：显示/隐藏 UI
- `ESC`：退出程序

## 11. 当前限制

1. 布料仍是 mass-spring，并非 FEM / XPBD 等工业级模型
2. 没有自碰撞，也没有通用网格碰撞
3. 无空间加速结构（广相/窄相未系统化）
4. 解算与法线重算在 CPU 上，尚未 GPU 化
5. OBJ 加载器较简化，且当前主场景未走 OBJ 资源路径
6. 材质系统较基础，无 PBR/贴图/法线贴图
7. 阴影仅单张方向光 shadow map（无 CSM）

