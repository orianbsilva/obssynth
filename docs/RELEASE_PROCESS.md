# Release Process Documentation

This document describes the automated release system for the Synth Audio Synthesizer project.

## Overview

The project uses GitHub Actions to automatically build, test, and release the synthesizer for multiple platforms. The system creates downloadable packages with all executables, documentation, and sample files.

## Release Workflow

### Automated Release Creation

When a version tag (e.g., `v1.0.0`) is pushed to the repository, the following happens automatically:

1. **Multi-Platform Build**
   - Windows x64 (MSVC compiler)
   - Linux x64 (Clang compiler)

2. **Quality Assurance**
   - Full compilation test
   - Unit test execution
   - Build artifact validation

3. **Package Creation**
   - All executables bundled
   - Documentation included
   - Version information generated
   - Platform-specific instructions

4. **GitHub Release**
   - Automatic release page creation
   - Download links for all platforms
   - Auto-generated release notes
   - Asset upload and organization

### Release Artifacts

Each release provides two main downloads:

#### Windows Package (`synth-windows-x64.zip`)
Contains:
- All `.exe` files from the build
- Complete `docs/` folder with technical documentation
- `VERSION.txt` with build information and usage instructions
- Sample WAV files (if generated during build)
- `README.md` for quick reference

System Requirements:
- Windows 10 or later
- No additional dependencies required

#### Linux Package (`synth-linux-x64.tar.gz`)
Contains:
- All executable binaries
- Complete `docs/` folder with technical documentation
- `VERSION.txt` with build information and usage instructions
- Sample WAV files (if generated during build)
- `README.md` for quick reference

System Requirements:
- Linux with ALSA support
- Standard C++ runtime libraries

## Creating a Release

### For Maintainers

Use the provided helper scripts to create new releases:

#### Quick Release (Recommended)

**Linux/macOS:**
```bash
./create_release.sh v1.2.3
```

**Windows:**
```cmd
create_release.bat v1.2.3
```

These scripts will:
- Validate the version format
- Check that your working directory is clean
- Create and push the version tag
- Display links to monitor the build progress

#### Manual Release

If you prefer manual control:

```bash
# Ensure you're on main branch with clean working directory
git checkout main
git pull origin main
git status  # Should show "working tree clean"

# Create annotated tag
git tag -a v1.2.3 -m "Release v1.2.3"

# Push tag to trigger release
git push origin v1.2.3
```

### Version Numbering

Follow semantic versioning (SemVer):
- `v1.0.0` - Major release
- `v1.1.0` - Minor release (new features)
- `v1.1.1` - Patch release (bug fixes)
- `v2.0.0-beta` - Pre-release versions

## Build Process Details

### GitHub Actions Workflow

The release workflow (`.github/workflows/release.yml`) performs:

1. **Environment Setup**
   - Checks out the tagged code
   - Installs platform-specific dependencies
   - Configures CMake with Release settings

2. **Compilation**
   - Builds all targets using CMake
   - Uses platform-native compilers (MSVC on Windows, Clang on Linux)
   - Applies Release optimizations

3. **Testing**
   - Runs the full test suite
   - Validates core functionality
   - Ensures audio generation works

4. **Packaging**
   - Copies all built executables
   - Includes documentation and examples
   - Creates version information files
   - Generates platform-appropriate archives

5. **Release Creation**
   - Uploads build artifacts
   - Creates GitHub release page
   - Generates release notes with feature list
   - Links download packages

### Continuous Integration

Regular CI builds (`.github/workflows/cmake-multi-platform.yml`) test every commit:
- Multi-compiler support (GCC, Clang, MSVC)
- Cross-platform validation
- Artifact preservation for testing
- Integration with pull request reviews

## Monitoring Releases

### Build Status

Monitor release progress at:
- GitHub Actions tab: `https://github.com/orianbsilva/obssynth/actions`
- Specific workflow: Look for "Build and Release" workflow runs

### Release Page

Completed releases appear at:
- Releases page: `https://github.com/orianbsilva/obssynth/releases`
- Latest release: `https://github.com/orianbsilva/obssynth/releases/latest`

## Troubleshooting

### Common Issues

**Tag already exists:**
```bash
# Delete local and remote tag if needed
git tag -d v1.0.0
git push origin --delete v1.0.0
```

**Build failure:**
- Check the Actions tab for detailed error logs
- Common causes: compilation errors, missing dependencies, test failures
- Fix issues and create a new patch version

**Missing artifacts:**
- Verify the tag triggered the release workflow
- Check that both Windows and Linux builds completed successfully
- Ensure GitHub repository has Actions enabled

### Build Dependencies

The automated builds require these system dependencies:

**Linux:**
- `libasound2-dev` (ALSA development headers)
- `libpulse-dev` (PulseAudio development headers)
- `pkg-config` (Package configuration tool)

**Windows:**
- Windows SDK (for WinMM libraries)
- MSVC compiler toolchain
- No additional dependencies

## Release History

Releases follow this naming convention:
- Major versions for significant architecture changes
- Minor versions for new features and effects
- Patch versions for bug fixes and performance improvements

Each release includes:
- Changelog of features and fixes
- Compatibility notes
- Build information and checksums
- Documentation updates

## Security

- All builds run in isolated GitHub Actions environments
- Source code is validated before building
- Artifacts are automatically scanned for common issues
- Release assets are served directly from GitHub's secure CDN

For questions about the release process, see the main project documentation or open an issue on GitHub.
