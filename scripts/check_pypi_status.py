#!/usr/bin/env python3
"""
Check PyPI publication status for cJSON-Tools
"""

import requests
import json
import time
import sys
from datetime import datetime


def check_pypi_package():
    """Check if the package exists on PyPI and get version info."""
    try:
        response = requests.get("https://pypi.org/pypi/cjson-tools/json", timeout=10)
        if response.status_code == 200:
            data = response.json()
            latest_version = data['info']['version']
            upload_time = data['releases'].get(latest_version, [{}])[0].get('upload_time', 'Unknown')
            
            print(f"✅ Package found on PyPI!")
            print(f"📦 Latest version: {latest_version}")
            print(f"📅 Upload time: {upload_time}")
            print(f"🔗 PyPI URL: https://pypi.org/project/cjson-tools/{latest_version}/")
            
            # Check if v1.4.0 is available
            if '1.4.0' in data['releases']:
                v140_files = data['releases']['1.4.0']
                print(f"\n🎉 Version 1.4.0 is available!")
                print(f"📁 Number of files: {len(v140_files)}")
                
                # Show file types
                file_types = {}
                for file_info in v140_files:
                    file_type = file_info.get('packagetype', 'unknown')
                    file_types[file_type] = file_types.get(file_type, 0) + 1
                
                print("📋 Available files:")
                for file_type, count in file_types.items():
                    print(f"   {file_type}: {count} files")
                
                return True
            else:
                print(f"\n⏳ Version 1.4.0 not yet available")
                print(f"📋 Available versions: {list(data['releases'].keys())}")
                return False
                
        elif response.status_code == 404:
            print("❌ Package not found on PyPI")
            return False
        else:
            print(f"⚠️ Unexpected response: {response.status_code}")
            return False
            
    except requests.RequestException as e:
        print(f"❌ Error checking PyPI: {e}")
        return False


def check_github_workflow():
    """Check the status of the GitHub Actions workflow."""
    try:
        # Get latest workflow runs
        response = requests.get(
            "https://api.github.com/repos/amaye15/cJSON-Tools/actions/workflows/publish.yml/runs?per_page=3",
            timeout=10
        )
        
        if response.status_code == 200:
            data = response.json()
            runs = data.get('workflow_runs', [])
            
            if runs:
                latest_run = runs[0]
                status = latest_run.get('status', 'unknown')
                conclusion = latest_run.get('conclusion', 'unknown')
                created_at = latest_run.get('created_at', 'unknown')
                html_url = latest_run.get('html_url', '')
                
                print(f"\n🤖 Latest GitHub Actions workflow:")
                print(f"📊 Status: {status}")
                print(f"🎯 Conclusion: {conclusion}")
                print(f"📅 Started: {created_at}")
                print(f"🔗 View: {html_url}")
                
                if status == 'completed' and conclusion == 'success':
                    print("✅ Workflow completed successfully!")
                    return True
                elif status == 'in_progress':
                    print("⏳ Workflow is still running...")
                    return False
                else:
                    print("❌ Workflow failed or was cancelled")
                    return False
            else:
                print("\n❌ No workflow runs found")
                return False
        else:
            print(f"\n⚠️ Could not check workflow status: {response.status_code}")
            return False
            
    except requests.RequestException as e:
        print(f"\n❌ Error checking GitHub workflow: {e}")
        return False


def main():
    """Main function."""
    print("🔍 Checking cJSON-Tools PyPI Publication Status")
    print("=" * 50)
    
    # Check PyPI status
    pypi_success = check_pypi_package()
    
    # Check GitHub workflow status
    workflow_success = check_github_workflow()
    
    print("\n" + "=" * 50)
    
    if pypi_success:
        print("🎉 SUCCESS: cJSON-Tools v1.4.0 is published on PyPI!")
        print("📦 Install with: pip install cjson-tools==1.4.0")
    elif workflow_success:
        print("⏳ Workflow completed but package not yet visible on PyPI")
        print("   This can take a few minutes to propagate")
    else:
        print("❌ Publication not yet complete")
        print("🔄 Check again in a few minutes")
        print("🔗 Monitor: https://github.com/amaye15/cJSON-Tools/actions/workflows/publish.yml")
    
    return 0 if pypi_success else 1


if __name__ == "__main__":
    sys.exit(main())
