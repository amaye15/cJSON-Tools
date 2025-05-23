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

   - On Windows:
     - Install Visual C++ Build Tools: https://visualstudio.microsoft.com/visual-cpp-build-tools/
     - Make sure to select "C++ build tools" during installation
     - Python will automatically use the installed compiler

## Installation

Once you have installed the prerequisites, you can install JSON Alchemy using pip:

```bash
pip install aimv2-json-alchemy
```

Or, if you want to install from source:

```bash
git clone https://github.com/amaye15/AIMv2-rs.git
cd AIMv2-rs/python
pip install -e .
```

## Platform-Specific Notes

### Windows

- Make sure you have the Visual C++ Build Tools installed as mentioned above
- The package should build and install without any additional configuration

### macOS

- If you're using Apple Silicon (M1/M2), make sure you're using a compatible Python version
- You may need to install the Xcode Command Line Tools: `xcode-select --install`

### Linux

- Make sure you have the appropriate development tools installed (gcc, make, etc.)
- On some distributions, you may need to install additional packages like `build-essential`

## Troubleshooting

If you encounter build errors:

1. Make sure you have the correct development tools installed for your platform
2. Check that your Python version is supported (3.8 or higher)
3. On Windows, ensure Visual C++ Build Tools are properly installed
4. On macOS, ensure Xcode Command Line Tools are installed