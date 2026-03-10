# metacall-inspect-poc

Static symbol extraction PoC for `.py` and `.js` files using Tree-sitter.

## What it does

`ast_inspect` takes a single file path, parses it statically, and can print:

- `ast` format (default): PoC AST-like output
- `inspect` format: MetaCall inspect-compatible shape (`<loader_tag> -> [handle -> scope]`)

Extracted data now includes:

- `path`
- `language` (`python` or `javascript`)
- `symbols` array with:
  - `name`
  - `kind` (`function`, `class`, `method`, `constructor`)
  - `start_line`
  - `end_line`
- JS module export detection (`module.exports` / `exports.*` / `export ...`)
- Class method extraction for Python and JavaScript

Supported CLI flags:

- `--pretty` pretty-prints JSON output.
- `--format ast|inspect` select output format.
- `--output-dir <dir>` also writes output JSON file to `<dir>/<input_filename>.json`.

## Build (static-only)

Run from `metacall-inspect-poc`:

- `./run.sh`

This configures and builds only static analyzer targets and runs AST checks.

## Build (with MetaCall runtime targets)

If you also want runtime inspect demo/tests/bench targets:

- `./run.sh /absolute/path/to/core/build`

## Direct usage

After build, from `build-poc`:

- `./ast_inspect ../scripts/test.py`
- `./ast_inspect ../scripts/test.js`
- `./ast_inspect --pretty ../scripts/test.py`
- `./ast_inspect --pretty --output-dir ../out ../scripts/test.js`
- `./ast_inspect --format inspect --pretty ../scripts/test.js`

Example output shape:

`{"path":".../test.py","language":"python","symbols":[{"name":"PythonTestClass","kind":"class","start_line":1,"end_line":6}]}`
