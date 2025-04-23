#!/usr/bin/env python3
"""
Project Structure Documentation Generator

This script generates a markdown file documenting the structure and contents of a project,
with formatting optimized for large language models (LLMs).

Just configure the variables at the top of the script and run it.
"""

import os
import fnmatch
from pathlib import Path
import mimetypes
from collections import Counter

# ===== CONFIGURE THESE VARIABLES =====
# Path to the project directory (default: current directory)
PROJECT_DIR = '/Users/andrewmayes/Dev/aimv2-large-patch14-native/aimv2-large-patch14-native'

# Path where the markdown file will be saved
OUTPUT_FILE = 'project_structure.md'

# Patterns to ignore (files and directories)
IGNORE_PATTERNS = [
    "dev", "R1_dataset", "terraform", "__init__.py", ".ipynb", ".lock", "notebooks",

    # Version control
    '.git', '.github', '.gitignore', 
    
    # Python specific
    '__pycache__', '*.pyc', '*.pyo', '*.pyd', '.pytest_cache',
    '.coverage', 'htmlcov', '.tox', '.nox',
    '.venv', 'venv', 'env', '.env', '*.egg-info', 'dist', 'build',
    '*.egg', '.eggs', '.mypy_cache', '.ruff_cache', '.flake8',
    
    # Virtual environments
    '.venv', 'venv', 'env', '.env',
    
    # AWS/CDK specific
    'cdk.out', '.aws-sam', '.chalice', '.serverless',
    'cdk.context.json', 'samconfig.toml',
    
    # Docker
    '.dockerignore', 'docker-compose*.yml',
    
    # Environment files
    '.env*', '*.env', 'env.template',
    
    # IDE
    '.vscode', '.idea', '*.iml', '*.suo', '*.user', 
    '.project', '.classpath', '.settings',
    
    # Temp files
    '*.swp', '*.swo', '*~', '*.bak', '*.tmp', '*.temp', '.DS_Store',
    
    # Media & binary
    '*.jpg', '*.jpeg', '*.png', '*.gif', '*.webp', '*.ico', '*.svg',
    '*.pdf', '*.zip', '*.tar.gz', '*.rar', '*.7z', '*.exe', '*.dll',
    
    # Node.js
    'node_modules', 'package-lock.json', 'yarn.lock',
    
    # Generated files
    '*.min.js', '*.min.css', 'dist', 'build',
    
    # Log files
    '*.log', 'logs'
]

# Maximum directory depth to traverse (None for unlimited)
MAX_DEPTH = None

# Maximum file size to include content (in bytes, None for unlimited)
MAX_FILE_SIZE = 1024 * 500  # 500KB

# Only show structure without file contents
STRUCTURE_ONLY = False

# Skip binary files
SKIP_BINARY = True

# Include summary statistics
INCLUDE_SUMMARY = True
# =====================================

def generate_markdown_for_project(project_dir, output_file, ignore_patterns=None, max_depth=None, 
                                max_file_size=None, structure_only=False, skip_binary=True,
                                include_summary=True):
    """
    Generate a markdown file documenting the project structure and file contents.
    
    Args:
        project_dir (str): Path to the project directory
        output_file (str): Path to the output markdown file
        ignore_patterns (list): List of patterns to ignore (e.g., ['.git', '__pycache__', '*.pyc'])
        max_depth (int): Maximum depth to traverse
        max_file_size (int): Maximum file size in bytes to include content
        structure_only (bool): If True, only show structure without file contents
        skip_binary (bool): If True, skip binary files
        include_summary (bool): If True, include a summary of file types and counts
    """
    if ignore_patterns is None:
        ignore_patterns = IGNORE_PATTERNS
    
    project_path = Path(project_dir).resolve()
    
    # Gather statistics if summary is requested
    file_stats = Counter()
    total_size_tracker = [0] if include_summary else None
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f"# Project Structure: {project_path.name}\n\n")
        f.write("This document provides an overview of the project structure and contents of each file.\n\n")
        
        # Include summary if requested
        if include_summary:
            f.write("## Project Summary\n\n")
            
            # This will be filled after traversing the directory
            summary_placeholder_pos = f.tell()
            f.write("*Summary will be generated after processing...*\n\n")
        
        f.write("## Directory Structure\n\n")
        
        # First, write the directory structure
        dir_count, file_count = write_directory_structure(
            f, project_path, project_path, 0, ignore_patterns, max_depth, file_stats, total_size_tracker
        )
        
        if include_summary:
            # Now that we have stats, go back and write the summary
            current_pos = f.tell()
            f.seek(summary_placeholder_pos)
            
            f.write(f"- **Total Directories:** {dir_count}\n")
            f.write(f"- **Total Files:** {file_count}\n")
            f.write(f"- **Total Size:** {format_size(total_size_tracker[0])}\n\n")
            
            if file_stats:
                f.write("### File Types\n\n")
                for ext, count in sorted(file_stats.items(), key=lambda x: x[1], reverse=True):
                    f.write(f"- **{ext or 'No extension'}:** {count} file(s)\n")
                f.write("\n")
            
            # Return to the end of the file
            f.seek(current_pos)
        
        if not structure_only:
            f.write("\n## File Contents\n\n")
            
            # Then, write the file contents
            write_file_contents(f, project_path, project_path, ignore_patterns, max_depth, max_file_size, skip_binary)
    
    print(f"Markdown documentation generated at: {output_file}")

