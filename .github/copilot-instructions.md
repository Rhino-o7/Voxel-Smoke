# Copilot Instructions

## Project Guidelines
- For Client-Web work, avoid #ifdef/#endif platform guards because it is Emscripten-only. Prefer separate files under Client-Web and avoid modifying CoreLib files used by other projects.