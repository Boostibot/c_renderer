# c_renderer
Custom c renderer from scratch with almost zero dependencies. (Currently using stb_image for image loading and glfw for window creation).

This is a always work in progress repo. The goal is to learn all necessary skills for managing complex codebase and generate pretty images along the way. 
This means the goal is not to develop fully general engine, just lots of programable pieces that can be eventually used for something practical.

Files are organized in the single header file style of the stb libraries. 
This is mostly for personal comfort as constant switching between `.h` and `.c` files makes my life more difficult than it should be.

I try my best to include a big comment in the more or less stable files explaining not only the WHAT but also the WHY of the implementation.
(See `array.h`, `allocator_debug.h` or others).

## Main features
  - Obj asset loading pipeline
  - Local allocators and arenas
  - Common (type safe!) data structures (arrays, hash tables, reference counted storage)
  - Math librarary
  - Environment mapped PBR and phong rendering

## Unrelated but useful files
All of these have no dependency on the rest of the engine.
  - `unicode.h` Simple to use fully compliant unicode conversion functions
  - `base64.h` Customizable base 64 encode decode
  - `random.h` Very simple random number generators
  - `profile.h` Very simple atomic profiler tracking min, max, σ, μ 
  - `platform.h` / `platform_windows.c` Extensive win32 base layer including filesystem, callstack unwinding, sandboxing, virtual memory, ...

## Main todos
  - Full separation of renderer from the rest of the engine
  - Concept of a scene
  - Transparency
  - Shadow mapping
  - Screen space reflections
  - Screen space AO 
  - Uniform buffers
  - Instanced/Batch rendering
  - Pool allocator
