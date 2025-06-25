#!/usr/bin/env python3
"""
Simple script to format Python code using Black.
Usage: python format.py [--check]
"""

import subprocess
import sys
import os


def run_black(check_only=False):
    """Run Black formatter on the codebase."""
    cmd = ["python3", "-m", "black"]
    
    if check_only:
        cmd.extend(["--check", "--diff"])
    
    cmd.append(".")
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=os.path.dirname(__file__))
    return result.returncode


def main():
    """Main entry point."""
    check_only = "--check" in sys.argv
    
    if check_only:
        print("🔍 Checking code formatting...")
        exit_code = run_black(check_only=True)
        if exit_code == 0:
            print("✅ All files are properly formatted!")
        else:
            print("❌ Some files need formatting. Run 'python format.py' to fix.")
    else:
        print("🎨 Formatting code with Black...")
        exit_code = run_black(check_only=False)
        if exit_code == 0:
            print("✅ Code formatting completed!")
        else:
            print("❌ Formatting failed.")
    
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
