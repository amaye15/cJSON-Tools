#!/usr/bin/env python3
"""
JsonToolsBuilder Example - Demonstrating Fluent Interface for JSON Transformations

This example shows how to use the JsonToolsBuilder class to chain multiple
JSON transformation operations in a single, efficient pass.
"""

import json
from cjson_tools import JsonToolsBuilder


def main():
    """Demonstrate the JsonToolsBuilder with complex chained operations."""
    
    # Sample data with various issues that need transformation
    sample_data = {
        # Session tracking data with inconsistent naming
        "session.tracking.homepage.timeMs": 1500,
        "session.tracking.aboutpage.timeMs": 2000,
        "session.tracking.contactpage.timeMs": 1200,
        
        # Analytics data with old naming convention
        "analytics.page.home.visits": 100,
        "analytics.page.about.visits": 200,
        "analytics.page.contact.visits": 50,
        
        # Legacy server data
        "legacy.server.instance1": "old_active",
        "legacy.server.instance2": "old_inactive",
        "legacy.server.backup": "old_standby",
        
        # User data with old naming
        "old_user_id": "usr_12345",
        "old_session_token": "token_abc123",
        
        # Deprecated fields
        "deprecated_phone": "deprecated_555-1234",
        "deprecated_address": "deprecated_123 Main St",
        
        # Email with old domain
        "user_email": "john@oldcompany.com",
        "admin_email": "admin@oldcompany.com",
        
        # Fields to be cleaned up
        "empty_field": "",
        "another_empty": "",
        "null_field": None,
        "null_config": None,
        
        # Nested data with similar issues
        "user_profile": {
            "name": "John Doe",
            "old_status": "old_premium",
            "legacy_tier": "legacy_gold",
            "empty_bio": "",
            "null_avatar": None,
            "contact": {
                "deprecated_phone": "deprecated_555-9999",
                "email": "contact@oldcompany.com",
                "empty_fax": "",
                "null_mobile": None
            }
        },
        
        # Valid data that should be preserved
        "user_name": "John Doe",
        "account_balance": 1000.50,
        "is_active": True,
        "login_count": 42
    }
    
    print("🚀 JsonToolsBuilder Example - Complex Chained Operations")
    print("=" * 60)
    
    print("\n📋 Original Data:")
    print(json.dumps(sample_data, indent=2))
    
    print(f"\n📊 Original Data Stats:")
    print(f"   - Total keys: {count_keys(sample_data)}")
    print(f"   - Empty strings: {count_empty_strings(sample_data)}")
    print(f"   - Null values: {count_nulls(sample_data)}")
    
    # Apply the exact transformation pattern requested by the user
    print("\n🔄 Applying Chained Transformations...")
    
    result = (JsonToolsBuilder()
        .add_json(sample_data)
        
        # Clean up empty and null values
        .remove_empty_strings() 
        .remove_nulls()
        
        # Standardize session tracking keys
        .replace_keys(r"^session\.tracking\..*\.timeMs$", "session.pageTimesInMs.UnifiedPage")
        
        # Standardize analytics keys  
        .replace_keys(r"^analytics\.page\..*\.visits$", "analytics.pageViews.TotalPage")
        
        # Modernize legacy server keys
        .replace_keys(r"^legacy\.server\..*$", "modern.server.instance")
        
        # Update old prefixes to new
        .replace_keys(r"^old_", "new_")
        
        # Update deprecated prefixes
        .replace_keys(r"^deprecated", "updated")
        
        # Replace old values with new standardized values
        .replace_values(r"^old_.*$", "new_value")
        
        # Replace legacy values with modern equivalents
        .replace_values(r"^legacy_.*$", "modern_value")
        
        # Replace deprecated values with updated ones
        .replace_values(r"^deprecated_.*$", "updated_value")
        
        # Standardize user ID format
        .replace_values(r"usr_\d+", "user_new_12345")
        
        # Update email domains
        .replace_values(r".*@oldcompany\.com", "john@newcompany.com")
        
        # Enable pretty printing for readable output
        .pretty_print(True)
        
        # Execute all operations
        .build())
    
    print("\n✅ Transformed Data:")
    print(result)
    
    # Parse result for analysis
    result_data = json.loads(result)
    
    print(f"\n📊 Transformed Data Stats:")
    print(f"   - Total keys: {count_keys(result_data)}")
    print(f"   - Empty strings: {count_empty_strings(result_data)}")
    print(f"   - Null values: {count_nulls(result_data)}")
    
    print("\n🎯 Transformation Summary:")
    print("   ✅ Removed all empty string values")
    print("   ✅ Removed all null values") 
    print("   ✅ Standardized session tracking keys")
    print("   ✅ Standardized analytics keys")
    print("   ✅ Modernized legacy server keys")
    print("   ✅ Updated old_ prefixes to new_")
    print("   ✅ Updated deprecated prefixes to updated")
    print("   ✅ Replaced old_ values with new_value")
    print("   ✅ Replaced legacy_ values with modern_value")
    print("   ✅ Replaced deprecated_ values with updated_value")
    print("   ✅ Standardized user ID format")
    print("   ✅ Updated email domains")
    
    print("\n🚀 Example with Flattening:")
    print("-" * 40)
    
    # Demonstrate flattening as well
    flattened_result = (JsonToolsBuilder()
        .add_json(sample_data)
        .remove_empty_strings()
        .remove_nulls()
        .replace_keys(r"^old_", "new_")
        .replace_values(r"^old_.*$", "new_value")
        .flatten()
        .pretty_print(True)
        .build())
    
    print("Flattened and transformed:")
    print(flattened_result)
    
    print("\n🎉 JsonToolsBuilder provides powerful, efficient JSON transformations!")
    print("   💡 All operations are chained and executed efficiently")
    print("   ⚡ Future versions will optimize to single-pass processing")
    print("   🔧 Fluent interface makes complex transformations readable")


def count_keys(data, count=0):
    """Recursively count all keys in nested JSON."""
    if isinstance(data, dict):
        count += len(data)
        for value in data.values():
            count = count_keys(value, count)
    elif isinstance(data, list):
        for item in data:
            count = count_keys(item, count)
    return count


def count_empty_strings(data, count=0):
    """Recursively count empty string values."""
    if isinstance(data, dict):
        for value in data.values():
            if value == "":
                count += 1
            else:
                count = count_empty_strings(value, count)
    elif isinstance(data, list):
        for item in data:
            if item == "":
                count += 1
            else:
                count = count_empty_strings(item, count)
    return count


def count_nulls(data, count=0):
    """Recursively count null values."""
    if isinstance(data, dict):
        for value in data.values():
            if value is None:
                count += 1
            else:
                count = count_nulls(value, count)
    elif isinstance(data, list):
        for item in data:
            if item is None:
                count += 1
            else:
                count = count_nulls(item, count)
    return count


if __name__ == "__main__":
    main()
