/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Suckless OGL", "index.html", [
    [ "Suckless-OGL", "index.html", "index" ],
    [ "Architecture Documentation - Refactoring Core", "md_docs_2architecture.html", [
      [ "Overview", "md_docs_2architecture.html#autotoc_md9", [
        [ "Modules", "md_docs_2architecture.html#autotoc_md10", null ]
      ] ],
      [ "Architecture Diagram", "md_docs_2architecture.html#autotoc_md11", null ],
      [ "Data Ownership", "md_docs_2architecture.html#autotoc_md12", null ],
      [ "Build System", "md_docs_2architecture.html#autotoc_md13", null ],
      [ "Performance Impact", "md_docs_2architecture.html#autotoc_md14", null ]
    ] ],
    [ "Asynchronous Environment Map Loader", "md_docs_2async__loader.html", [
      [ "Overview", "md_docs_2async__loader.html#autotoc_md16", null ],
      [ "Architecture", "md_docs_2async__loader.html#autotoc_md17", [
        [ "Data Flow", "md_docs_2async__loader.html#autotoc_md18", null ]
      ] ]
    ] ],
    [ "Build System and Dependency Management", "md_docs_2build.html", [
      [ "Build Requirements", "md_docs_2build.html#autotoc_md20", null ],
      [ "Distrobox Integration", "md_docs_2build.html#autotoc_md21", [
        [ "Common Commands", "md_docs_2build.html#autotoc_md22", null ]
      ] ],
      [ "Offline Build Support", "md_docs_2build.html#autotoc_md23", [
        [ "1. Preparation (Online)", "md_docs_2build.html#autotoc_md24", null ],
        [ "2. Building (Offline)", "md_docs_2build.html#autotoc_md25", null ],
        [ "3. Testing the Offline Mode", "md_docs_2build.html#autotoc_md26", null ]
      ] ],
      [ "Dependency Management", "md_docs_2build.html#autotoc_md27", [
        [ "cglm (OpenGL Mathematics for C)", "md_docs_2build.html#autotoc_md28", null ],
        [ "GLAD (OpenGL Loader Generator)", "md_docs_2build.html#autotoc_md29", null ],
        [ "GLFW", "md_docs_2build.html#autotoc_md30", null ],
        [ "Unity", "md_docs_2build.html#autotoc_md31", null ],
        [ "cJSON", "md_docs_2build.html#autotoc_md32", null ]
      ] ],
      [ "Folder Structure", "md_docs_2build.html#autotoc_md33", null ],
      [ "Fast Parallel Builds", "md_docs_2build.html#autotoc_md34", null ],
      [ "Logging System", "md_docs_2build.html#autotoc_md35", null ],
      [ "Automated Dependencies", "md_docs_2build.html#autotoc_md36", null ],
      [ "Folder Structure", "md_docs_2build.html#autotoc_md37", null ]
    ] ],
    [ "Cubemap Seam Resolution", "md_docs_2cubemap__seam__resolution.html", [
      [ "🔍 Identified Problem", "md_docs_2cubemap__seam__resolution.html#autotoc_md39", null ],
      [ "🏁 Definitive Solution: Equirectangular Mapping", "md_docs_2cubemap__seam__resolution.html#autotoc_md40", [
        [ "Why?", "md_docs_2cubemap__seam__resolution.html#autotoc_md41", null ],
        [ "Visual Comparison", "md_docs_2cubemap__seam__resolution.html#autotoc_md42", null ],
        [ "Comparison: Cubemap vs Equirectangular", "md_docs_2cubemap__seam__resolution.html#autotoc_md43", null ],
        [ "Software Implementation", "md_docs_2cubemap__seam__resolution.html#autotoc_md44", null ],
        [ "Conclusion", "md_docs_2cubemap__seam__resolution.html#autotoc_md45", null ]
      ] ]
    ] ],
    [ "Docker Support", "md_docs_2docker.html", [
      [ "Prerequisites", "md_docs_2docker.html#autotoc_md47", null ],
      [ "Quick Start", "md_docs_2docker.html#autotoc_md48", [
        [ "Build the Image", "md_docs_2docker.html#autotoc_md49", null ],
        [ "Run the Application", "md_docs_2docker.html#autotoc_md50", null ]
      ] ],
      [ "Architecture", "md_docs_2docker.html#autotoc_md51", [
        [ "Multi-Stage Build", "md_docs_2docker.html#autotoc_md52", [
          [ "Stage 1: Builder (fedora:41)", "md_docs_2docker.html#autotoc_md53", null ],
          [ "Stage 2: Runtime (fedora:41)", "md_docs_2docker.html#autotoc_md54", null ]
        ] ],
        [ "Headless Rendering with Xvfb", "md_docs_2docker.html#autotoc_md55", null ],
        [ "Build Cache Optimization", "md_docs_2docker.html#autotoc_md56", null ]
      ] ],
      [ "Makefile Targets", "md_docs_2docker.html#autotoc_md57", [
        [ "Build Targets", "md_docs_2docker.html#autotoc_md58", null ],
        [ "Maintenance Targets", "md_docs_2docker.html#autotoc_md59", null ]
      ] ],
      [ "Advanced Usage", "md_docs_2docker.html#autotoc_md60", [
        [ "Custom Container Engine", "md_docs_2docker.html#autotoc_md61", null ],
        [ "Incremental Builds", "md_docs_2docker.html#autotoc_md62", null ],
        [ "Running with Host X11", "md_docs_2docker.html#autotoc_md63", null ]
      ] ],
      [ "Troubleshooting", "md_docs_2docker.html#autotoc_md64", [
        [ "Build Cache Not Working", "md_docs_2docker.html#autotoc_md65", null ],
        [ "Xvfb Fails to Start", "md_docs_2docker.html#autotoc_md66", null ],
        [ "X11 Permission Denied", "md_docs_2docker.html#autotoc_md67", null ]
      ] ],
      [ "CI/CD Integration", "md_docs_2docker.html#autotoc_md68", null ]
    ] ],
    [ "Doxygen Customization & Modernization", "md_docs_2doxygen__customization.html", [
      [ "🎨 Theme: Doxygen-Awesome", "md_docs_2doxygen__customization.html#autotoc_md71", [
        [ "Integrated Components", "md_docs_2doxygen__customization.html#autotoc_md72", null ]
      ] ],
      [ "💻 Advanced Syntax Highlighting", "md_docs_2doxygen__customization.html#autotoc_md74", [
        [ "1. Shader Support", "md_docs_2doxygen__customization.html#autotoc_md75", null ],
        [ "2. Indentation Preservation Fix", "md_docs_2doxygen__customization.html#autotoc_md76", null ]
      ] ],
      [ "📊 \"Suckless-Modern Ghost\" Diagrams", "md_docs_2doxygen__customization.html#autotoc_md78", [
        [ "1. Engine Configuration", "md_docs_2doxygen__customization.html#autotoc_md79", null ],
        [ "2. Design Tokens", "md_docs_2doxygen__customization.html#autotoc_md80", null ]
      ] ],
      [ "🔧 Core Configuration (Doxyfile)", "md_docs_2doxygen__customization.html#autotoc_md82", null ],
      [ "🛠️ Maintenance", "md_docs_2doxygen__customization.html#autotoc_md84", null ]
    ] ],
    [ "Global Exposure Management Analysis", "md_docs_2exposure__analysis.html", [
      [ "1. Definition and Data Structure", "md_docs_2exposure__analysis.html#autotoc_md86", [
        [ "CPU Side (include/postprocess.h, src/postprocess.c)", "md_docs_2exposure__analysis.html#autotoc_md87", null ],
        [ "GPU Side (Uniforms & Textures)", "md_docs_2exposure__analysis.html#autotoc_md88", null ]
      ] ],
      [ "2. Runpaths and Usage Modes", "md_docs_2exposure__analysis.html#autotoc_md90", [
        [ "Mode A: Automatic Exposure (POSTFX_AUTO_EXPOSURE)", "md_docs_2exposure__analysis.html#autotoc_md91", null ],
        [ "Mode B: Manual Exposure (POSTFX_EXPOSURE)", "md_docs_2exposure__analysis.html#autotoc_md92", null ],
        [ "Mode C: No Exposure", "md_docs_2exposure__analysis.html#autotoc_md93", null ]
      ] ],
      [ "3. Integration in the Pipeline (postprocess.frag)", "md_docs_2exposure__analysis.html#autotoc_md95", null ],
      [ "4. Pre-calculation and Optimizations", "md_docs_2exposure__analysis.html#autotoc_md96", null ],
      [ "Synthetic Summary", "md_docs_2exposure__analysis.html#autotoc_md97", [
        [ "Adaptation Pipeline", "md_docs_2exposure__analysis.html#autotoc_md98", null ]
      ] ]
    ] ],
    [ "FXAA 3.11 Implementation & Optimizations", "md_docs_2FXAA.html", [
      [ "Overview", "md_docs_2FXAA.html#autotoc_md100", null ],
      [ "Key Optimizations", "md_docs_2FXAA.html#autotoc_md101", [
        [ "1. Luma-in-Alpha (Bandwidth Optimization)", "md_docs_2FXAA.html#autotoc_md102", [
          [ "Pipeline Data Flow", "md_docs_2FXAA.html#autotoc_md103", null ]
        ] ],
        [ "2. sRGB / Gamma Correctness", "md_docs_2FXAA.html#autotoc_md104", null ],
        [ "3. Dual Mode (Quality vs. Performance)", "md_docs_2FXAA.html#autotoc_md105", null ],
        [ "4. Non-Linear Search (Quality Mode)", "md_docs_2FXAA.html#autotoc_md106", null ]
      ] ],
      [ "Configuration", "md_docs_2FXAA.html#autotoc_md107", null ],
      [ "Debugging", "md_docs_2FXAA.html#autotoc_md108", null ]
    ] ],
    [ "GPU Rendering Synchronization: Intel vs NVIDIA", "md_docs_2gpu-rendering-synchronization.html", [
      [ "Executive Summary", "md_docs_2gpu-rendering-synchronization.html#autotoc_md110", null ],
      [ "Visual Comparison", "md_docs_2gpu-rendering-synchronization.html#autotoc_md111", null ],
      [ "Root Causes Identified", "md_docs_2gpu-rendering-synchronization.html#autotoc_md112", [
        [ "Issue 1: FXAA Luminance Recalculation", "md_docs_2gpu-rendering-synchronization.html#autotoc_md113", null ],
        [ "Issue 2: Derivative-Based Roughness Clamping", "md_docs_2gpu-rendering-synchronization.html#autotoc_md114", null ]
      ] ],
      [ "Why Derivatives Differ", "md_docs_2gpu-rendering-synchronization.html#autotoc_md115", null ],
      [ "Trade-offs", "md_docs_2gpu-rendering-synchronization.html#autotoc_md116", [
        [ "Lost", "md_docs_2gpu-rendering-synchronization.html#autotoc_md117", null ],
        [ "Gained", "md_docs_2gpu-rendering-synchronization.html#autotoc_md118", null ]
      ] ],
      [ "Derivatives vs Analytic Performance", "md_docs_2gpu-rendering-synchronization.html#autotoc_md119", null ],
      [ "Validation Results", "md_docs_2gpu-rendering-synchronization.html#autotoc_md120", [
        [ "Visual Inspection", "md_docs_2gpu-rendering-synchronization.html#autotoc_md121", null ],
        [ "Reference Metrics (FXAA Synthetic Test)", "md_docs_2gpu-rendering-synchronization.html#autotoc_md122", null ]
      ] ],
      [ "Files Modified", "md_docs_2gpu-rendering-synchronization.html#autotoc_md123", null ],
      [ "References", "md_docs_2gpu-rendering-synchronization.html#autotoc_md124", null ]
    ] ],
    [ "IBL Optimization Strategies", "md_docs_2ibl__architecture__ideas.html", [
      [ "1. Problem Analysis", "md_docs_2ibl__architecture__ideas.html#autotoc_md126", null ],
      [ "2. Proposed Solutions", "md_docs_2ibl__architecture__ideas.html#autotoc_md127", [
        [ "A. Progressive IBL (Temporal Amortization)", "md_docs_2ibl__architecture__ideas.html#autotoc_md128", null ],
        [ "B. Adaptive Sample Counting", "md_docs_2ibl__architecture__ideas.html#autotoc_md129", null ],
        [ "C. Resolution Capping", "md_docs_2ibl__architecture__ideas.html#autotoc_md130", null ],
        [ "D. Tiled Dispatching", "md_docs_2ibl__architecture__ideas.html#autotoc_md131", null ],
        [ "E. Instant Placeholder (The \"Fast Path\")", "md_docs_2ibl__architecture__ideas.html#autotoc_md132", null ]
      ] ],
      [ "3. Implementation Roadmap", "md_docs_2ibl__architecture__ideas.html#autotoc_md133", null ]
    ] ],
    [ "Mouse Camera Control", "md_docs_2mouse__camera__control.html", [
      [ "Overview", "md_docs_2mouse__camera__control.html#autotoc_md135", null ],
      [ "Features", "md_docs_2mouse__camera__control.html#autotoc_md136", null ],
      [ "Architecture", "md_docs_2mouse__camera__control.html#autotoc_md137", [
        [ "Data Flow", "md_docs_2mouse__camera__control.html#autotoc_md138", null ]
      ] ],
      [ "Implementation Details", "md_docs_2mouse__camera__control.html#autotoc_md139", [
        [ "Rotation Calculation (Euler Angles)", "md_docs_2mouse__camera__control.html#autotoc_md140", null ],
        [ "Input Handling", "md_docs_2mouse__camera__control.html#autotoc_md141", null ]
      ] ],
      [ "Usage", "md_docs_2mouse__camera__control.html#autotoc_md142", null ]
    ] ],
    [ "NVIDIA OpenGL Support: Launch, Stability & Optimizations", "md_docs_2nvidia__optimizations.html", [
      [ "I. Startup & Stability Fixes (Critical)", "md_docs_2nvidia__optimizations.html#autotoc_md145", [
        [ "1. Robust Texture Mipmap Allocation (Error 0x501)", "md_docs_2nvidia__optimizations.html#autotoc_md146", null ],
        [ "2. Object Labeling & Initialization Sequence (Error 1282)", "md_docs_2nvidia__optimizations.html#autotoc_md147", null ]
      ] ],
      [ "II. Performance & Warning Cleanups", "md_docs_2nvidia__optimizations.html#autotoc_md149", [
        [ "1. Dummy Texture Strategy (Unit Binding Cleanup)", "md_docs_2nvidia__optimizations.html#autotoc_md150", null ],
        [ "2. VAO State Reconciliation (Shader Recompilation)", "md_docs_2nvidia__optimizations.html#autotoc_md151", null ],
        [ "3. Efficient Memory Placement (Buffer Movement)", "md_docs_2nvidia__optimizations.html#autotoc_md152", null ]
      ] ]
    ] ],
    [ "OpenGL Stability & Performance Cleanup (2026-01-28)", "md_docs_2opengl__cleanup.html", [
      [ "1. Stability & Rendering Errors", "md_docs_2opengl__cleanup.html#autotoc_md154", [
        [ "IBL: Mipmap Allocation (0x501)", "md_docs_2opengl__cleanup.html#autotoc_md155", null ],
        [ "Object Labeling (1282)", "md_docs_2opengl__cleanup.html#autotoc_md156", null ],
        [ "Fallback Protection (0x502)", "md_docs_2opengl__cleanup.html#autotoc_md157", null ]
      ] ],
      [ "2. Performance Optimizations (NVIDIA)", "md_docs_2opengl__cleanup.html#autotoc_md158", [
        [ "Buffer Migration (0x20072)", "md_docs_2opengl__cleanup.html#autotoc_md159", null ],
        [ "Resize Bridge (0x20084)", "md_docs_2opengl__cleanup.html#autotoc_md160", null ],
        [ "Shader Recompilation (0x20092)", "md_docs_2opengl__cleanup.html#autotoc_md161", null ]
      ] ],
      [ "3. Residual Warning 0x20092 Analysis", "md_docs_2opengl__cleanup.html#autotoc_md162", null ]
    ] ],
    [ "Photographic Standards for Real-Time Rendering", "md_docs_2photographic__standards.html", [
      [ "📸 The 18% Middle Gray - The Cornerstone", "md_docs_2photographic__standards.html#autotoc_md165", [
        [ "Photographic Origin", "md_docs_2photographic__standards.html#autotoc_md166", null ],
        [ "Why 18%?", "md_docs_2photographic__standards.html#autotoc_md167", null ],
        [ "Applications", "md_docs_2photographic__standards.html#autotoc_md168", null ]
      ] ],
      [ "🎚️ Photographic Values Scale", "md_docs_2photographic__standards.html#autotoc_md169", [
        [ "Exposure Values (EV)", "md_docs_2photographic__standards.html#autotoc_md170", null ],
        [ "Visual EV Scale", "md_docs_2photographic__standards.html#autotoc_md171", null ],
        [ "Auto-Exposure Formula", "md_docs_2photographic__standards.html#autotoc_md172", null ]
      ] ],
      [ "🌈 Material Reflectance Values", "md_docs_2photographic__standards.html#autotoc_md173", [
        [ "Standard Albedos (Physically Based)", "md_docs_2photographic__standards.html#autotoc_md174", null ]
      ] ],
      [ "📊 Tone Mapping - Standard Curves", "md_docs_2photographic__standards.html#autotoc_md175", [
        [ "1. Linear (Naive)", "md_docs_2photographic__standards.html#autotoc_md176", null ],
        [ "2. Reinhard (Simple)", "md_docs_2photographic__standards.html#autotoc_md177", null ],
        [ "3. Uncharted 2 / Hable (Filmic)", "md_docs_2photographic__standards.html#autotoc_md178", null ],
        [ "4. ACES (Academy Color Encoding System)", "md_docs_2photographic__standards.html#autotoc_md179", null ]
      ] ],
      [ "🎮 Recommended Values for Games", "md_docs_2photographic__standards.html#autotoc_md180", [
        [ "Auto-Exposure Settings", "md_docs_2photographic__standards.html#autotoc_md181", null ],
        [ "Examples by Genre", "md_docs_2photographic__standards.html#autotoc_md182", null ]
      ] ],
      [ "🔧 Interesting Alternative Values", "md_docs_2photographic__standards.html#autotoc_md183", [
        [ "Key Value Alternatives", "md_docs_2photographic__standards.html#autotoc_md184", null ],
        [ "Middle Gray in sRGB", "md_docs_2photographic__standards.html#autotoc_md185", null ]
      ] ],
      [ "📐 Useful Formulas", "md_docs_2photographic__standards.html#autotoc_md186", [
        [ "Luminance Conversion", "md_docs_2photographic__standards.html#autotoc_md187", null ],
        [ "EV ↔ Luminance Conversion", "md_docs_2photographic__standards.html#autotoc_md188", null ],
        [ "Temporal Adaptation", "md_docs_2photographic__standards.html#autotoc_md189", null ]
      ] ],
      [ "🎨 Practical Workflow", "md_docs_2photographic__standards.html#autotoc_md190", [
        [ "1. Initial Calibration", "md_docs_2photographic__standards.html#autotoc_md191", null ],
        [ "2. Test with Type Scenes", "md_docs_2photographic__standards.html#autotoc_md192", null ],
        [ "3. Artistic Tweaking", "md_docs_2photographic__standards.html#autotoc_md193", null ]
      ] ],
      [ "📚 Recommended Resources", "md_docs_2photographic__standards.html#autotoc_md194", null ],
      [ "✨ Bonus: Exotic Values", "md_docs_2photographic__standards.html#autotoc_md195", [
        [ "Lunar Scenes", "md_docs_2photographic__standards.html#autotoc_md196", null ],
        [ "Underwater", "md_docs_2photographic__standards.html#autotoc_md197", null ],
        [ "Space", "md_docs_2photographic__standards.html#autotoc_md198", null ]
      ] ]
    ] ],
    [ "Post-Processing UBO Architecture", "md_docs_2postprocess__ubo__architecture.html", [
      [ "1. Overview", "md_docs_2postprocess__ubo__architecture.html#autotoc_md200", null ],
      [ "2. Critical Constraints: std140 Layout", "md_docs_2postprocess__ubo__architecture.html#autotoc_md201", [
        [ "std140 Alignment Rules (Simplified)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md202", null ],
        [ "The \"Array vs Scalar\" Padding Trap", "md_docs_2postprocess__ubo__architecture.html#autotoc_md203", null ],
        [ "Memory Visualization (std140)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md204", null ]
      ] ],
      [ "3. Data Structure", "md_docs_2postprocess__ubo__architecture.html#autotoc_md205", [
        [ "C Structure (postprocess.h)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md206", null ],
        [ "GLSL Block (ubo.glsl)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md207", null ]
      ] ],
      [ "4. Adding a New Parameter", "md_docs_2postprocess__ubo__architecture.html#autotoc_md208", null ],
      [ "5. Performance", "md_docs_2postprocess__ubo__architecture.html#autotoc_md209", null ]
    ] ],
    [ "Profiling Guide: ApiTrace & Trace Analysis", "md_docs_2profiling__guide.html", [
      [ "Why use this workflow?", "md_docs_2profiling__guide.html#autotoc_md211", null ],
      [ "1. Prerequisites", "md_docs_2profiling__guide.html#autotoc_md213", null ],
      [ "2. Generating a Trace", "md_docs_2profiling__guide.html#autotoc_md215", null ],
      [ "3. High-Level Performance Analysis", "md_docs_2profiling__guide.html#autotoc_md217", [
        [ "Understanding the Metrics", "md_docs_2profiling__guide.html#autotoc_md218", null ]
      ] ],
      [ "4. Advanced Tool Usage", "md_docs_2profiling__guide.html#autotoc_md220", [
        [ "Script Logic", "md_docs_2profiling__guide.html#autotoc_md221", null ]
      ] ],
      [ "5. Coding for Profiling", "md_docs_2profiling__guide.html#autotoc_md223", null ],
      [ "6. Developing the Tool", "md_docs_2profiling__guide.html#autotoc_md225", null ]
    ] ],
    [ "Progressive & Asynchronous IBL Architecture", "md_docs_2progressive__ibl.html", [
      [ "1. Overview", "md_docs_2progressive__ibl.html#autotoc_md227", [
        [ "The Pipeline", "md_docs_2progressive__ibl.html#autotoc_md228", null ]
      ] ],
      [ "2. \"Slicing\" Strategy", "md_docs_2progressive__ibl.html#autotoc_md230", [
        [ "2.1 Overlap Protection (Crucial)", "md_docs_2progressive__ibl.html#autotoc_md231", null ]
      ] ],
      [ "3. Optimized Configuration (Adaptive Slicing)", "md_docs_2progressive__ibl.html#autotoc_md233", [
        [ "A. Irradiance Map (64x64)", "md_docs_2progressive__ibl.html#autotoc_md234", null ],
        [ "B. Specular Map (512x512)", "md_docs_2progressive__ibl.html#autotoc_md235", null ]
      ] ],
      [ "4. Global Performance", "md_docs_2progressive__ibl.html#autotoc_md237", null ],
      [ "5. Key Files", "md_docs_2progressive__ibl.html#autotoc_md238", null ]
    ] ],
    [ "Project Structure: Refactored Icosphere", "md_docs_2project__structure.html", [
      [ "Modular Architecture", "md_docs_2project__structure.html#autotoc_md240", null ],
      [ "Folder Structure", "md_docs_2project__structure.html#autotoc_md241", null ],
      [ "Architecture Diagram", "md_docs_2project__structure.html#autotoc_md242", null ],
      [ "Modules and Responsibilities", "md_docs_2project__structure.html#autotoc_md243", [
        [ "1. Core (App)", "md_docs_2project__structure.html#autotoc_md244", null ],
        [ "2. Post-Processing", "md_docs_2project__structure.html#autotoc_md245", null ],
        [ "3. Physical Rendering (PBR)", "md_docs_2project__structure.html#autotoc_md246", null ]
      ] ],
      [ "Improvements over Original Code", "md_docs_2project__structure.html#autotoc_md247", [
        [ "✅ Organization", "md_docs_2project__structure.html#autotoc_md248", null ],
        [ "✅ Reusability", "md_docs_2project__structure.html#autotoc_md249", null ],
        [ "✅ Maintainability", "md_docs_2project__structure.html#autotoc_md250", null ],
        [ "✅ Extensibility", "md_docs_2project__structure.html#autotoc_md251", null ],
        [ "✅ Memory Management", "md_docs_2project__structure.html#autotoc_md252", null ],
        [ "✅ Readability", "md_docs_2project__structure.html#autotoc_md253", null ]
      ] ],
      [ "Compilation and Execution", "md_docs_2project__structure.html#autotoc_md254", null ],
      [ "Controls", "md_docs_2project__structure.html#autotoc_md255", [
        [ "🖱️ Mouse Camera Control", "md_docs_2project__structure.html#autotoc_md256", null ],
        [ "⌨️ Keyboard Control", "md_docs_2project__structure.html#autotoc_md257", null ]
      ] ],
      [ "Dependencies", "md_docs_2project__structure.html#autotoc_md258", null ],
      [ "Technical Notes", "md_docs_2project__structure.html#autotoc_md259", null ]
    ] ],
    [ "Automatic Resource Management (RAII) in C", "md_docs_2raii__cleanup__guide.html", [
      [ "1. The Concept: RAII in C", "md_docs_2raii__cleanup__guide.html#autotoc_md262", [
        [ "The attribute cleanup Extension", "md_docs_2raii__cleanup__guide.html#autotoc_md263", null ]
      ] ],
      [ "2. Implementation in this Project", "md_docs_2raii__cleanup__guide.html#autotoc_md266", [
        [ "The Core Components", "md_docs_2raii__cleanup__guide.html#autotoc_md267", null ]
      ] ],
      [ "3. Benefits & Usage", "md_docs_2raii__cleanup__guide.html#autotoc_md268", [
        [ "Less Indentation", "md_docs_2raii__cleanup__guide.html#autotoc_md269", null ],
        [ "Safety with Early Returns", "md_docs_2raii__cleanup__guide.html#autotoc_md270", null ]
      ] ],
      [ "4. Real-world Example: src/pbr.c", "md_docs_2raii__cleanup__guide.html#autotoc_md272", null ],
      [ "5. Compatibility & Requirements", "md_docs_2raii__cleanup__guide.html#autotoc_md274", null ],
      [ "6. Satisfying Static Analyzers (Clang-Tidy)", "md_docs_2raii__cleanup__guide.html#autotoc_md277", [
        [ "The RAII_SATISFY_* Patterns", "md_docs_2raii__cleanup__guide.html#autotoc_md278", null ],
        [ "Why use this instead of // NOLINT?", "md_docs_2raii__cleanup__guide.html#autotoc_md279", null ]
      ] ],
      [ "7. Critical Perspectives & Limitations", "md_docs_2raii__cleanup__guide.html#autotoc_md281", [
        [ "\"Just Put RAII in C, Bro\" (Analysis)", "md_docs_2raii__cleanup__guide.html#autotoc_md282", null ]
      ] ],
      [ "Further Reading", "md_docs_2raii__cleanup__guide.html#autotoc_md284", null ]
    ] ],
    [ "User Guide: RenderDoc on Debian 13 (Intel Iris Xe)", "md_docs_2renderdoc__guide.html", [
      [ "1. Installation on Debian 13", "md_docs_2renderdoc__guide.html#autotoc_md286", null ],
      [ "2. Profiling Configuration", "md_docs_2renderdoc__guide.html#autotoc_md287", null ],
      [ "3. Capturing a Loading Event", "md_docs_2renderdoc__guide.html#autotoc_md288", null ],
      [ "4. Performance Analysis (Ground Truth)", "md_docs_2renderdoc__guide.html#autotoc_md289", null ],
      [ "5. Specific Intel / Mesa Tips", "md_docs_2renderdoc__guide.html#autotoc_md290", null ]
    ] ],
    [ "Shader Cross-GPU Compatibility Guidelines", "md_docs_2shader-cross-gpu-compatibility.html", [
      [ "Overview", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md292", null ],
      [ "Key Principles", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md293", [
        [ "1. Avoid Relying on Derivative Precision", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md294", null ],
        [ "2. Pre-Calculate and Store Values", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md295", null ],
        [ "3. Use Explicit Precision Qualifiers (Mobile/WebGL)", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md296", null ],
        [ "4. Clamp Intermediate Values", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md297", null ],
        [ "5. Avoid Fast Math Assumptions", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md298", null ]
      ] ],
      [ "Common Pitfalls", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md299", [
        [ "Pitfall 1: Texture LOD Calculation", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md300", null ],
        [ "Pitfall 2: Small Exponents in pow()", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md301", null ],
        [ "Pitfall 3: Derivatives in Divergent Branches", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md302", null ]
      ] ],
      [ "Testing Workflow", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md303", [
        [ "Tools", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md304", null ]
      ] ],
      [ "When to Use Derivatives", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md305", null ],
      [ "Implementation Example: Analytic Edge Smoothing", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md306", null ],
      [ "References", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md307", null ]
    ] ],
    [ "Post-Processing Shader Optimization", "md_docs_2shader__optimization.html", [
      [ "Overview", "md_docs_2shader__optimization.html#autotoc_md309", null ],
      [ "Build Modes", "md_docs_2shader__optimization.html#autotoc_md310", [
        [ "Debug Build (make debug / make run)", "md_docs_2shader__optimization.html#autotoc_md311", null ],
        [ "Release Build (make release / make run-release)", "md_docs_2shader__optimization.html#autotoc_md312", null ]
      ] ],
      [ "Technical Implementation", "md_docs_2shader__optimization.html#autotoc_md313", [
        [ "1. Shader Code (postprocess.frag)", "md_docs_2shader__optimization.html#autotoc_md314", null ],
        [ "2. Startup Logic (src/app.c)", "md_docs_2shader__optimization.html#autotoc_md315", null ],
        [ "3. Conditional Uniforms (src/postprocess.c)", "md_docs_2shader__optimization.html#autotoc_md316", null ]
      ] ],
      [ "Verification", "md_docs_2shader__optimization.html#autotoc_md317", null ]
    ] ],
    [ "Skybox Rendering Technique (Equirectangular)", "md_docs_2skybox__rendering.html", [
      [ "🔍 Technical Details", "md_docs_2skybox__rendering.html#autotoc_md322", [
        [ "Early-Z Optimization", "md_docs_2skybox__rendering.html#autotoc_md319", null ],
        [ "Optimization Diagram", "md_docs_2skybox__rendering.html#autotoc_md320", null ],
        [ "C Implementation (View Matrix)", "md_docs_2skybox__rendering.html#autotoc_md321", null ],
        [ "Mipmap Sampling", "md_docs_2skybox__rendering.html#autotoc_md323", null ],
        [ "Orientation Correction", "md_docs_2skybox__rendering.html#autotoc_md324", null ]
      ] ],
      [ "🎨 Full Workflow", "md_docs_2skybox__rendering.html#autotoc_md325", null ],
      [ "🌟 Advantages", "md_docs_2skybox__rendering.html#autotoc_md326", null ],
      [ "📝 Important Notes", "md_docs_2skybox__rendering.html#autotoc_md327", null ],
      [ "🔗 Python → C Equivalence", "md_docs_2skybox__rendering.html#autotoc_md328", [
        [ "Python (moderngl)", "md_docs_2skybox__rendering.html#autotoc_md329", null ],
        [ "C (cglm)", "md_docs_2skybox__rendering.html#autotoc_md330", null ]
      ] ]
    ] ],
    [ "Sphere Rendering: Transparency and Analytic Anti-Aliasing", "md_docs_2sphere__rendering.html", [
      [ "1. Sphere Sorting (Transparency)", "md_docs_2sphere__rendering.html#autotoc_md332", [
        [ "SphereSorter Architecture", "md_docs_2sphere__rendering.html#autotoc_md333", null ],
        [ "Rendering Pipeline", "md_docs_2sphere__rendering.html#autotoc_md334", null ],
        [ "Ray-Tracing Diagram", "md_docs_2sphere__rendering.html#autotoc_md335", null ]
      ] ],
      [ "2. Analytic Anti-Aliasing (\"Perfect AA\")", "md_docs_2sphere__rendering.html#autotoc_md337", [
        [ "The Aliasing Problem", "md_docs_2sphere__rendering.html#autotoc_md338", null ],
        [ "Solution: Discriminant Smoothing", "md_docs_2sphere__rendering.html#autotoc_md339", null ]
      ] ],
      [ "3. Configuration & Macros", "md_docs_2sphere__rendering.html#autotoc_md341", null ]
    ] ],
    [ "Texture Pipeline Optimization (Immutable Storage)", "md_docs_2texture__optimization.html", [
      [ "1. Problem: Mutable Storage", "md_docs_2texture__optimization.html#autotoc_md343", null ],
      [ "2. Solution: Immutable Storage", "md_docs_2texture__optimization.html#autotoc_md344", null ],
      [ "3. Implementation in Engine", "md_docs_2texture__optimization.html#autotoc_md345", [
        [ "Code (texture.c)", "md_docs_2texture__optimization.html#autotoc_md346", null ]
      ] ],
      [ "4. Alignment Constraint (Unpack Alignment)", "md_docs_2texture__optimization.html#autotoc_md347", null ],
      [ "5. Memory Layout Comparison", "md_docs_2texture__optimization.html#autotoc_md348", null ],
      [ "6. Performance Gains", "md_docs_2texture__optimization.html#autotoc_md349", null ]
    ] ],
    [ "Visual Testing & Regression Artifacts", "md_docs_2visual__testing__artifacts.html", [
      [ "The Core Challenge: Software vs. Hardware", "md_docs_2visual__testing__artifacts.html#autotoc_md351", [
        [ "1. Floating Point Precision", "md_docs_2visual__testing__artifacts.html#autotoc_md352", null ],
        [ "2. The \"Sphere Center\" Artifact", "md_docs_2visual__testing__artifacts.html#autotoc_md353", null ]
      ] ],
      [ "PBR Engine Evolution", "md_docs_2visual__testing__artifacts.html#autotoc_md354", [
        [ "1. Multiple Scattering (Kulla-Conty)", "md_docs_2visual__testing__artifacts.html#autotoc_md355", null ],
        [ "2. Analytic Roughness Clamping", "md_docs_2visual__testing__artifacts.html#autotoc_md356", null ]
      ] ],
      [ "Guidelines for Reference Updates", "md_docs_2visual__testing__artifacts.html#autotoc_md357", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Variables", "functions_vars.html", "functions_vars" ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", "globals_vars" ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"adaptive__sampler_8c.html",
"camera_8h.html#adb4c37062fe8dfb2f1958406eb8863ed",
"globals_q.html",
"md_docs_2build.html#autotoc_md21",
"md_docs_2shader__optimization.html#autotoc_md309",
"postprocess_8frag.html#ab6364d5a7236e49af86d4f2f53611799",
"structApp.html#a21a8389dbe19e64507b78e3658cb17e6",
"structLoadedBuffer.html#aab42ed78eb04e97315cb610877f4e0ae",
"ui_8c.html#a3c924c9b9fc2b783756213d65e6d28d8"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';