def should_ignore(path, ignore_patterns):
    """Check if a path should be ignored based on patterns."""
    path_str = str(path)
    path_name = path.name
    
    for pattern in ignore_patterns:
        # Check exact directory/file names
        if pattern == path_name or pattern == path_str:
            return True
        
        # Check glob patterns
        if '*' in pattern or '?' in pattern:
            if fnmatch.fnmatch(path_name, pattern) or fnmatch.fnmatch(path_str, pattern):
                return True
    
    return False

def write_directory_structure(f, base_path, current_path, level, ignore_patterns, max_depth=None, 
                             file_stats=None, total_size_tracker=None):
    """Write the directory structure as a tree in markdown."""
    if max_depth is not None and level > max_depth:
        return 0, 0
    
    indent = '  ' * level
    dir_count = 1  # Count the current directory
    file_count = 0
    
    items = sorted(current_path.iterdir(), key=lambda x: (not x.is_dir(), x.name.lower()))
    
    for item in items:
        if should_ignore(item, ignore_patterns):
            continue
        
        if item.is_symlink():
            target = os.readlink(item)
            f.write(f"{indent}- 🔗 {item.name} -> {target}\n")
            continue
        
        if item.is_dir():
            f.write(f"{indent}- 📁 **{item.name}/**\n")
            sub_dirs, sub_files = write_directory_structure(
                f, base_path, item, level + 1, ignore_patterns, max_depth, 
                file_stats, total_size_tracker
            )
            dir_count += sub_dirs
            file_count += sub_files
        else:
            f.write(f"{indent}- 📄 {item.name}\n")
            file_count += 1
            
            # Update statistics if requested
            if file_stats is not None:
                ext = item.suffix.lower() or "No extension"
                file_stats[ext] += 1
            
            if total_size_tracker is not None:
                try:
                    total_size_tracker[0] += item.stat().st_size
                except (OSError, FileNotFoundError):
                    pass  # Skip files that can't be accessed
    
    return dir_count, file_count

def is_binary_file(file_path):
    """Determine if a file is binary by checking MIME type and content."""
    mime_type, _ = mimetypes.guess_type(str(file_path))
    if mime_type and not mime_type.startswith('text/'):
        return True
    
    try:
        with open(file_path, 'rb') as file:
            chunk = file.read(1024)
            # Check for null bytes which often indicate binary content
            if b'\x00' in chunk:
                return True
            
            # Try to decode as text
            try:
                chunk.decode('utf-8')
                return False
            except UnicodeDecodeError:
                return True
    except (OSError, IOError):
        # If we can't open the file, treat it as binary
        return True

