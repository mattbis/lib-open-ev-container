# lib-open-ev-container.dev

All fast exports of complex structured data—such as a DAW project—should use a single open container format. Without one, it is too easy to lose an entire project or fail to enforce common interchange standards.

This container is also useful for any kind of data serialization that requires structure and portable export.

See [doc/ARCHITECTURE-V0.md](doc/ARCHITECTURE-V0.md) for the v0 specification, binary wire format, and build/test instructions.

## TODO (matt)
- Clean up the repository layout (some paths may become symlinks).
- Move `res/` into the proper place and separate the concerns of the repository, the library, and the container format so it is easier to read and follow how everything fits together.
