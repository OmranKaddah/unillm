# NVIDIA NIM Contact Parser

Source: `examples/nvidia_nim_contact_parser.cpp`

## What it does

- Calls NVIDIA NIM chat API
- Requests strict JSON output for company contact fields
- Parses and prints:
  - company name
  - website
  - phone
  - email
  - headquarters
  - socials

## Build

```bash
cmake -S . -B build
cmake --build build --target unillm_nim_contact_example
```

## Run

```bash
./build/unillm_nim_contact_example "NVIDIA" "nemotron-nano-12b-v2-vl"
```

If the short model ID is unavailable, the example attempts model discovery and fallback.
