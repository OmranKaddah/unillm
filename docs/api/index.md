# API Reference

The API reference is generated directly from public headers in:

- `include/unillm/unillm.hpp`
- `include/unillm/providers.hpp`
- `include/unillm/proxy.hpp`

## Quick links

- Public headers map: [Public Headers](public-headers.md)
- Generated Doxygen entry: [Open Doxygen API](doxygen.md)

## What you should expect

The generated reference includes:

- Every public class, enum, struct, alias, and function signature
- Field-level documentation for request/response/config structs
- Member-level details for `UnifiedClient`, `UnifiedError`, and proxy APIs

If the generated page appears empty, run:

```bash
./docs/build_docs.sh
```

The build script now fails if expected API symbols are missing from generated output.
