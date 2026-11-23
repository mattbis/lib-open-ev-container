# `std:op`

> **Note:** Most of these operations are unnecessary for a DAW host and are included only for the standalone library/CLI tool.

## Container Library Operations

- TODO (matt): Move into `lib/op`.

- **`calc`**: Calculate the size of a path within the container.
- **`create`**: Create a new empty container.
- **`ctest`**: Test the container.
- **`ctier`**: Extend the container with a new tier.
- **`ctree`**: Used by `consolidate` to restructure the container.
- **`empty`**: Completely empty header-only container with markers and an empty word section.
- **`export`**: Container export, controlled by modes.
- **`id`**: Get a unique standard container ID.
- **`import`**: Import another container into the current container.
- **`imut`**: Mark a dotpath as immutable.
- **`mod`**: Change any container path using selector format.
- **`mut`**: Mutate a direct path and a single value quickly.
- **`q`**: Query paths.
- **`qbuf`**: Get all buffers.
- **`qrate`**: Grade the quality of container data (whether the save was fast/partial or complete with all data required **to guarantee it can be reproduced**).
- **`repair`**: Check inner consistency and data stored within buffers, salvaging valid chunks.
- **`rtest`**: Check whether repair is needed.
- **`sbuf`**: Set a new buffer as an interface (this is how data is added).
- **`sfixed`**: Set a fixed-size buffer for arbitrary data.
- **`sign`**: Sign a data buffer with a checksum (**automatic**: all writes are checksummed).
- **`stest`**: Set a test that the container must pass (not transmutable to other hosts).
- **`stier`**: Create a tier level. A container can have nested tiers as a tree, typically to separate vendor-protected data from free data. In practice, avoid deep nesting; `consolidate` serializes deep structures into faster chunks.
- **`swrite`**: Standard write for bulk or single data.
