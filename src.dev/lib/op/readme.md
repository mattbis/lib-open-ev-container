# Standard Container Operations

See `std/op` for internal container operations.

## Always-Included Operations

- **`qrate`**: Indicates completeness—for example, if a host did not save a free tier and only saved its protected tier, or whether its IL can auto-schema or not.
  - Integrity and export granularity are controlled by the host. The `a1`, `b2`, `c3` markers represent progressive completeness levels up to `C3` (most of the data needed so *it just works*).
