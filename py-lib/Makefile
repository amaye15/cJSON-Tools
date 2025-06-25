# Makefile for cJSON-Tools Python package

.PHONY: help install install-dev format format-check lint test clean build

help:
	@echo "Available commands:"
	@echo "  install      - Install package in development mode"
	@echo "  install-dev  - Install development dependencies"
	@echo "  format       - Format code with Black"
	@echo "  format-check - Check if code is formatted correctly"
	@echo "  lint         - Run linting checks"
	@echo "  test         - Run tests"
	@echo "  clean        - Clean build artifacts"
	@echo "  build        - Build package"

install:
	pip install -e .

install-dev:
	pip install -r requirements-dev.txt
	pip install -e .

format:
	black .
	isort --profile=black .

format-check:
	black --check --diff .
	isort --profile=black --check-only --diff .

lint:
	flake8 .
	mypy cjson_tools/

test:
	python -m pytest tests/ -v

clean:
	rm -rf build/
	rm -rf dist/
	rm -rf *.egg-info/
	find . -type d -name __pycache__ -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete

build: clean
	python -m build
