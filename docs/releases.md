# Releases

This project uses Git tags to publish GitHub Releases.

## Versioning

Use semantic version tags:

- `v0.1.0`
- `v0.2.0`
- `v1.0.0`

## How a release is created

1. Push a version tag to `origin`.
2. GitHub Actions workflow `release.yml` runs.
3. A GitHub Release is created with auto-generated notes.

## Create a release

```bash
git tag v0.1.0
git push origin v0.1.0
```

## Release channels

- `main`: ongoing development
- `v*` tags: stable release snapshots
