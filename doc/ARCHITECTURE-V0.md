# Open Event Container (`lib-open-ev-container`) — v0 Architecture

> **"No music must be lost; all data must be preserved."**

## 1. Purpose & Design Principles

`lib-open-ev-container` (`loec`) is a vendor-neutral, zero-dependency C11 library and CLI tool for rescuing, exchanging, and archiving DAW projects across hosts (e.g. FL Studio → Cubase) without JVM/heavy-runtime overhead.

1. **Thin Host Mandate, Fat Rescue Tool:** A DAW only needs to write a fast binary stream (or minimal `std/export`) to participate. Heavy operations (`rtest`, `repair`, `consolidate`, `export` to JSON, `qrate` grading) live in `liboevc` and the standalone `loec` CLI.
2. **Tiers (`open` vs. `vendor_void`):**
   - `OEV_TIER_OPEN (0)`: Standard `sym_IL` musical data (tempo, tracks, MIDI/score events, audio asset references, routing, open plugin parameters). Any host or tool can read and rescue this tier.
   - `OEV_TIER_VENDOR_VOID (1)`: Opaque vendor-specific state (proprietary DSP presets, UI layout, DAW-internal blobs). Preserved untouched, never blocking open-tier rescue.
   - `OEV_TIER_ARCHIVE (2)`: Optional historical snapshots.
3. **Append-Friendly Chunk Stream (`oevcontbinf` v1):** Instead of fragile in-place binary tree mutation, the on-disk format is a flat, self-synchronizing stream of CRC32-verified chunks. The logical tree is reconstructed in memory, and `consolidate` compacts superseded/deleted chunks.
4. **Quality Rating (`qrate`):**
   - `qrate == 0`: Complete, self-contained open tier (reproducible).
   - `qrate > 0`: Fast/partial save (e.g., raw buffer only or missing `sym_IL` descriptors).

---

## 2. v0 Binary Wire Format (`oevcontbinf` v1, Little-Endian)

### File Header (16 bytes)
| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 8 | `magic` | `"OEVCONT\0"` |
| 8 | 2 | `version` | `1` |
| 10 | 2 | `flags` | `1 = FAST_SAVE`, `2 = COMPLETE` |
| 12 | 4 | `qrate` | `0` = complete/reproducible, `>0` = partial |

### Chunk Record (24-byte header + `payload_len` bytes)
| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 4 | `magic` | `"CHMK"` (resync marker for `repair`) |
| 4 | 1 | `tier` | `0 = OPEN`, `1 = VENDOR_VOID`, `2 = ARCHIVE` |
| 5 | 1 | `kind` | `1 = META`, `2 = SYM_TRACK`, `3 = SYM_CLIP`, `4 = SYM_ROUTE`, `5 = SYM_PLUGIN`, `6 = RAW_BUF`, `7 = VENDOR_BLOB` |
| 6 | 2 | `flags` | `1 = DELETED` |
| 8 | 4 | `chunk_id` | Unique chunk ID |
| 12 | 4 | `payload_len` | Payload length in bytes |
| 16 | 4 | `crc32` | IEEE CRC-32 over header bytes `[4..15]` + `payload` |
| 20 | 4 | `reserved` | `0` |
| 24 | N | `payload` | Chunk payload |

---

## 3. Core v0 Operations (`liboevc` & `loec`)

1. **`create`**: Initialize a new `oevcontbinf` v1 container.
2. **`sbuf` / `write`**: Append a typed, tiered chunk with automatic CRC-32 signing.
3. **`q` (query)**: Inspect header, `qrate`, and list/filter chunks by tier or kind.
4. **`rtest` / `repair`**: Verify header and every chunk CRC; on corruption or truncation, scan for `"CHMK"` markers, salvage all intact chunks, and emit a clean repaired file.
5. **`consolidate`**: Strip `DELETED` chunks and rewrite a clean container.
6. **`export`**: Dump container metadata, open-tier `sym_IL` structures, and `vendor_void` summaries to human-readable JSON.

---

## 4. Roadmap Beyond v0

- **Licensing Note:** The repo currently uses GPL-3.0 ([LICENSE](../LICENSE)). If proprietary DAW vendors should link `liboevc` directly, consider dual-licensing or LGPL-3.0 / MIT / Apache-2.0 for the core C library while keeping `loec` tooling GPL-3.0.
- **v0.2:** Canonical cross-DAW automation curves & pattern-to-linear timeline adapters (FL Studio ↔ Cubase).
- **v0.3:** Optional LZMA2 compression for `ARCHIVE` tiers (`atier`) and additional export targets (XML / DAWProject bridge).
