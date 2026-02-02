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
    [ "Effet de Banding (Quantisation Couleurs)", "md_docs_2banding.html", [
      [ "🚀 Fonctionnement Rapide", "md_docs_2banding.html#autotoc_md20", null ],
      [ "🎨 Les 5 Styles Artistiques", "md_docs_2banding.html#autotoc_md22", [
        [ "1. Pop Art (Linear)", "md_docs_2banding.html#autotoc_md23", null ],
        [ "2. Retro Computing (Dithered)", "md_docs_2banding.html#autotoc_md24", null ],
        [ "3. Analog (Perceptual)", "md_docs_2banding.html#autotoc_md25", null ],
        [ "4. CGA/VGA Style (Channel)", "md_docs_2banding.html#autotoc_md26", null ],
        [ "5. Blueprint (Luminance)", "md_docs_2banding.html#autotoc_md27", null ]
      ] ],
      [ "⚙️ Paramètres (PostProcessPreset)", "md_docs_2banding.html#autotoc_md29", null ],
      [ "🛠 Intégration Technique", "md_docs_2banding.html#autotoc_md31", null ]
    ] ],
    [ "Build System and Dependency Management", "md_docs_2build.html", [
      [ "Build Requirements", "md_docs_2build.html#autotoc_md33", null ],
      [ "Distrobox Integration", "md_docs_2build.html#autotoc_md34", [
        [ "Common Commands", "md_docs_2build.html#autotoc_md35", null ]
      ] ],
      [ "Offline Build Support", "md_docs_2build.html#autotoc_md36", [
        [ "1. Preparation (Online)", "md_docs_2build.html#autotoc_md37", null ],
        [ "2. Building (Offline)", "md_docs_2build.html#autotoc_md38", null ],
        [ "3. Testing the Offline Mode", "md_docs_2build.html#autotoc_md39", null ]
      ] ],
      [ "Dependency Management", "md_docs_2build.html#autotoc_md40", [
        [ "cglm (OpenGL Mathematics for C)", "md_docs_2build.html#autotoc_md41", null ],
        [ "GLAD (OpenGL Loader Generator)", "md_docs_2build.html#autotoc_md42", null ],
        [ "GLFW", "md_docs_2build.html#autotoc_md43", null ],
        [ "Unity", "md_docs_2build.html#autotoc_md44", null ],
        [ "cJSON", "md_docs_2build.html#autotoc_md45", null ]
      ] ],
      [ "Folder Structure", "md_docs_2build.html#autotoc_md46", null ],
      [ "Fast Parallel Builds", "md_docs_2build.html#autotoc_md47", null ],
      [ "Logging System", "md_docs_2build.html#autotoc_md48", null ],
      [ "Automated Dependencies", "md_docs_2build.html#autotoc_md49", null ],
      [ "Folder Structure", "md_docs_2build.html#autotoc_md50", null ]
    ] ],
    [ "Cubemap Seam Resolution", "md_docs_2cubemap__seam__resolution.html", [
      [ "🔍 Identified Problem", "md_docs_2cubemap__seam__resolution.html#autotoc_md52", null ],
      [ "🏁 Definitive Solution: Equirectangular Mapping", "md_docs_2cubemap__seam__resolution.html#autotoc_md53", [
        [ "Why?", "md_docs_2cubemap__seam__resolution.html#autotoc_md54", null ],
        [ "Visual Comparison", "md_docs_2cubemap__seam__resolution.html#autotoc_md55", null ],
        [ "Comparison: Cubemap vs Equirectangular", "md_docs_2cubemap__seam__resolution.html#autotoc_md56", null ],
        [ "Software Implementation", "md_docs_2cubemap__seam__resolution.html#autotoc_md57", null ],
        [ "Conclusion", "md_docs_2cubemap__seam__resolution.html#autotoc_md58", null ]
      ] ]
    ] ],
    [ "Docker Support", "md_docs_2docker.html", [
      [ "Prerequisites", "md_docs_2docker.html#autotoc_md60", null ],
      [ "Quick Start", "md_docs_2docker.html#autotoc_md61", [
        [ "Build the Image", "md_docs_2docker.html#autotoc_md62", null ],
        [ "Run the Application", "md_docs_2docker.html#autotoc_md63", null ]
      ] ],
      [ "Architecture", "md_docs_2docker.html#autotoc_md64", [
        [ "Multi-Stage Build", "md_docs_2docker.html#autotoc_md65", [
          [ "Stage 1: Builder (fedora:41)", "md_docs_2docker.html#autotoc_md66", null ],
          [ "Stage 2: Runtime (fedora:41)", "md_docs_2docker.html#autotoc_md67", null ]
        ] ],
        [ "Headless Rendering with Xvfb", "md_docs_2docker.html#autotoc_md68", null ],
        [ "Build Cache Optimization", "md_docs_2docker.html#autotoc_md69", null ]
      ] ],
      [ "Makefile Targets", "md_docs_2docker.html#autotoc_md70", [
        [ "Build Targets", "md_docs_2docker.html#autotoc_md71", null ],
        [ "Maintenance Targets", "md_docs_2docker.html#autotoc_md72", null ]
      ] ],
      [ "Advanced Usage", "md_docs_2docker.html#autotoc_md73", [
        [ "Custom Container Engine", "md_docs_2docker.html#autotoc_md74", null ],
        [ "Incremental Builds", "md_docs_2docker.html#autotoc_md75", null ],
        [ "Running with Host X11", "md_docs_2docker.html#autotoc_md76", null ]
      ] ],
      [ "Troubleshooting", "md_docs_2docker.html#autotoc_md77", [
        [ "Build Cache Not Working", "md_docs_2docker.html#autotoc_md78", null ],
        [ "Xvfb Fails to Start", "md_docs_2docker.html#autotoc_md79", null ],
        [ "X11 Permission Denied", "md_docs_2docker.html#autotoc_md80", null ]
      ] ],
      [ "CI/CD Integration", "md_docs_2docker.html#autotoc_md81", null ]
    ] ],
    [ "Documentation Staging & Preview Guide", "md_docs_2docs__staging__guide.html", [
      [ "How it Works", "md_docs_2docs__staging__guide.html#autotoc_md83", null ],
      [ "Setup Instructions", "md_docs_2docs__staging__guide.html#autotoc_md84", [
        [ "1. Obtain a Surge Token", "md_docs_2docs__staging__guide.html#autotoc_md85", null ],
        [ "2. Configure GitHub Secrets", "md_docs_2docs__staging__guide.html#autotoc_md86", null ]
      ] ],
      [ "Workflow Integration", "md_docs_2docs__staging__guide.html#autotoc_md87", null ],
      [ "Maintenance", "md_docs_2docs__staging__guide.html#autotoc_md88", [
        [ "Clearing Previews", "md_docs_2docs__staging__guide.html#autotoc_md89", null ],
        [ "Security", "md_docs_2docs__staging__guide.html#autotoc_md90", null ]
      ] ]
    ] ],
    [ "Doxygen Customization & Modernization", "md_docs_2doxygen__customization.html", [
      [ "🎨 Theme: Doxygen-Awesome", "md_docs_2doxygen__customization.html#autotoc_md93", [
        [ "Integrated Components", "md_docs_2doxygen__customization.html#autotoc_md94", null ]
      ] ],
      [ "💻 Advanced Syntax Highlighting", "md_docs_2doxygen__customization.html#autotoc_md96", [
        [ "1. Shader Support", "md_docs_2doxygen__customization.html#autotoc_md97", null ],
        [ "2. Indentation Preservation Fix", "md_docs_2doxygen__customization.html#autotoc_md98", null ]
      ] ],
      [ "📊 \"Suckless-Modern Ghost\" Diagrams", "md_docs_2doxygen__customization.html#autotoc_md100", [
        [ "1. Engine Configuration", "md_docs_2doxygen__customization.html#autotoc_md101", null ],
        [ "2. Design Tokens", "md_docs_2doxygen__customization.html#autotoc_md102", null ]
      ] ],
      [ "🔧 Core Configuration (Doxyfile)", "md_docs_2doxygen__customization.html#autotoc_md104", null ],
      [ "🛠️ Maintenance", "md_docs_2doxygen__customization.html#autotoc_md106", null ]
    ] ],
    [ "Global Exposure Management Analysis", "md_docs_2exposure__analysis.html", [
      [ "1. Definition and Data Structure", "md_docs_2exposure__analysis.html#autotoc_md108", [
        [ "CPU Side (include/postprocess.h, src/postprocess.c)", "md_docs_2exposure__analysis.html#autotoc_md109", null ],
        [ "GPU Side (Uniforms & Textures)", "md_docs_2exposure__analysis.html#autotoc_md110", null ]
      ] ],
      [ "2. Runpaths and Usage Modes", "md_docs_2exposure__analysis.html#autotoc_md112", [
        [ "Mode A: Automatic Exposure (POSTFX_AUTO_EXPOSURE)", "md_docs_2exposure__analysis.html#autotoc_md113", null ],
        [ "Mode B: Manual Exposure (POSTFX_EXPOSURE)", "md_docs_2exposure__analysis.html#autotoc_md114", null ],
        [ "Mode C: No Exposure", "md_docs_2exposure__analysis.html#autotoc_md115", null ]
      ] ],
      [ "3. Integration in the Pipeline (postprocess.frag)", "md_docs_2exposure__analysis.html#autotoc_md117", null ],
      [ "4. Pre-calculation and Optimizations", "md_docs_2exposure__analysis.html#autotoc_md118", null ],
      [ "Synthetic Summary", "md_docs_2exposure__analysis.html#autotoc_md119", [
        [ "Adaptation Pipeline", "md_docs_2exposure__analysis.html#autotoc_md120", null ]
      ] ]
    ] ],
    [ "FXAA 3.11 Implementation & Optimizations", "md_docs_2FXAA.html", [
      [ "Overview", "md_docs_2FXAA.html#autotoc_md122", null ],
      [ "Key Optimizations", "md_docs_2FXAA.html#autotoc_md123", [
        [ "1. Luma-in-Alpha (Bandwidth Optimization)", "md_docs_2FXAA.html#autotoc_md124", [
          [ "Pipeline Data Flow", "md_docs_2FXAA.html#autotoc_md125", null ]
        ] ],
        [ "2. sRGB / Gamma Correctness", "md_docs_2FXAA.html#autotoc_md126", null ],
        [ "3. Dual Mode (Quality vs. Performance)", "md_docs_2FXAA.html#autotoc_md127", null ],
        [ "4. Non-Linear Search (Quality Mode)", "md_docs_2FXAA.html#autotoc_md128", null ]
      ] ],
      [ "Configuration", "md_docs_2FXAA.html#autotoc_md129", null ],
      [ "Debugging", "md_docs_2FXAA.html#autotoc_md130", null ]
    ] ],
    [ "GPU Rendering Synchronization: Intel vs NVIDIA", "md_docs_2gpu-rendering-synchronization.html", [
      [ "Executive Summary", "md_docs_2gpu-rendering-synchronization.html#autotoc_md132", null ],
      [ "Visual Comparison", "md_docs_2gpu-rendering-synchronization.html#autotoc_md133", null ],
      [ "Root Causes Identified", "md_docs_2gpu-rendering-synchronization.html#autotoc_md134", [
        [ "Issue 1: FXAA Luminance Recalculation", "md_docs_2gpu-rendering-synchronization.html#autotoc_md135", null ],
        [ "Issue 2: Derivative-Based Roughness Clamping", "md_docs_2gpu-rendering-synchronization.html#autotoc_md136", null ]
      ] ],
      [ "Why Derivatives Differ", "md_docs_2gpu-rendering-synchronization.html#autotoc_md137", null ],
      [ "Trade-offs", "md_docs_2gpu-rendering-synchronization.html#autotoc_md138", [
        [ "Lost", "md_docs_2gpu-rendering-synchronization.html#autotoc_md139", null ],
        [ "Gained", "md_docs_2gpu-rendering-synchronization.html#autotoc_md140", null ]
      ] ],
      [ "Derivatives vs Analytic Performance", "md_docs_2gpu-rendering-synchronization.html#autotoc_md141", null ],
      [ "Validation Results", "md_docs_2gpu-rendering-synchronization.html#autotoc_md142", [
        [ "Visual Inspection", "md_docs_2gpu-rendering-synchronization.html#autotoc_md143", null ],
        [ "Reference Metrics (FXAA Synthetic Test)", "md_docs_2gpu-rendering-synchronization.html#autotoc_md144", null ]
      ] ],
      [ "Files Modified", "md_docs_2gpu-rendering-synchronization.html#autotoc_md145", null ],
      [ "References", "md_docs_2gpu-rendering-synchronization.html#autotoc_md146", null ]
    ] ],
    [ "IBL Optimization Strategies", "md_docs_2ibl__architecture__ideas.html", [
      [ "1. Problem Analysis", "md_docs_2ibl__architecture__ideas.html#autotoc_md148", null ],
      [ "2. Proposed Solutions", "md_docs_2ibl__architecture__ideas.html#autotoc_md149", [
        [ "A. Progressive IBL (Temporal Amortization)", "md_docs_2ibl__architecture__ideas.html#autotoc_md150", null ],
        [ "B. Adaptive Sample Counting", "md_docs_2ibl__architecture__ideas.html#autotoc_md151", null ],
        [ "C. Resolution Capping", "md_docs_2ibl__architecture__ideas.html#autotoc_md152", null ],
        [ "D. Tiled Dispatching", "md_docs_2ibl__architecture__ideas.html#autotoc_md153", null ],
        [ "E. Instant Placeholder (The \"Fast Path\")", "md_docs_2ibl__architecture__ideas.html#autotoc_md154", null ]
      ] ],
      [ "3. Implementation Roadmap", "md_docs_2ibl__architecture__ideas.html#autotoc_md155", null ]
    ] ],
    [ "Mouse Camera Control", "md_docs_2mouse__camera__control.html", [
      [ "Overview", "md_docs_2mouse__camera__control.html#autotoc_md157", null ],
      [ "Features", "md_docs_2mouse__camera__control.html#autotoc_md158", null ],
      [ "Architecture", "md_docs_2mouse__camera__control.html#autotoc_md159", [
        [ "Data Flow", "md_docs_2mouse__camera__control.html#autotoc_md160", null ]
      ] ],
      [ "Implementation Details", "md_docs_2mouse__camera__control.html#autotoc_md161", [
        [ "Rotation Calculation (Euler Angles)", "md_docs_2mouse__camera__control.html#autotoc_md162", null ],
        [ "Input Handling", "md_docs_2mouse__camera__control.html#autotoc_md163", null ]
      ] ],
      [ "Usage", "md_docs_2mouse__camera__control.html#autotoc_md164", null ]
    ] ],
    [ "NVIDIA OpenGL Support: Launch, Stability & Optimizations", "md_docs_2nvidia__optimizations.html", [
      [ "I. Startup & Stability Fixes (Critical)", "md_docs_2nvidia__optimizations.html#autotoc_md167", [
        [ "1. Robust Texture Mipmap Allocation (Error 0x501)", "md_docs_2nvidia__optimizations.html#autotoc_md168", null ],
        [ "2. Object Labeling & Initialization Sequence (Error 1282)", "md_docs_2nvidia__optimizations.html#autotoc_md169", null ]
      ] ],
      [ "II. Performance & Warning Cleanups", "md_docs_2nvidia__optimizations.html#autotoc_md171", [
        [ "1. Dummy Texture Strategy (Unit Binding Cleanup)", "md_docs_2nvidia__optimizations.html#autotoc_md172", null ],
        [ "2. VAO State Reconciliation (Shader Recompilation)", "md_docs_2nvidia__optimizations.html#autotoc_md173", null ],
        [ "3. Efficient Memory Placement (Buffer Movement)", "md_docs_2nvidia__optimizations.html#autotoc_md174", null ]
      ] ]
    ] ],
    [ "OpenGL Stability & Performance Cleanup (2026-01-28)", "md_docs_2opengl__cleanup.html", [
      [ "1. Stability & Rendering Errors", "md_docs_2opengl__cleanup.html#autotoc_md176", [
        [ "IBL: Mipmap Allocation (0x501)", "md_docs_2opengl__cleanup.html#autotoc_md177", null ],
        [ "Object Labeling (1282)", "md_docs_2opengl__cleanup.html#autotoc_md178", null ],
        [ "Fallback Protection (0x502)", "md_docs_2opengl__cleanup.html#autotoc_md179", null ]
      ] ],
      [ "2. Performance Optimizations (NVIDIA)", "md_docs_2opengl__cleanup.html#autotoc_md180", [
        [ "Buffer Migration (0x20072)", "md_docs_2opengl__cleanup.html#autotoc_md181", null ],
        [ "Resize Bridge (0x20084)", "md_docs_2opengl__cleanup.html#autotoc_md182", null ],
        [ "Shader Recompilation (0x20092)", "md_docs_2opengl__cleanup.html#autotoc_md183", null ]
      ] ],
      [ "3. Residual Warning 0x20092 Analysis", "md_docs_2opengl__cleanup.html#autotoc_md184", null ]
    ] ],
    [ "Photographic Standards for Real-Time Rendering", "md_docs_2photographic__standards.html", [
      [ "📸 The 18% Middle Gray - The Cornerstone", "md_docs_2photographic__standards.html#autotoc_md187", [
        [ "Photographic Origin", "md_docs_2photographic__standards.html#autotoc_md188", null ],
        [ "Why 18%?", "md_docs_2photographic__standards.html#autotoc_md189", null ],
        [ "Applications", "md_docs_2photographic__standards.html#autotoc_md190", null ]
      ] ],
      [ "🎚️ Photographic Values Scale", "md_docs_2photographic__standards.html#autotoc_md191", [
        [ "Exposure Values (EV)", "md_docs_2photographic__standards.html#autotoc_md192", null ],
        [ "Visual EV Scale", "md_docs_2photographic__standards.html#autotoc_md193", null ],
        [ "Auto-Exposure Formula", "md_docs_2photographic__standards.html#autotoc_md194", null ]
      ] ],
      [ "🌈 Material Reflectance Values", "md_docs_2photographic__standards.html#autotoc_md195", [
        [ "Standard Albedos (Physically Based)", "md_docs_2photographic__standards.html#autotoc_md196", null ]
      ] ],
      [ "📊 Tone Mapping - Standard Curves", "md_docs_2photographic__standards.html#autotoc_md197", [
        [ "1. Linear (Naive)", "md_docs_2photographic__standards.html#autotoc_md198", null ],
        [ "2. Reinhard (Simple)", "md_docs_2photographic__standards.html#autotoc_md199", null ],
        [ "3. Uncharted 2 / Hable (Filmic)", "md_docs_2photographic__standards.html#autotoc_md200", null ],
        [ "4. ACES (Academy Color Encoding System)", "md_docs_2photographic__standards.html#autotoc_md201", null ]
      ] ],
      [ "🎮 Recommended Values for Games", "md_docs_2photographic__standards.html#autotoc_md202", [
        [ "Auto-Exposure Settings", "md_docs_2photographic__standards.html#autotoc_md203", null ],
        [ "Examples by Genre", "md_docs_2photographic__standards.html#autotoc_md204", null ]
      ] ],
      [ "🔧 Interesting Alternative Values", "md_docs_2photographic__standards.html#autotoc_md205", [
        [ "Key Value Alternatives", "md_docs_2photographic__standards.html#autotoc_md206", null ],
        [ "Middle Gray in sRGB", "md_docs_2photographic__standards.html#autotoc_md207", null ]
      ] ],
      [ "📐 Useful Formulas", "md_docs_2photographic__standards.html#autotoc_md208", [
        [ "Luminance Conversion", "md_docs_2photographic__standards.html#autotoc_md209", null ],
        [ "EV ↔ Luminance Conversion", "md_docs_2photographic__standards.html#autotoc_md210", null ],
        [ "Temporal Adaptation", "md_docs_2photographic__standards.html#autotoc_md211", null ]
      ] ],
      [ "🎨 Practical Workflow", "md_docs_2photographic__standards.html#autotoc_md212", [
        [ "1. Initial Calibration", "md_docs_2photographic__standards.html#autotoc_md213", null ],
        [ "2. Test with Type Scenes", "md_docs_2photographic__standards.html#autotoc_md214", null ],
        [ "3. Artistic Tweaking", "md_docs_2photographic__standards.html#autotoc_md215", null ]
      ] ],
      [ "📚 Recommended Resources", "md_docs_2photographic__standards.html#autotoc_md216", null ],
      [ "✨ Bonus: Exotic Values", "md_docs_2photographic__standards.html#autotoc_md217", [
        [ "Lunar Scenes", "md_docs_2photographic__standards.html#autotoc_md218", null ],
        [ "Underwater", "md_docs_2photographic__standards.html#autotoc_md219", null ],
        [ "Space", "md_docs_2photographic__standards.html#autotoc_md220", null ]
      ] ]
    ] ],
    [ "Post-Processing UBO Architecture", "md_docs_2postprocess__ubo__architecture.html", [
      [ "1. Overview", "md_docs_2postprocess__ubo__architecture.html#autotoc_md222", null ],
      [ "2. Critical Constraints: std140 Layout", "md_docs_2postprocess__ubo__architecture.html#autotoc_md223", [
        [ "std140 Alignment Rules (Simplified)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md224", null ],
        [ "The \"Array vs Scalar\" Padding Trap", "md_docs_2postprocess__ubo__architecture.html#autotoc_md225", null ],
        [ "Memory Visualization (std140)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md226", null ]
      ] ],
      [ "3. Data Structure", "md_docs_2postprocess__ubo__architecture.html#autotoc_md227", [
        [ "C Structure (postprocess.h)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md228", null ],
        [ "GLSL Block (ubo.glsl)", "md_docs_2postprocess__ubo__architecture.html#autotoc_md229", null ]
      ] ],
      [ "4. Adding a New Parameter", "md_docs_2postprocess__ubo__architecture.html#autotoc_md230", null ],
      [ "5. Performance", "md_docs_2postprocess__ubo__architecture.html#autotoc_md231", null ]
    ] ],
    [ "Profiling Guide: ApiTrace & Trace Analysis", "md_docs_2profiling__guide.html", [
      [ "Why use this workflow?", "md_docs_2profiling__guide.html#autotoc_md233", null ],
      [ "1. Prerequisites", "md_docs_2profiling__guide.html#autotoc_md235", null ],
      [ "2. Generating a Trace", "md_docs_2profiling__guide.html#autotoc_md237", null ],
      [ "3. High-Level Performance Analysis", "md_docs_2profiling__guide.html#autotoc_md239", [
        [ "Understanding the Metrics", "md_docs_2profiling__guide.html#autotoc_md240", null ]
      ] ],
      [ "4. Advanced Tool Usage", "md_docs_2profiling__guide.html#autotoc_md242", [
        [ "Script Logic", "md_docs_2profiling__guide.html#autotoc_md243", null ]
      ] ],
      [ "5. Coding for Profiling", "md_docs_2profiling__guide.html#autotoc_md245", null ],
      [ "6. Developing the Tool", "md_docs_2profiling__guide.html#autotoc_md247", null ]
    ] ],
    [ "Progressive & Asynchronous IBL Architecture", "md_docs_2progressive__ibl.html", [
      [ "1. Overview", "md_docs_2progressive__ibl.html#autotoc_md249", [
        [ "The Pipeline", "md_docs_2progressive__ibl.html#autotoc_md250", null ]
      ] ],
      [ "2. \"Slicing\" Strategy", "md_docs_2progressive__ibl.html#autotoc_md252", [
        [ "2.1 Overlap Protection (Crucial)", "md_docs_2progressive__ibl.html#autotoc_md253", null ]
      ] ],
      [ "3. Optimized Configuration (Adaptive Slicing)", "md_docs_2progressive__ibl.html#autotoc_md255", [
        [ "A. Irradiance Map (64x64)", "md_docs_2progressive__ibl.html#autotoc_md256", null ],
        [ "B. Specular Map (512x512)", "md_docs_2progressive__ibl.html#autotoc_md257", null ]
      ] ],
      [ "4. Global Performance", "md_docs_2progressive__ibl.html#autotoc_md259", null ],
      [ "5. Key Files", "md_docs_2progressive__ibl.html#autotoc_md260", null ]
    ] ],
    [ "Project Structure: Refactored Icosphere", "md_docs_2project__structure.html", [
      [ "Modular Architecture", "md_docs_2project__structure.html#autotoc_md262", null ],
      [ "Folder Structure", "md_docs_2project__structure.html#autotoc_md263", null ],
      [ "Architecture Diagram", "md_docs_2project__structure.html#autotoc_md264", null ],
      [ "Modules and Responsibilities", "md_docs_2project__structure.html#autotoc_md265", [
        [ "1. Core (App)", "md_docs_2project__structure.html#autotoc_md266", null ],
        [ "2. Post-Processing", "md_docs_2project__structure.html#autotoc_md267", null ],
        [ "3. Physical Rendering (PBR)", "md_docs_2project__structure.html#autotoc_md268", null ]
      ] ],
      [ "Improvements over Original Code", "md_docs_2project__structure.html#autotoc_md269", [
        [ "✅ Organization", "md_docs_2project__structure.html#autotoc_md270", null ],
        [ "✅ Reusability", "md_docs_2project__structure.html#autotoc_md271", null ],
        [ "✅ Maintainability", "md_docs_2project__structure.html#autotoc_md272", null ],
        [ "✅ Extensibility", "md_docs_2project__structure.html#autotoc_md273", null ],
        [ "✅ Memory Management", "md_docs_2project__structure.html#autotoc_md274", null ],
        [ "✅ Readability", "md_docs_2project__structure.html#autotoc_md275", null ]
      ] ],
      [ "Compilation and Execution", "md_docs_2project__structure.html#autotoc_md276", null ],
      [ "Controls", "md_docs_2project__structure.html#autotoc_md277", [
        [ "🖱️ Mouse Camera Control", "md_docs_2project__structure.html#autotoc_md278", null ],
        [ "⌨️ Keyboard Control", "md_docs_2project__structure.html#autotoc_md279", null ]
      ] ],
      [ "Dependencies", "md_docs_2project__structure.html#autotoc_md280", null ],
      [ "Technical Notes", "md_docs_2project__structure.html#autotoc_md281", null ]
    ] ],
    [ "Automatic Resource Management (RAII) in C", "md_docs_2raii__cleanup__guide.html", [
      [ "1. The Concept: RAII in C", "md_docs_2raii__cleanup__guide.html#autotoc_md284", [
        [ "The attribute cleanup Extension", "md_docs_2raii__cleanup__guide.html#autotoc_md285", null ]
      ] ],
      [ "2. Implementation in this Project", "md_docs_2raii__cleanup__guide.html#autotoc_md288", [
        [ "The Core Components", "md_docs_2raii__cleanup__guide.html#autotoc_md289", null ]
      ] ],
      [ "3. Benefits & Usage", "md_docs_2raii__cleanup__guide.html#autotoc_md291", [
        [ "Less Indentation", "md_docs_2raii__cleanup__guide.html#autotoc_md292", null ],
        [ "Safety with Early Returns", "md_docs_2raii__cleanup__guide.html#autotoc_md293", null ]
      ] ],
      [ "4. Real-world Example: src/pbr.c", "md_docs_2raii__cleanup__guide.html#autotoc_md295", null ],
      [ "5. Compatibility & Requirements", "md_docs_2raii__cleanup__guide.html#autotoc_md297", null ],
      [ "6. Satisfying Static Analyzers (Clang-Tidy)", "md_docs_2raii__cleanup__guide.html#autotoc_md300", [
        [ "The RAII_SATISFY_* Patterns", "md_docs_2raii__cleanup__guide.html#autotoc_md301", null ],
        [ "Why use this instead of // NOLINT?", "md_docs_2raii__cleanup__guide.html#autotoc_md302", null ]
      ] ],
      [ "7. Critical Perspectives & Limitations", "md_docs_2raii__cleanup__guide.html#autotoc_md304", [
        [ "\"Just Put RAII in C, Bro\" (Analysis)", "md_docs_2raii__cleanup__guide.html#autotoc_md305", null ]
      ] ],
      [ "Further Reading", "md_docs_2raii__cleanup__guide.html#autotoc_md307", null ]
    ] ],
    [ "User Guide: RenderDoc on Debian 13 (Intel Iris Xe)", "md_docs_2renderdoc__guide.html", [
      [ "1. Installation on Debian 13", "md_docs_2renderdoc__guide.html#autotoc_md309", null ],
      [ "2. Profiling Configuration", "md_docs_2renderdoc__guide.html#autotoc_md310", null ],
      [ "3. Capturing a Loading Event", "md_docs_2renderdoc__guide.html#autotoc_md311", null ],
      [ "4. Performance Analysis (Ground Truth)", "md_docs_2renderdoc__guide.html#autotoc_md312", null ],
      [ "5. Specific Intel / Mesa Tips", "md_docs_2renderdoc__guide.html#autotoc_md313", null ]
    ] ],
    [ "Shader Cross-GPU Compatibility Guidelines", "md_docs_2shader-cross-gpu-compatibility.html", [
      [ "Overview", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md315", null ],
      [ "Key Principles", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md316", [
        [ "1. Avoid Relying on Derivative Precision", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md317", null ],
        [ "2. Pre-Calculate and Store Values", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md318", null ],
        [ "3. Use Explicit Precision Qualifiers (Mobile/WebGL)", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md319", null ],
        [ "4. Clamp Intermediate Values", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md320", null ],
        [ "5. Avoid Fast Math Assumptions", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md321", null ]
      ] ],
      [ "Common Pitfalls", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md322", [
        [ "Pitfall 1: Texture LOD Calculation", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md323", null ],
        [ "Pitfall 2: Small Exponents in pow()", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md324", null ],
        [ "Pitfall 3: Derivatives in Divergent Branches", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md325", null ]
      ] ],
      [ "Testing Workflow", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md326", [
        [ "Tools", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md327", null ]
      ] ],
      [ "When to Use Derivatives", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md328", null ],
      [ "Implementation Example: Analytic Edge Smoothing", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md329", null ],
      [ "References", "md_docs_2shader-cross-gpu-compatibility.html#autotoc_md330", null ]
    ] ],
    [ "Post-Processing Shader Optimization", "md_docs_2shader__optimization.html", [
      [ "Overview", "md_docs_2shader__optimization.html#autotoc_md332", null ],
      [ "Build Modes", "md_docs_2shader__optimization.html#autotoc_md333", [
        [ "Debug Build (make debug / make run)", "md_docs_2shader__optimization.html#autotoc_md334", null ],
        [ "Release Build (make release / make run-release)", "md_docs_2shader__optimization.html#autotoc_md335", null ]
      ] ],
      [ "Technical Implementation", "md_docs_2shader__optimization.html#autotoc_md336", [
        [ "1. Shader Code (postprocess.frag)", "md_docs_2shader__optimization.html#autotoc_md337", null ],
        [ "2. Startup Logic (src/app.c)", "md_docs_2shader__optimization.html#autotoc_md338", null ],
        [ "3. Conditional Uniforms (src/postprocess.c)", "md_docs_2shader__optimization.html#autotoc_md339", null ]
      ] ],
      [ "Verification", "md_docs_2shader__optimization.html#autotoc_md340", null ]
    ] ],
    [ "Skybox Rendering Technique (Equirectangular)", "md_docs_2skybox__rendering.html", [
      [ "🔍 Technical Details", "md_docs_2skybox__rendering.html#autotoc_md345", [
        [ "Early-Z Optimization", "md_docs_2skybox__rendering.html#autotoc_md342", null ],
        [ "Optimization Diagram", "md_docs_2skybox__rendering.html#autotoc_md343", null ],
        [ "C Implementation (View Matrix)", "md_docs_2skybox__rendering.html#autotoc_md344", null ],
        [ "Mipmap Sampling", "md_docs_2skybox__rendering.html#autotoc_md346", null ],
        [ "Orientation Correction", "md_docs_2skybox__rendering.html#autotoc_md347", null ]
      ] ],
      [ "🎨 Full Workflow", "md_docs_2skybox__rendering.html#autotoc_md348", null ],
      [ "🌟 Advantages", "md_docs_2skybox__rendering.html#autotoc_md349", null ],
      [ "📝 Important Notes", "md_docs_2skybox__rendering.html#autotoc_md350", null ],
      [ "🔗 Python → C Equivalence", "md_docs_2skybox__rendering.html#autotoc_md351", [
        [ "Python (moderngl)", "md_docs_2skybox__rendering.html#autotoc_md352", null ],
        [ "C (cglm)", "md_docs_2skybox__rendering.html#autotoc_md353", null ]
      ] ]
    ] ],
    [ "Sphere Rendering: Transparency and Analytic Anti-Aliasing", "md_docs_2sphere__rendering.html", [
      [ "1. Sphere Sorting (Transparency)", "md_docs_2sphere__rendering.html#autotoc_md355", [
        [ "SphereSorter Architecture", "md_docs_2sphere__rendering.html#autotoc_md356", null ],
        [ "Rendering Pipeline", "md_docs_2sphere__rendering.html#autotoc_md357", null ],
        [ "Ray-Tracing Diagram", "md_docs_2sphere__rendering.html#autotoc_md358", null ]
      ] ],
      [ "2. Analytic Anti-Aliasing (\"Perfect AA\")", "md_docs_2sphere__rendering.html#autotoc_md360", [
        [ "The Aliasing Problem", "md_docs_2sphere__rendering.html#autotoc_md361", null ],
        [ "Solution: Discriminant Smoothing", "md_docs_2sphere__rendering.html#autotoc_md362", null ]
      ] ],
      [ "3. Configuration & Macros", "md_docs_2sphere__rendering.html#autotoc_md364", null ]
    ] ],
    [ "Texture Pipeline Optimization (Immutable Storage)", "md_docs_2texture__optimization.html", [
      [ "1. Problem: Mutable Storage", "md_docs_2texture__optimization.html#autotoc_md366", null ],
      [ "2. Solution: Immutable Storage", "md_docs_2texture__optimization.html#autotoc_md367", null ],
      [ "3. Implementation in Engine", "md_docs_2texture__optimization.html#autotoc_md368", [
        [ "Code (texture.c)", "md_docs_2texture__optimization.html#autotoc_md369", null ]
      ] ],
      [ "4. Alignment Constraint (Unpack Alignment)", "md_docs_2texture__optimization.html#autotoc_md370", null ],
      [ "5. Memory Layout Comparison", "md_docs_2texture__optimization.html#autotoc_md371", null ],
      [ "6. Performance Gains", "md_docs_2texture__optimization.html#autotoc_md372", null ]
    ] ],
    [ "Visual Testing & Regression Artifacts", "md_docs_2visual__testing__artifacts.html", [
      [ "The Core Challenge: Software vs. Hardware", "md_docs_2visual__testing__artifacts.html#autotoc_md374", [
        [ "1. Floating Point Precision", "md_docs_2visual__testing__artifacts.html#autotoc_md375", null ],
        [ "2. The \"Sphere Center\" Artifact", "md_docs_2visual__testing__artifacts.html#autotoc_md376", null ]
      ] ],
      [ "PBR Engine Evolution", "md_docs_2visual__testing__artifacts.html#autotoc_md377", [
        [ "1. Multiple Scattering (Kulla-Conty)", "md_docs_2visual__testing__artifacts.html#autotoc_md378", null ],
        [ "2. Analytic Roughness Clamping", "md_docs_2visual__testing__artifacts.html#autotoc_md379", null ]
      ] ],
      [ "Guidelines for Reference Updates", "md_docs_2visual__testing__artifacts.html#autotoc_md380", null ]
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
"camera_8h.html#aaa490f87bbe7d1b73db8c6f80987aa58",
"globals_l.html",
"md_docs_2async__loader.html#autotoc_md16",
"md_docs_2renderdoc__guide.html",
"postprocess_8c.html#a7dfdbd52fd26553208453d418e3ecca4",
"spmap_8glsl.html#a4d6b00b31639e83ae9b7ce8ceee2aa60",
"structGlyphInfo.html#a2e1c51a272e5a5590a2b456db512add8",
"structUILayout.html#ae88f53f3645f9f62eb568d2ab0e0c529"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';