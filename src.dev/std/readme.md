### Standard

1. Bare-bones by design: supports a binary serializer, JSON, or XML.
2. Its primary purpose is to guarantee project rescue no matter what.

   i. An export consists of symbols and containers from export serializers—either in common formats or across different resolution containers, provided the chunk data is compatible (TODO).

   a. **Scale, standard, precision, units:**
      - A *standard* groups these settings together (TODO).
      - *Precision* can be set at the container, subcontainer, or global level (e.g., `m1`, `m2`).
      - This lets a host perform a fast save instead of a complete save with every IL-expected field, adjusting the numeric precision of buffer data-type symbols as needed.

   b. The library does not need to know your custom IL, as long as it can export it into a general format.

   c. Tiers allow a project to store inner part revisions and link common data (TODO: this would allow a single project to hold its own complete history).

   d. DRM or proprietary vendor info is discouraged, but supported as a separate protected tier.

#### Examples

- Modify author metadata on the container, set the software name, create a free tier, and serialize a buffer into the internal buffer format using the standard IL symbol language.
- Do the same using structured text data (XML, JSON, etc.) and string format.
- Compare paths between the free and open tiers to merge inner parts into another combined project.
