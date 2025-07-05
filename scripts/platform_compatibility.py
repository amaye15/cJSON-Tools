#!/usr/bin/env python3
"""
Platform Compatibility Checker for cJSON-Tools
Checks for platform-specific issues and compatibility
"""

import os
import sys
import platform
import subprocess
import re
from pathlib import Path

def get_system_info():
    """Get detailed system information"""
    info = {
        'system': platform.system(),
        'machine': platform.machine(),
        'processor': platform.processor(),
        'python_version': platform.python_version(),
        'architecture': platform.architecture()[0]
    }
    
    # Get compiler information
    for compiler in ['gcc', 'clang', 'cc']:
        try:
            result = subprocess.run([compiler, '--version'], capture_output=True, text=True)
            if result.returncode == 0:
                info[f'{compiler}_version'] = result.stdout.split('\n')[0]
        except FileNotFoundError:
            info[f'{compiler}_version'] = 'Not available'
    
    return info

def check_compiler_features():
    """Check compiler feature support"""
    print("🔍 Checking Compiler Features")
    print("-" * 30)
    
    features = {}
    
    # Check for SIMD support (platform-specific)
    machine = platform.machine().lower()
    if machine in ['x86_64', 'amd64', 'i386', 'i686']:
        # Intel/AMD SIMD instructions
        simd_tests = {
            'SSE2': '-msse2',
            'SSE4.1': '-msse4.1',
            'AVX': '-mavx',
            'AVX2': '-mavx2',
            'AVX512': '-mavx512f'
        }
    elif machine in ['arm64', 'aarch64', 'armv7l']:
        # ARM NEON instructions
        simd_tests = {
            'NEON': '-mfpu=neon' if 'armv7' in machine else '-march=armv8-a'
        }
    else:
        # Unknown architecture
        simd_tests = {}
    
    for feature, flag in simd_tests.items():
        test_code = '''
        #include <stdio.h>
        int main() { printf("SIMD test\\n"); return 0; }
        '''
        
        try:
            # Write test file
            with open('test_simd.c', 'w') as f:
                f.write(test_code)
            
            # Try to compile with SIMD flag
            result = subprocess.run(
                ['gcc', flag, '-o', 'test_simd', 'test_simd.c'],
                capture_output=True, text=True
            )
            
            features[feature] = result.returncode == 0
            
            # Cleanup
            for file in ['test_simd.c', 'test_simd']:
                if os.path.exists(file):
                    os.remove(file)
                    
        except Exception:
            features[feature] = False
    
    # Check for other compiler features
    other_features = {
        'LTO': '-flto',
        'AddressSanitizer': '-fsanitize=address',
        'ThreadSanitizer': '-fsanitize=thread',
        'UBSanitizer': '-fsanitize=undefined'
    }
    
    for feature, flag in other_features.items():
        test_code = '''
        #include <stdio.h>
        int main() { printf("Feature test\\n"); return 0; }
        '''
        
        try:
            with open('test_feature.c', 'w') as f:
                f.write(test_code)
            
            result = subprocess.run(
                ['gcc', flag, '-o', 'test_feature', 'test_feature.c'],
                capture_output=True, text=True
            )
            
            features[feature] = result.returncode == 0
            
            for file in ['test_feature.c', 'test_feature']:
                if os.path.exists(file):
                    os.remove(file)
                    
        except Exception:
            features[feature] = False
    
    # Print results
    for feature, supported in features.items():
        status = "✅" if supported else "❌"
        print(f"{status} {feature}")
    
    return features

def check_dependencies():
    """Check for required dependencies"""
    print("\n📦 Checking Dependencies")
    print("-" * 30)
    
    dependencies = {}
    
    # Check for required libraries
    libraries = ['pthread', 'm']  # pthread and math library
    
    for lib in libraries:
        test_code = f'''
        #include <stdio.h>
        {"#include <pthread.h>" if lib == "pthread" else "#include <math.h>"}
        int main() {{ 
            {"pthread_t t;" if lib == "pthread" else "double x = sqrt(4.0);"}
            printf("Library test\\n"); 
            return 0; 
        }}
        '''
        
        try:
            with open('test_lib.c', 'w') as f:
                f.write(test_code)
            
            link_flag = f'-l{lib}' if lib != 'm' or platform.system() != 'Darwin' else ''
            cmd = ['gcc', '-o', 'test_lib', 'test_lib.c']
            if link_flag:
                cmd.append(link_flag)
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            dependencies[lib] = result.returncode == 0
            
            for file in ['test_lib.c', 'test_lib']:
                if os.path.exists(file):
                    os.remove(file)
                    
        except Exception:
            dependencies[lib] = False
    
    # Check Python dependencies
    python_deps = ['json', 'ctypes', 'pathlib']
    for dep in python_deps:
        try:
            __import__(dep)
            dependencies[f'python_{dep}'] = True
        except ImportError:
            dependencies[f'python_{dep}'] = False
    
    # Print results
    for dep, available in dependencies.items():
        status = "✅" if available else "❌"
        print(f"{status} {dep}")
    
    return dependencies

