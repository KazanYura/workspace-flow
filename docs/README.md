# DevFlow Orchestrator

A lightweight, native **C++20 desktop utility for Windows** that automates a brittle, multi-step development environment startup while guarding against thermal-induced system freezes.

## 🛑 The Problem
Context-switching overhead. The current development environment requires a strict, sequential, and brittle 6-step initialization process:
1. Unlock an encrypted drive (BitLocker) with a password.
2. Start the Docker Desktop daemon (WSL2 backend).
3. Connect to the corporate VPN using a Cisco AnyConnect one-time passcode (TOTP).
4. Open the dev container via VS Code.
5. Pull the latest repository changes.
6. Rebuild the dev container.

Missing a step, executing them out of order, or experiencing a thermal-induced system freeze mandates restarting the entire ~15-minute procedure. This occurs multiple times a day due to idle timeouts and laptop overheating.

## 🚀 The Solution
A single, self-contained Windows executable that reads a declarative YAML file and drives the whole sequence to completion.

**Core Features:**
* **Deterministic Execution:** Reads a YAML configuration to execute steps strictly in order.
* **Process Polling & Readiness:** Doesn't rely on arbitrary `Sleep()` calls. It actively polls readiness signals (e.g., the Docker Engine named pipe `\\.\pipe\docker_engine`) and process state to ensure a step is fully complete before advancing.
* **Interactive Gates:** Halts execution for manual input when necessary (e.g., waiting for the user to approve a Cisco AnyConnect VPN token).
* **Thermal Monitoring & Auto-Mitigation:** Runs a background telemetry thread to monitor CPU/package temperatures via WMI/PDH. If approaching critical thresholds, it automatically suspends or terminates predefined resource-heavy processes (like browser renderers or IDE indexers) to prevent a hard lockup.
* **Immediate Mode UI:** Uses Dear ImGui for an ultra-low overhead, highly responsive heads-up display tracking pipeline progress and system temperatures.

## 🧩 Task Types
The pipeline is composed of three standardized task types:
| Type | Purpose |
| --- | --- |
| `command` | Launch a process and wait for it to exit (e.g., `git pull`). |
| `poll` | Run a readiness check on a loop until it succeeds or times out (e.g., `docker info`). |
| `gate` | Pause the pipeline and wait for explicit user confirmation (e.g., the VPN 2FA prompt). |

## 🧪 MVP Scope (Console First)
The first deliverable is a **console application** — no GUI. It reads the YAML pipeline, executes the three task types in order, and streams structured logs (via spdlog) to stdout, including task state transitions, subprocess output, and thermal readings. Interactive `gate` tasks pause for the user to press Enter. This proves the orchestration and thermal-mitigation logic end-to-end before any UI work begins. The Dear ImGui dashboard is layered on top afterward (see [ROADMAP.md](ROADMAP.md), Phases 4–5).

## 🛠️ Target Platform
* **OS:** Windows 10/11 (x64).
* **Toolchain:** MSVC (Visual Studio 2022) or clang-cl, C++20.
* **Build:** CMake with `FetchContent` for all dependencies — no system package manager required.

## 🤖 How to Use This Project with AI (Cursor / Copilot / Claude)
This directory contains the foundational context documents for this project. When starting a new AI chat on a fresh codebase, feed [AI_CONTEXT.md](AI_CONTEXT.md) to the AI first. This sets the persona, technology stack, and architectural constraints. Then follow the phases in [ROADMAP.md](ROADMAP.md) sequentially to build the application without overwhelming the AI context window. See [ARCHITECTURE.md](ARCHITECTURE.md) for module boundaries and [example_config.yaml](example_config.yaml) for a working pipeline definition.
