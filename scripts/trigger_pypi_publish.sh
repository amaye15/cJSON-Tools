#!/bin/bash

# Automated PyPI Publishing Trigger for cJSON-Tools
# This script creates a git tag and pushes it, which automatically triggers
# the GitHub Actions workflow to build and publish to PyPI.

set -e  # Exit on any error

VERSION="1.4.0"
TAG="v${VERSION}"

echo "🚀 cJSON-Tools Automated PyPI Publishing"
echo "========================================"
echo ""
echo "📦 Version: ${VERSION}"
echo "🏷️  Tag: ${TAG}"
echo "🔗 Will publish to: https://pypi.org/project/cjson-tools/"
echo ""

# Check if we're in the right directory
if [ ! -f "py-lib/setup.py" ]; then
    echo "❌ Error: Not in cJSON-Tools root directory"
    echo "   Please run this script from the project root"
    exit 1
fi

# Check if we're on main branch
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ]; then
    echo "⚠️  Warning: Not on main branch (current: $CURRENT_BRANCH)"
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Aborted by user"
        exit 1
    fi
fi

# Check if tag already exists
if git tag -l | grep -q "^${TAG}$"; then
    echo "⚠️  Tag ${TAG} already exists"
    read -p "Delete existing tag and recreate? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🗑️  Deleting existing tag..."
        git tag -d "${TAG}" || true
        git push origin ":refs/tags/${TAG}" || true
    else
        echo "❌ Aborted by user"
        exit 1
    fi
fi

# Ensure we have the latest changes
echo "🔄 Fetching latest changes..."
git fetch origin

# Check if local main is up to date
LOCAL_COMMIT=$(git rev-parse main)
REMOTE_COMMIT=$(git rev-parse origin/main)

if [ "$LOCAL_COMMIT" != "$REMOTE_COMMIT" ]; then
    echo "⚠️  Local main branch is not up to date with remote"
    echo "   Local:  $LOCAL_COMMIT"
    echo "   Remote: $REMOTE_COMMIT"
    read -p "Pull latest changes? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "⬇️  Pulling latest changes..."
        git pull origin main
    else
        echo "❌ Aborted by user"
        exit 1
    fi
fi

# Confirm publication
echo ""
echo "📋 Ready to trigger PyPI publication:"
echo "   • Create git tag: ${TAG}"
echo "   • Push tag to GitHub"
echo "   • GitHub Actions will automatically:"
echo "     - Build wheels for multiple platforms"
echo "     - Test the built packages"
echo "     - Publish to PyPI using your credentials"
echo ""
read -p "🚀 Proceed with automated PyPI publishing? (y/N): " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "❌ Publication cancelled by user"
    exit 1
fi

# Create and push the tag
echo ""
echo "🏷️  Creating git tag ${TAG}..."
git tag -a "${TAG}" -m "Release cJSON-Tools v${VERSION}

🚀 Major Stability & Quality Improvements

✅ Key Features:
- Fixed critical memory access violation in SIMD operations
- Integrated Black code formatter with comprehensive configuration  
- Resolved all GitHub Actions workflow failures (26+ checks passing)
- Enhanced cross-platform reliability (Ubuntu, macOS, Python 3.8-3.12)
- Improved developer experience with formatting tools and pre-commit hooks

✅ Technical Improvements:
- Memory safety: SIMD bounds checking prevents reading past boundaries
- Code quality: Black + isort integration across entire codebase
- Performance: All optimizations maintained while improving safety
- CI/CD: Robust pipeline with comprehensive testing

This release establishes a solid foundation for future development
with enhanced stability, code quality, and developer experience.

Full changelog: https://github.com/amaye15/cJSON-Tools/blob/main/CHANGELOG.md"

echo "📤 Pushing tag to GitHub..."
git push origin "${TAG}"

echo ""
echo "🎉 Tag pushed successfully!"
echo ""
echo "🤖 GitHub Actions workflow will now automatically:"
echo "   1. Build wheels for multiple platforms (Ubuntu, macOS, Windows)"
echo "   2. Build source distribution"
echo "   3. Test all built packages"
echo "   4. Publish to PyPI using your credentials"
echo ""
echo "📊 Monitor progress at:"
echo "   https://github.com/amaye15/cJSON-Tools/actions/workflows/publish.yml"
echo ""
echo "📦 Once complete, install with:"
echo "   pip install cjson-tools==${VERSION}"
echo ""
echo "🔗 View on PyPI:"
echo "   https://pypi.org/project/cjson-tools/"
echo ""
echo "⏱️  Expected completion time: 20-30 minutes"
echo ""
echo "✅ Automated PyPI publishing initiated!"
