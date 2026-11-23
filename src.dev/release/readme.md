### Releases

- The container wire format itself will never change without a major version bump; any regular release cadence applies to container code and includes.
- The standard buffer format is versioned, and the base will not be frozen until all initiating parties agree on it.

#### Modification Registry

This directory specifies how source files are modified or included, strictly controlled by a linear mod registry.

- Unlike other projects, channel `a0` still works for storage, and later channels are refinements.
- TODO (matt): Better channel names would help—for example, noisy development lives in `alpha:0`, `b1` is the stabilization channel, and `c2` is experimental.
