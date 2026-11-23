#### Serialization Formats

1. JSON and XML are verbose and less efficient, but structured so nothing is lost.
2. BSON (or similar binary formats) is much faster and groups data more compactly.
3. The serializer uses either raw chunks with a lookup table, or JSON/XML to guarantee readability everywhere.
4. XML and JSON work everywhere—which is a core goal—though a binary format is still needed for speed.
5. `oevcontbinf` is the binary buffer format for this.

- Ultimately, the specific wire format matters less than having a working representation that translates cleanly across hosts or acts as an IL.
