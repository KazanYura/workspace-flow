---
name: add-pipeline-task
description: 'Add or modify a pipeline task in DevFlow Orchestrator. Use when introducing a new task instance in config.yaml, extending the YAML schema/parser, or reasoning about the command/poll/gate task types and how PipelineRunner executes them.'
---

# Add a Pipeline Task

The pipeline supports exactly three task types: **`command`**, **`poll`**, **`gate`**. Do not add new type keys without an explicit design decision — most needs fit one of these three.

## When to Use
- Adding a new step to a user's `config.yaml` (see [example_config.yaml](../../docs/example_config.yaml)).
- Extending the `ConfigParser` to read a new field on an existing task type.
- Wiring a task into the `PipelineRunner` state machine.

## Task-Type Reference
| Type | Runs | Advances when | Key fields |
|------|------|---------------|-----------|
| `command` | A process via `CreateProcessW`, waits for exit | Exit code `0` (or `timeout_sec` elapses → fail) | `command`, `timeout_sec` |
| `poll` | A readiness probe on a loop | Probe succeeds, or fails after `max_retries` | `command`, `retry_interval_sec`, `max_retries` |
| `gate` | Optional `pre_command`, then blocks | User confirms (Enter in console MVP; button in GUI) | `message`, `pre_command` |

All tasks share `id`, `name`, `type`.

## Procedure — add a config entry
1. Append a list item under `pipeline:` with a unique `id`, human `name`, and the correct `type`.
2. Provide the type-specific fields from the table above. Quote Windows paths and escape backslashes (`"C:\\dev\\repo"`).
3. Order matters — the runner executes top-to-bottom.

## Procedure — extend the parser/runner
1. Add the field to the C++ task struct/variant in the config data model.
2. Read it in `ConfigParser` with a sensible default; on an unknown `type` or missing required field, return a parse error via `std::optional`/`std::expected` (do not throw).
3. Handle the field in the matching branch of the `PipelineRunner` state machine.
4. Add a fixture to `tests/` covering the new field (happy path + missing/invalid).
5. Update [example_config.yaml](../../docs/example_config.yaml) and the task-type table in [README.md](../../docs/README.md) if behavior changed.

## Guidance
- A `poll` task must be idempotent and side-effect-free — it may run many times.
- Reserve `gate` for genuinely manual steps (2FA); don't use it as a sleep.
- Keep parsing total: an unrecognized `type` should fail fast with a clear message, not be silently skipped.