def check_build_system():
    """Check build system compatibility"""
    print("\n🔧 Checking Build System")
    print("-" * 30)
    
    build_checks = {}
    
    # Check for make
    try:
        result = subprocess.run(['make', '--version'], capture_output=True, text=True)
        build_checks['make'] = result.returncode == 0
        if build_checks['make']:
            version = result.stdout.split('\n')[0]
            print(f"✅ make: {version}")
    except FileNotFoundError:
        build_checks['make'] = False
        print("❌ make: Not found")
    
    # Check if our Makefile works
    try:
        result = subprocess.run(['make', '-n', 'clean'], capture_output=True, text=True)
        build_checks['makefile'] = result.returncode == 0
        status = "✅" if build_checks['makefile'] else "❌"
        print(f"{status} Makefile compatibility")
    except Exception:
        build_checks['makefile'] = False
        print("❌ Makefile compatibility")
    
    return build_checks

def check_platform_specific():
    """Check platform-specific issues"""
    print("\n🖥️  Platform-Specific Checks")
    print("-" * 30)
    
    system = platform.system()
    issues = []
    recommendations = []
    
    if system == 'Darwin':  # macOS
        print("🍎 macOS detected")
        
        # Check for Xcode command line tools
        try:
            result = subprocess.run(['xcode-select', '--print-path'], capture_output=True)
            if result.returncode == 0:
                print("✅ Xcode command line tools installed")
            else:
                issues.append("Xcode command line tools not found")
                recommendations.append("Install with: xcode-select --install")
        except FileNotFoundError:
            issues.append("xcode-select not found")
            recommendations.append("Install Xcode command line tools")
        
        # Check for Homebrew (optional but recommended)
        try:
            result = subprocess.run(['brew', '--version'], capture_output=True)
            if result.returncode == 0:
                print("✅ Homebrew available")
            else:
                recommendations.append("Consider installing Homebrew for easier dependency management")
        except FileNotFoundError:
            recommendations.append("Consider installing Homebrew for easier dependency management")
    
    elif system == 'Linux':
        print("🐧 Linux detected")
        
        # Check for build-essential or equivalent
        try:
            result = subprocess.run(['dpkg', '-l', 'build-essential'], capture_output=True)
            if result.returncode == 0:
                print("✅ build-essential package installed")
            else:
                recommendations.append("Install build-essential: sudo apt-get install build-essential")
        except FileNotFoundError:
            # Try rpm-based systems
            try:
                result = subprocess.run(['rpm', '-q', 'gcc'], capture_output=True)
                if result.returncode == 0:
                    print("✅ GCC installed")
                else:
                    recommendations.append("Install development tools for your distribution")
            except FileNotFoundError:
                recommendations.append("Install development tools for your distribution")
    
    elif system == 'Windows':
        print("🪟 Windows detected")
        issues.append("Windows support may require additional setup")
        recommendations.extend([
            "Consider using WSL (Windows Subsystem for Linux)",
            "Or use MinGW-w64 for native Windows compilation",
            "Visual Studio Build Tools may also work"
        ])
    
    # Print issues and recommendations
    if issues:
        print("\n⚠️  Issues found:")
        for issue in issues:
            print(f"  - {issue}")
    
    if recommendations:
        print("\n💡 Recommendations:")
        for rec in recommendations:
            print(f"  - {rec}")
    
    return issues, recommendations

def generate_compatibility_report():
    """Generate a comprehensive compatibility report"""
    print("🎯 cJSON-Tools Platform Compatibility Report")
    print("=" * 60)
    
    # Get system info
    info = get_system_info()
    print(f"System: {info['system']} {info['architecture']}")
    print(f"Machine: {info['machine']}")
    print(f"Python: {info['python_version']}")
    print()
    
    # Run all checks
    compiler_features = check_compiler_features()
    dependencies = check_dependencies()
    build_system = check_build_system()
    issues, recommendations = check_platform_specific()
    
    # Overall compatibility score (weighted)
    # Dependencies and build system are critical (weight 2)
    # Compiler features are nice-to-have (weight 1)
    critical_checks = len(dependencies) + len(build_system)
    critical_passed = sum(dependencies.values()) + sum(build_system.values())

    feature_checks = len(compiler_features)
    feature_passed = sum(compiler_features.values())

    total_weighted = (critical_checks * 2) + feature_checks
    passed_weighted = (critical_passed * 2) + feature_passed

    compatibility_score = (passed_weighted / total_weighted) * 100 if total_weighted > 0 else 0
    
    print(f"\n📊 Overall Compatibility Score: {compatibility_score:.1f}%")
    
    if compatibility_score >= 90:
        print("🎉 Excellent compatibility!")
    elif compatibility_score >= 75:
        print("✅ Good compatibility with minor issues")
    elif compatibility_score >= 50:
        print("⚠️  Moderate compatibility - some features may not work")
    else:
        print("❌ Poor compatibility - significant issues detected")
    
    return {
        'score': compatibility_score,
        'system_info': info,
        'compiler_features': compiler_features,
        'dependencies': dependencies,
        'build_system': build_system,
        'issues': issues,
        'recommendations': recommendations
    }

def main():
    # Change to project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    os.chdir(project_root)
    
    report = generate_compatibility_report()
    
    # Exit with appropriate code
    if report['score'] >= 75:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
