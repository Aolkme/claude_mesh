# Repository Guidelines

## Project Structure & Module Organization
`libmeshcore/` contains the C11 static library: public headers live in `include/`, implementations in `src/`. `meshgateway/` builds the daemon and owns config, logging, TCP server, and event-loop code. `meshgate-cli/` contains the client binary and output formatting helpers. `protobufs_protobuf-c/` stores generated `*.pb-c.[ch]` sources consumed by `meshcore`; `protobuf-c-1.5.2/` and `protoc-34.1-osx-aarch_64/` are vendored dependencies and tooling. `doc/spec/` and `doc/plan/` hold protocol and design references. Treat `build/` as generated CMake output, not a source directory.

## Build, Test, and Development Commands
Run all commands from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target meshgateway
cmake --build build --target meshgate-cli
cmake --install build --prefix /tmp/claude-mesh
```

For local runs, start from the sample config:

```bash
cp meshgateway.conf.example meshgateway.conf
./build/meshgateway/meshgateway -c meshgateway.conf
./build/meshgate-cli/meshgate-cli status
```

## Coding Style & Naming Conventions
Target C11 (`CMAKE_C_STANDARD 11`). Follow the style already present in the file you touch: 4-space indentation, `snake_case` for functions and variables, `UPPER_SNAKE_CASE` for macros/constants, and matched header/source names such as `frame_parser.h` and `frame_parser.c`. Keep public interfaces in `libmeshcore/include/` and module-local helpers beside their `.c` files. Preserve concise explanatory comments; bilingual English/Chinese notes are already used in core files. Avoid casual edits to vendored or generated protobuf files.

## Testing Guidelines
No automated `ctest` suite is configured yet. Every change should at least rebuild the affected target and run smoke checks for the touched binary, for example `./build/meshgateway/meshgateway -h` and `./build/meshgate-cli/meshgate-cli -h`. If you change serial, TCP, or protocol behavior, include exact manual verification steps and observed output or log lines in the PR.

## Commit & Pull Request Guidelines
Current history uses short, single-line subjects such as `init` and `修复`. Keep commit messages similarly brief, imperative, and limited to one logical change. For pull requests, describe the affected module (`libmeshcore`, `meshgateway`, or `meshgate-cli`), list the build and smoke-test commands you ran, link the related issue or design note, and include config snippets or CLI output when behavior changes.
