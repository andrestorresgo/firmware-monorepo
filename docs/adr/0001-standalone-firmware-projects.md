# Standalone Firmware Projects in Monorepo

We decided to keep `board-a-gateway` and `board-b-actuator` as completely standalone PlatformIO projects with independent protocol header definitions rather than linking a shared include directory. This eliminates compiler include path fragility and allows either firmware target to be built, flashed, or moved independently at the cost of maintaining identical packet struct definitions across both codebases.