def write_file_contents(f, base_path, current_path, ignore_patterns, max_depth=None, max_file_size=None, skip_binary=True):
    """Write the contents of each file in markdown format."""
    current_level = len(current_path.relative_to(base_path).parts)
    if max_depth is not None and current_level > max_depth:
        return
    
    for item in sorted(current_path.iterdir(), key=lambda x: (not x.is_dir(), x.name.lower())):
        if should_ignore(item, ignore_patterns):
            continue
        
        if item.is_symlink():
            # Skip symlinks for content
            continue
        
        if item.is_dir():
            f.write(f"### 📁 {item.relative_to(base_path)}/\n\n")
            write_file_contents(f, base_path, item, ignore_patterns, max_depth, max_file_size, skip_binary)
        else:
            rel_path = item.relative_to(base_path)
            f.write(f"### 📄 {rel_path}\n\n")
            
            # Get file size
            try:
                file_size = item.stat().st_size
                f.write(f"**Size:** {format_size(file_size)}\n\n")
                
                # Skip if file is too large
                if max_file_size is not None and file_size > max_file_size:
                    f.write(f"*File is too large ({format_size(file_size)}), content not displayed*\n\n")
                    continue
            except (OSError, FileNotFoundError):
                f.write("**Size:** Unable to determine\n\n")
                continue
            
            # Check if file is binary
            if skip_binary and is_binary_file(item):
                f.write("*Binary file, contents not displayed*\n\n")
                continue
            
            try:
                with open(item, 'r', encoding='utf-8') as file:
                    content = file.read()
                
                # Determine the language for the code block based on file extension
                ext = item.suffix.lower()[1:] if item.suffix else ""
                language = get_language_for_extension(ext)
                
                # Truncate very large files to avoid markdown bloat
                if len(content) > 100000:  # ~100KB limit
                    content = content[:97000] + "\n\n... (content truncated) ...\n"
                
                f.write("```" + language + "\n")
                f.write(content)
                f.write("\n```\n\n")
            except Exception as e:
                f.write(f"*Error reading file: {str(e)}*\n\n")

def format_size(size_bytes):
    """Format file size in a human-readable form."""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} TB"

def get_language_for_extension(ext):
    """Map file extension to markdown code block language."""
    extension_map = {
        # Common programming languages
        'py': 'python',
        'js': 'javascript',
        'ts': 'typescript',
        'jsx': 'jsx',
        'tsx': 'tsx',
        'html': 'html',
        'css': 'css',
        'scss': 'scss',
        'sass': 'sass',
        'less': 'less',
        'json': 'json',
        'md': 'markdown',
        'sql': 'sql',
        'sh': 'bash',
        'bash': 'bash',
        'zsh': 'bash',
        'bat': 'batch',
        'cmd': 'batch',
        'ps1': 'powershell',
        'c': 'c',
        'cpp': 'cpp',
        'cc': 'cpp',
        'h': 'c',
        'hpp': 'cpp',
        'java': 'java',
        'rb': 'ruby',
        'go': 'go',
        'php': 'php',
        'rs': 'rust',
        'swift': 'swift',
        'kt': 'kotlin',
        'dart': 'dart',
        
        # Configuration files
        'yaml': 'yaml',
        'yml': 'yaml',
        'toml': 'toml',
        'xml': 'xml',
        'ini': 'ini',
        'cfg': 'ini',
        'config': 'ini',
        'env': 'ini',
        
        # Data files
        'csv': 'csv',
        'ipynb': 'json',
        
        # Other languages
        'r': 'r',
        'lua': 'lua',
        'ex': 'elixir',
        'exs': 'elixir',
        'erl': 'erlang',
        'clj': 'clojure',
        'elm': 'elm',
        'hs': 'haskell',
        'fs': 'fsharp',
        'pl': 'perl',
        'pm': 'perl',
        'scala': 'scala',
        'vim': 'vim',
        'docker': 'dockerfile',
        'dockerfile': 'dockerfile',
        'tf': 'terraform',
        'vue': 'vue',
        'graphql': 'graphql',
        'gql': 'graphql',
        'sol': 'solidity',
    }
    
    return extension_map.get(ext, '')

# Run the script when executed directly
if __name__ == "__main__":
    generate_markdown_for_project(
        PROJECT_DIR,
        OUTPUT_FILE,
        IGNORE_PATTERNS,
        MAX_DEPTH,
        MAX_FILE_SIZE,
        STRUCTURE_ONLY,
        SKIP_BINARY,
        INCLUDE_SUMMARY
    )