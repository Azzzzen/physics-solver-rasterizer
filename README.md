# Cloth Lab

实时布料模拟与光栅渲染实验项目（C++17 / OpenGL 3.3 / GLFW / GLEW / GLM / Dear ImGui）。

## 1. 渲染器特性（简要）

- 前向渲染主通道：方向光 + 点光 + 环境光 + 高光
- 阴影映射：方向光深度图 + PCF 软阴影
- 动态布料网格：每帧更新顶点位置并重建法线
- 交互输入：W/A/S/D 相机移动、右键视角、左键拖拽布料
- 运行时调参 UI：刚度、阻尼、重力系数、风力

## 2. 渲染实现机制（简要）

每帧执行流程：

1. 处理输入与快捷键（`P/R/F1/H/ESC`）
2. 开启 ImGui 新帧并构建参数面板
3. 执行布料解算（可暂停）并刷新动态网格数据
4. 阴影 pass：从光源视角写入深度贴图
5. 主渲染 pass：采样阴影贴图并完成光照计算
6. 叠加 ImGui 绘制数据并交换缓冲

核心模块：

- `src/app_main.cpp`：主循环、输入、UI、渲染 pass 组织
- `src/PhysicsSolver.cpp`：CPU 布料解算器
- `src/Mesh.cpp`：动态/静态网格上传与更新
- `src/Shader.cpp`：着色器加载、编译与 uniform 设置
- `src/Camera.cpp`：相机运动与视角控制

## 3. CPU 布料解算器（详细）

解算器位于 `src/PhysicsSolver.cpp`，模型是经典 Mass-Spring（质点-弹簧）系统。

### 3.1 状态与离散结构

设粒子数量为 \(N = rows \times cols\)。

- 位置：\(\mathbf{x}_i \in \mathbb{R}^3\)
- 速度：\(\mathbf{v}_i \in \mathbb{R}^3\)
- 质量：\(m\)（当前统一为常数）
- 固定点约束：顶部两个角点固定
- 弹簧集合 \(\mathcal{S}\)：
- 结构弹簧（上下/左右）
- 剪切弹簧（对角）
- 弯曲弹簧（跨 2 个格点）

每条弹簧 \((a,b)\) 存储静止长度 \(L_{ab}^0\)。

### 3.2 力模型

对单条弹簧，定义：

- \(\mathbf{d}=\mathbf{x}_a-\mathbf{x}_b\)
- \(\ell=\|\mathbf{d}\|\)
- \(\hat{\mathbf{d}}=\mathbf{d}/\ell\)

胡克力 + 沿弹簧方向速度阻尼：

\[
\mathbf{f}_{spring} = \left(-k(\ell-L_{ab}^0) - c_s\,((\mathbf{v}_a-\mathbf{v}_b)\cdot\hat{\mathbf{d}})\right)\hat{\mathbf{d}}
\]

其中 \(k\) 为弹簧刚度，\(c_s\) 为弹簧方向阻尼系数。

每个粒子的总力：

\[
\mathbf{f}_i = m\mathbf{g} + \sum_{(i,j)\in\mathcal{S}} \mathbf{f}_{ij} + \mathbf{f}_{wind} - c_v\mathbf{v}_i
\]

- \(\mathbf{g}\)：重力
- \(\mathbf{f}_{wind}\)：风力（当前实现为统一外力方向）
- \(c_v\)：全局速度阻尼

### 3.3 时间积分方法

当前积分不是“全隐式积分”。

当前使用的是半隐式欧拉（Symplectic Euler）：

\[
\mathbf{a}_i^n = \mathbf{f}_i(\mathbf{x}^n,\mathbf{v}^n)/m
\]
\[
\mathbf{v}_i^{n+1} = \mathbf{v}_i^n + \Delta t\,\mathbf{a}_i^n
\]
\[
\mathbf{x}_i^{n+1} = \mathbf{x}_i^n + \Delta t\,\mathbf{v}_i^{n+1}
\]

该方法比显式欧拉更稳，但不是解线性系统的 fully implicit Euler。

### 3.4 稳定性机制

为提升实时稳定性，当前实现叠加了多层保护：

- 时间步长截断：`dt <= 1/30`
- 子步进：单子步最大 `1/240` 秒
- 速度上限裁剪：`m_maxSpeed`
- 应变限制（strain limiting）投影
- 地面约束：`y >= -1.2` + 小反弹
- NaN/Inf 检测后自动 `reset()`

### 3.5 应变限制（几何投影）

对每条弹簧，若当前长度 \(\ell\) 超过上限 \(L_{max}=\alpha L_0\)（\(\alpha=m\_maxStretchRatio\)），执行位置修正：

\[
\Delta \mathbf{x} = (\ell-L_{max})\hat{\mathbf{d}}
\]

- 双端可动：两端各分担一半
- 一端锁定：另一端承担全部修正
- 拖拽点与固定点视为锁定点

这一步本质上是位置层约束投影，可显著抑制弹簧爆炸伸长。

### 3.6 交互拖拽

- 射线拾取最近非固定粒子
- 记录射线参数 \(t\)
- 拖拽期间将该粒子位置约束到目标点，并将速度置零

这样可以在交互时避免“拖拽后强回弹”导致的数值震荡。

## 4. 构建与运行

```bash
cmake -S . -B build
cmake --build build -j
./build/cloth_rasterizer
```

## 5. 快捷键

- `W/A/S/D`：相机移动
- 右键拖动：旋转视角
- 滚轮：缩放 FOV
- 左键拖动（UI 外）：拖拽布料
- `P`：暂停/继续
- `R`：重置
- `F1`：线框模式
- `H`：显示/隐藏 UI
- `ESC`：退出

## 6. 版本记录（命名版本）

### v0.3.0 - UI Capture Fix + CPU Solver Notes（2026-02-17）

- 修复 ImGui 鼠标交互问题：左键可正常拖动滑条
- 调整 GLFW 回调与 ImGui 初始化顺序，避免回调覆盖
- 文档更新：明确 CPU 解算器数学形式与积分方法（半隐式欧拉，非全隐式）

### v0.2.0 - Stable CPU Mass-Spring（2026-02-16）

- 完成 CPU 质点弹簧布料解算主路径
- 增加子步进、速度裁剪、应变限制与地面约束
- 支持射线拖拽粒子交互

### v0.1.0 - Rasterizer Baseline（2026-02-15）

- 建立 OpenGL 主渲染框架
- 加入基础光照与阴影映射
- 接入 Dear ImGui 参数面板
