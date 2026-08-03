# feed/

Drop anything in here and the mind devours it.

This is **P13 — Intake**, the boundary between the world and the system, and the
first piece that makes behaviour observable. See
[the memory architecture](../docs/design/memory-concept-map-architecture.md), §8.

## Status

**Nothing consumes this folder yet.** The contract below is design, not
implemented behaviour. Files placed here today will simply sit here.

## Contract

- **Read-only source.** Files placed here are never modified, moved, or deleted
  by the system. Whatever you drop in stays exactly as you left it.
- **Ingestion is traversal, not import.** The file itself is not stored anywhere.
  Reading it walks paths through the atomic base, and that walking is what
  strengthens them. What persists is the trace of having read it.
- **Not idempotent, by design.** Reading the same file twice is not the same
  experience twice — the second reading traverses paths that are already strong.
  This is correct behaviour, not a defect.
- **Order matters.** A mind that reads A then B is not the same mind that read
  B then A.

## Contents are not version-controlled

Everything in this folder except this README is ignored by git. Feed material is
experience, not source — it belongs to a particular run of a particular mind, not
to the repository.
