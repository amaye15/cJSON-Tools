# Installation Guide for JSON Alchemy

## Prerequisites

Before installing JSON Alchemy, you need to have the following dependencies installed:

### Required Dependencies

1. **Python Development Headers**: Required for building Python C extensions.

   - On Debian/Ubuntu:
     ```bash
     sudo apt-get install python3-dev
     ```

   - On macOS (using Homebrew):
     ```bash
     brew install python
     ```

   - On CentOS/RHEL:
     ```bash
     sudo yum install python3-devel
     ```

### Note on cJSON

The cJSON library is now included directly in the repository, so you don't need to install it separately.

## Installation

Once you have installed the prerequisites, you can install JSON Alchemy using pip:

```bash
pip install json_alchemy
```

Or, if you want to install from source:

```bash
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python
pip install -e .
```

## Troubleshooting

If you encounter an error like:

```
fatal error: 'cjson/cJSON.h' file not found
```

This means the cJSON library is not installed or not in your include path. Follow the instructions above to install the cJSON library.