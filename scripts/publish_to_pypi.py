#!/usr/bin/env python3
"""
Automated PyPI Publishing Script for cJSON-Tools

This script automates the process of building and publishing
the cJSON-Tools package to PyPI using local tools.
"""

import os
import subprocess
import sys
import shutil
from pathlib import Path


def run_command(cmd, cwd=None, check=True):
    """Run a command and return the result."""
    print(f"🔄 Running: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    
    if result.stdout:
        print(f"📤 Output: {result.stdout.strip()}")
    
    if result.stderr and result.returncode != 0:
        print(f"❌ Error: {result.stderr.strip()}")
    
    if check and result.returncode != 0:
        sys.exit(f"💥 Command failed: {cmd}")
    
    return result


def check_dependencies():
    """Check if required tools are installed."""
    print("🔍 Checking dependencies...")
    
    required_tools = ['python3', 'pip', 'twine']
    missing_tools = []
    
    for tool in required_tools:
        result = run_command(f"which {tool}", check=False)
        if result.returncode != 0:
            missing_tools.append(tool)
    
    if missing_tools:
        print(f"❌ Missing required tools: {', '.join(missing_tools)}")
        print("📦 Install missing tools:")
        for tool in missing_tools:
            if tool == 'twine':
                print(f"   pip install {tool}")
            else:
                print(f"   Install {tool} from your package manager")
        sys.exit(1)
    
    print("✅ All dependencies available")


def clean_build_artifacts():
    """Clean previous build artifacts."""
    print("🧹 Cleaning build artifacts...")
    
    py_lib_dir = Path("py-lib")
    
    # Directories to clean
    clean_dirs = [
        py_lib_dir / "build",
        py_lib_dir / "dist", 
        py_lib_dir / "cjson_tools.egg-info",
        py_lib_dir / "*.egg-info"
    ]
    
    for clean_dir in clean_dirs:
        if clean_dir.exists():
            if clean_dir.is_dir():
                shutil.rmtree(clean_dir)
                print(f"🗑️  Removed directory: {clean_dir}")
            else:
                # Handle glob patterns
                for path in py_lib_dir.glob("*.egg-info"):
                    if path.is_dir():
                        shutil.rmtree(path)
                        print(f"🗑️  Removed directory: {path}")
    
    print("✅ Build artifacts cleaned")


def build_package():
    """Build the Python package."""
    print("🔨 Building Python package...")
    
    py_lib_dir = "py-lib"
    
    # Install build dependencies
    run_command("pip install --upgrade pip setuptools wheel build twine")
    
    # Build source distribution and wheel
    run_command("python setup.py sdist bdist_wheel", cwd=py_lib_dir)
    
    # List built packages
    dist_dir = Path(py_lib_dir) / "dist"
    if dist_dir.exists():
        print("📦 Built packages:")
        for package in dist_dir.iterdir():
            print(f"   📄 {package.name}")
    
    print("✅ Package built successfully")


def test_package():
    """Test the built package."""
    print("🧪 Testing built package...")
    
    py_lib_dir = "py-lib"
    
    # Check package with twine
    run_command("twine check dist/*", cwd=py_lib_dir)
    
    print("✅ Package validation passed")


def publish_to_pypi():
    """Publish the package to PyPI."""
    print("🚀 Publishing to PyPI...")
    
    py_lib_dir = "py-lib"
    
    # Check if we're in a git repository and on main branch
    try:
        branch_result = run_command("git branch --show-current", check=False)
        if branch_result.returncode == 0:
            current_branch = branch_result.stdout.strip()
            if current_branch != "main":
                print(f"⚠️  Warning: Not on main branch (current: {current_branch})")
                response = input("Continue anyway? (y/N): ")
                if response.lower() != 'y':
                    sys.exit("❌ Aborted by user")
    except:
        pass
    
    # Confirm publication
    print("📋 Ready to publish cJSON-Tools v1.4.0 to PyPI")
    print("🔗 Package will be available at: https://pypi.org/project/cjson-tools/")
    response = input("🚀 Proceed with publication? (y/N): ")
    
    if response.lower() != 'y':
        sys.exit("❌ Publication cancelled by user")
    
    # Publish to PyPI
    print("📤 Uploading to PyPI...")
    run_command("twine upload dist/*", cwd=py_lib_dir)
    
    print("🎉 Successfully published to PyPI!")
    print("📦 Install with: pip install cjson-tools==1.4.0")
    print("🔗 View at: https://pypi.org/project/cjson-tools/")


def main():
    """Main function."""
    print("🚀 cJSON-Tools PyPI Publishing Automation")
    print("=" * 50)
    
    # Change to project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    os.chdir(project_root)
    
    try:
        check_dependencies()
        clean_build_artifacts()
        build_package()
        test_package()
        publish_to_pypi()
        
        print("\n🎉 Publication completed successfully!")
        print("📦 cJSON-Tools v1.4.0 is now live on PyPI!")
        
    except KeyboardInterrupt:
        print("\n❌ Publication cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n💥 Publication failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
