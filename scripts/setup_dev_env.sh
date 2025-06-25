#!/bin/bash
# Development Environment Setup Script for cJSON-Tools
# Installs all necessary tools for automated code formatting and development

set -e

echo "🛠️  Setting up cJSON-Tools Development Environment"
echo "================================================="

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is required but not installed."
    exit 1
fi

echo "✅ Python 3 found: $(python3 --version)"

# Install Python development tools
echo "📦 Installing Python development tools..."
python3 -m pip install --upgrade pip
python3 -m pip install black isort flake8 pytest pre-commit

# Install the package in development mode
echo "📦 Installing cJSON-Tools in development mode..."
cd py-lib
python3 -m pip install -e .
cd ..

# Install pre-commit hooks
echo "🪝 Installing pre-commit hooks..."
pre-commit install

# Run initial formatting
echo "🎨 Running initial code formatting..."
make format

echo ""
echo "✅ Development environment setup complete!"
echo ""
echo "🎯 Available commands:"
echo "  make format       - Format all Python code"
echo "  make format-check - Check code formatting"
echo "  make lint         - Run linting checks"
echo "  make dev-install  - Reinstall dev tools"
echo "  make setup-hooks  - Reinstall pre-commit hooks"
echo ""
echo "🔧 Pre-commit hooks installed - code will be auto-formatted on commit!"
echo "💡 VS Code settings configured for auto-formatting on save"
echo ""
echo "🚀 Ready for development!"
