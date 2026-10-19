# Contributing

Thank you for helping improve KWin Active Window Outline.

## Reporting bugs

Please include:

- distribution and version;
- KDE Plasma, KWin, Qt, and KDE Frameworks versions;
- Wayland or X11 session;
- GPU and driver;
- steps to reproduce the problem;
- expected and actual behavior;
- relevant output from the user journal if available.

For rendering issues, include a screenshot or short screen recording and state whether **Draw inside window** is enabled.

## Development

Build the Wayland version without installing it:

```bash
cmake -S . -B build-wayland \
  -DKWIN_BACKEND=WAYLAND \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-wayland --parallel
```

Run `shellcheck` on both shell scripts and validate `metadata.json`, the `.ui` file, and the KConfig schema before opening a pull request.

## Code style and scope

- Keep changes focused and avoid unrelated formatting rewrites.
- Preserve SPDX headers and GPL-3.0-or-later compatibility.
- Do not claim compatibility with a KWin version or backend that was not tested.
- Document user-visible changes in `CHANGELOG.md`.

By submitting a contribution, you agree to license it under GPL-3.0-or-later.
