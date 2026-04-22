# ZeroEngine Apps (`.zeroapp`)

A `.zeroapp` is the simplest possible bundle format for ZeroEngine: a
folder containing a JSON manifest and a JSPP entry script.

## Layout

```
my-app/
├── zeroapp.json
└── main.jspp
```

## Manifest (`zeroapp.json`)

```json
{
    "name": "My App",
    "version": "0.1.0",
    "description": "Short one-liner",
    "author": "You",
    "entry": "main.jspp",
    "mode": "2d"
}
```

Fields:

| key           | required | notes                                              |
| ------------- | -------- | -------------------------------------------------- |
| `name`        | yes      | Display name shown in the banner.                  |
| `entry`       | yes      | Relative path to a `.jspp` file inside the bundle. |
| `version`     | no       | Free-form string.                                  |
| `description` | no       | Shown in the banner.                               |
| `author`      | no       | Metadata only.                                     |
| `mode`        | no       | `"2d"` or `"3d"` hint. Auto-detected from output.  |

`entry` must be a plain relative path — no `..`, no absolute paths, no
drive letters. The file must live inside the app folder.

## Running

1. Launch ZeroEngine Desktop.
2. On the home screen click the **ZeroEngine Apps** tile.
3. In the file picker select the app's `zeroapp.json`.
4. The Playground opens, loads the entry script, and auto-compiles it
   to native C++ via the local toolchain.

## Example

See [`zeroapps/bouncy/`](../zeroapps/bouncy/) — 24 bouncing balls.

## Roadmap

Future versions may support:

- Zipped `.zeroapp` files (currently folders only).
- Multi-file JSPP projects with `import` resolution against the bundle
  root.
- Bundled native C++ modules loaded as hot paths.
- Per-app persistent storage sandbox.
