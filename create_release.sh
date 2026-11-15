#!/bin/bash

# Release helper script for Synth Audio Synthesizer
# Usage: ./create_release.sh <version>
# Example: ./create_release.sh v1.0.0

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 v1.0.0"
    exit 1
fi

VERSION=$1

# Validate version format
if [[ ! $VERSION =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+)?$ ]]; then
    echo "Error: Version must be in format v1.2.3 or v1.2.3-alpha"
    echo "Examples: v1.0.0, v1.2.3, v2.0.0-beta"
    exit 1
fi

echo "Creating release $VERSION for Synth Audio Synthesizer"

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "Error: Not in a git repository"
    exit 1
fi

# Check if working directory is clean
if ! git diff-index --quiet HEAD --; then
    echo "Error: Working directory is not clean. Please commit your changes first."
    git status --porcelain
    exit 1
fi

# Check if we're on main branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$CURRENT_BRANCH" != "main" ]; then
    echo "Warning: You're not on the main branch (current: $CURRENT_BRANCH)"
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if tag already exists
if git rev-parse "$VERSION" >/dev/null 2>&1; then
    echo "Error: Tag $VERSION already exists"
    exit 1
fi

# Create and push tag
echo "Creating and pushing tag $VERSION..."
git tag -a "$VERSION" -m "Release $VERSION"
git push origin "$VERSION"

echo "✅ Release $VERSION created successfully!"
echo ""
echo "The GitHub Action will now:"
echo "1. Build the project for Windows and Linux"
echo "2. Run tests to ensure quality"
echo "3. Package build artifacts with documentation"
echo "4. Create a GitHub release with download links"
echo ""
echo "You can monitor the progress at:"
echo "https://github.com/$(git config remote.origin.url | sed 's/.*://g' | sed 's/.git$//g')/actions"
echo ""
echo "The release will be available at:"
echo "https://github.com/$(git config remote.origin.url | sed 's/.*://g' | sed 's/.git$//g')/releases/tag/$VERSION"
