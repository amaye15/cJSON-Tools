# Code Formatting Guide for cJSON-Tools Python Package

This project uses [Black](https://black.readthedocs.io/) for consistent Python code formatting.

## Quick Start

### Format all code:
```bash
# Option 1: Using the format script
python format.py

# Option 2: Using Black directly
python3 -m black .

# Option 3: Using Make
make format
```

### Check formatting without changing files:
```bash
# Option 1: Using the format script
python format.py --check

# Option 2: Using Black directly
python3 -m black --check --diff .

# Option 3: Using Make
make format-check
```

## Installation

### Install Black and other dev tools:
```bash
# Install all development dependencies
pip install -r requirements-dev.txt

# Or install just Black
pip install black
```

## Configuration

Black is configured in `pyproject.toml`:

```toml
[tool.black]
line-length = 88
target-version = ['py38', 'py39', 'py310', 'py311', 'py312']
include = '\.pyi?$'
extend-exclude = '''
/(
  \.eggs
  | \.git
  | \.mypy_cache
  | \.tox
  | \.venv
  | _build
  | buck-out
  | build
  | dist
  | cjson_tools\.egg-info
)/
'''
```

## Pre-commit Hooks

To automatically format code before commits:

```bash
# Install pre-commit
pip install pre-commit

# Install the hooks
pre-commit install

# Run on all files
pre-commit run --all-files
```

## GitHub Actions

Code formatting is automatically checked in CI/CD:
- The workflow will fail if code is not properly formatted
- Run `python format.py` locally before pushing to fix any issues

## Editor Integration

### VS Code
Install the Python extension and add to settings.json:
```json
{
    "python.formatting.provider": "black",
    "python.formatting.blackArgs": ["--line-length=88"],
    "editor.formatOnSave": true
}
```

### PyCharm
1. Go to Settings → Tools → External Tools
2. Add Black as an external tool
3. Set up a keyboard shortcut for formatting

## Why Black?

- **Consistent**: Everyone gets the same formatting
- **Fast**: Formats code quickly
- **Deterministic**: Same input always produces same output
- **Minimal configuration**: Fewer decisions to make
- **Widely adopted**: Industry standard for Python formatting
