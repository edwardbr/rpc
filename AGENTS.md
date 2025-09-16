# Repository Guidelines

## Project Structure & Module Organization
- Core headers live in `rpc/include/rpc`; implementations live in `rpc/src` and mirror the API tree.
- IDL outputs and proxies live under `rpc/interfaces`; rerun `rpc_generator` (see presets below) whenever `.idl` files change.
- Tooling and generator code is in `generator/`; supporting deep dives and plans sit in `docs/` (start with `RPC++_User_Guide.md`).
- Tests reside in `tests/`: shared fixtures in `tests/common`, SGX orchestration in `tests/test_enclave` + `tests/test_host`, fuzz assets under `tests/fuzz_test`.
- Treat `build/` as disposable per preset; avoid committing generated telemetry or coverage artefacts.

## Build, Test, and Development Commands
- `cmake --preset Debug` configures a Ninja host build with logging/telemetry hooks enabled.
- `cmake --preset Debug_with_coroutines` enables the coroutine pipeline (proxies emit `coro::task<T>` signatures).
- `cmake --build build --target rpc` compiles the core library; add `rpc_generator` to regenerate interfaces after IDL edits.
- `ctest --test-dir build --output-on-failure` executes every GoogleTest binary, including the Y-topology regression suite.
- Toggle coverage locally with `cmake --preset Debug -DENABLE_COVERAGE=ON` and review `build/coverage` before large merges.

## Coding Style & Naming Conventions
- Apply `.clang-format` (WebKit base, 4-space indent, type-aligned pointers) via `clang-format -i` before committing.
- Baseline code targets C++17; coroutine builds rely on the C++20 toolchain described in the user guide (Clang ≥10, GCC ≥9.4, MSVC 2019).
- Interfaces follow `IName`; generated proxies become `Name_proxy`, services `Name_service`, and telemetry components use `*_telemetry_service`.
- Prefer the structured logging macros (`RPC_INFO`, `RPC_WARNING`, etc.); see `Logging_Unification_Plan.md` before touching logging internals.

## Testing Guidelines
- Host tests adopt GoogleTest with filenames suffixed `_test.cpp`; reflect runtime topology in namespaces and fixture names.
- For SGX paths run `cmake --preset Debug_SGX` then `ctest --tests-regex enclave`; never gate enclave tests into default CI.
- When touching routing or reference counting, run `test_y_topology_*` and the multithreaded suites in `tests/test_host` to catch regressions.

## Commit & Pull Request Guidelines
- Keep commits short, imperative, and under ~60 chars (e.g., `fix y topology add_ref hint`).
- PR descriptions must list presets and commands run, highlight telemetry/logging flags, and link to any relevant docs or design notes.
- Attach logs or console snapshots for tooling, generator, or telemetry changes to document behavior shifts.

## Architecture & Telemetry Notes
- RPC++ models execution as zones linked by service proxies; review `RPC++_AI_Developer_Guide.md` for reference-counting nuances before altering proxy lifecycles.
- Telemetry defaults to console output; enable `USE_RPC_TELEMETRY_RAII_LOGGING` only during investigations and scrub traces prior to upload.
- Planned logging unification expects logical thread IDs—coordinate with the plan before introducing new sinks or macros.
