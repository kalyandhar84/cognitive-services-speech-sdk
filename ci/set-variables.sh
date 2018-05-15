#!/bin/bash
# Set variables based on source commit and requested options.
# This is only run in VSTS currently.
#
# Inputs:
# * version.txt in the repository directory
# * Variables coming from the build definition:
#   * $RUNPHASE - build options
# * Various VSTS build variables
#   * $BUILD_BUILDID
#   * $BUILD_REASON
#   * $BUILD_SOURCEBRANCH
#   * $SYSTEM_COLLECTIONID
#   * $SYSTEM_DEFINITIONID
#   * $SYSTEM_PULLREQUEST_PULLREQUESTID
#
# Outputs:
# * SPEECHSDK_MAIN_BUILD - equal to "true" if running from our main build
#   definition (4062), "false" otherwise.
# * SPEECHSDK_BUILD_TYPE - can be "dev", "int", "prod", which (roughly) correspond to
#   dev-box / PR / feature-branch, nightly, release-branch builds. "int" and "prod"
#   can only come from the main build definition (i.e., no clone, and no draft).
# * SPEECHSDK_SEMVER2 - semver 2 version without build meta data (commit ID)
#   The version number is determined from the contents of the version.txt
#   in the repository root, as well as the build type. The build types
#   dev and int correspond to alpha and beta prerelease version (the associated is the
#   VSTS build number). For prod builds, version.txt must fully specify the
#   version (without build meta data). For releases, it is expected that a
#   release branch is created and version.txt is edited on the path to the
#   release.
#   For example: release/0.3.0 is created, version.txt contents are changed to
#   "0.3.0-rc1", and further along to "0.3.0-rc2", and finally to "0.3.0" from which the
#   release can be created./
# * SPEECHSDK_SEMVER2NOMETA - same as above, without build meta data
#   We are currently using this (and not SPEECHSDK_SEMVER2) for NuGet packages,
#   since VSTS package management does not support build meta information.
# * SPEECHSDK_SIGN - equal to "true" if should sign, "false" otherwise.
#   For testing signing, if RUNPHASE contains SIGN, this is set to "true".
# * SPEECHSDK_NUGET_VSTS_PUSH - equal to "true" if a push to (one of) our internal
#   VSTS packagement feeds should be made.
# * SPEECHSDK_BUILD_AGENT_PLATFORM - can be "Windows-x64", "OSX-x64", "Linux-x64"
#
# Override mechanism:
# Any of the outputs can be overridden. To override output X, specify a
# variable OVERRIDE_X with the desired override value. Note this isn't exposed
# at the build definition level, but it could be (in important enough cases),
# without source change. Overrides only happen before outputting and do not
# change the flow of the script.
#

SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
SOURCE_ROOT="$SCRIPT_DIR/.."

# Get some helpers
. "$SCRIPT_DIR/functions.sh"

set -x -e -o pipefail

# Determine build agent platform
case $(uname -a) in
  Linux*x86_64\ GNU/Linux)
    SPEECHSDK_BUILD_AGENT_PLATFORM=Linux-x64
    ;;
  MINGW64_NT-*x86_64\ Msys)
    SPEECHSDK_BUILD_AGENT_PLATFORM=Windows-x64
    ;;
  Darwin*\ x86_64)
    SPEECHSDK_BUILD_AGENT_PLATFORM=OSX-x64
    ;;
  *)
    echo Unexpected build agent platform:
    uname -a
    exit 1
    ;;
esac

VERSION="$(cat "$SCRIPT_DIR/../version.txt")"

# Must be a major.minor.patch version
echo VERSION=$VERSION

# Determine the build type, ID, and commit

SPEECHSDK_BUILD_TYPE=dev

IN_VSTS=$([[ -n $SYSTEM_DEFINITIONID && -n $SYSTEM_COLLECTIONID ]] && echo true || echo false)

if $IN_VSTS; then
  # We're running in VSTS

  MAIN_BUILD_DEF=19422243-19b9-4d85-9ca6-bc961861d287/4062

  SPEECHSDK_MAIN_BUILD=$([[ $SYSTEM_COLLECTIONID/$SYSTEM_DEFINITIONID == $MAIN_BUILD_DEF ]] && echo true || echo false)

  if [[ $SPEECHSDK_MAIN_BUILD ]]; then
    # Non-draft build definition

    if [[ $BUILD_SOURCEBRANCH == refs/heads/release/* ]]; then
      SPEECHSDK_BUILD_TYPE=prod
    elif [[ $BUILD_SOURCEBRANCH == refs/heads/master && ( $BUILD_REASON == Schedule || $BUILD_REASON == Manual ) ]]; then
      SPEECHSDK_BUILD_TYPE=int
    fi
  fi
  if [[ $BUILD_REASON == PullRequest ]]; then
    _BUILD_COMMIT=pr$SYSTEM_PULLREQUEST_PULLREQUESTID
  else
    _BUILD_COMMIT=
  fi
  _BUILD_COMMIT+=$(echo $BUILD_SOURCEVERSION | cut -c 1-8)
  _BUILD_ID=$BUILD_BUILDID
else
  # Dev box
  _BUILD_ID=$(date -u +%Y%m%d%H%M%S)
  _BUILD_COMMIT=$(git rev-parse --short HEAD)
  SPEECHSDK_MAIN_BUILD=false
fi

if [[ $SPEECHSDK_BUILD_TYPE != prod && ! $VERSION =~ ^([0-9]+\.){2}[0-9]+$ ]]; then
  echo Invalid version, should be MAJOR.MINOR.PATCH: $VERSION
  exit 1
elif [[ ! $VERSION =~ ^([0-9]+\.){2}[0-9]+(-(alpha|beta|rc)\.[0-9]+)?$ ]]; then
  echo Invalid version, should be MAJOR.MINOR.PATCH with optional alpha/beta/rc pre-release: $VERSION
  exit 1
fi

case $SPEECHSDK_BUILD_TYPE in
  dev)
    PRERELEASE_VERSION=-alpha.0.$_BUILD_ID
    META=+$_BUILD_COMMIT
    SPEECHSDK_SIGN=false
    SPEECHSDK_NUGET_VSTS_PUSH=false
    ;;
  int)
    PRERELEASE_VERSION=-beta.0.$_BUILD_ID
    META=+$_BUILD_COMMIT
    SPEECHSDK_SIGN=true
    SPEECHSDK_NUGET_VSTS_PUSH=true
    ;;
  prod)
    # Prod builds take exactly the version from version.txt, no extra
    # pre-release or meta.
    PRERELEASE_VERSION=
    META=
    SPEECHSDK_SIGN=true
    SPEECHSDK_NUGET_VSTS_PUSH=true
    ;;
esac

# Set SPEECHSDK_SIGN to true if explicitly requested
if [[ "$RUNPHASE" = *Sign* || "$RUNPHASE" = *All* ]]; then
  SPEECHSDK_SIGN=true
fi

# Set SPEECHSDK_NUGET_VSTS_PUSH to true if explicitly requested
if [[ "$RUNPHASE" = *WindowsNuGetPush* || "$RUNPHASE" = *All* ]]; then
  SPEECHSDK_NUGET_VSTS_PUSH=true
fi

SPEECHSDK_SEMVER2NOMETA="$VERSION$PRERELEASE_VERSION"
SPEECHSDK_SEMVER2="$SPEECHSDK_SEMVER2NOMETA$META"

set +x

# Note: VSTS package management does not (yet?) support build meta, so upstream
# build definition will pickup SPEECHSDK_SEMVER2NOMETA for the NuGet.

for var in \
  SPEECHSDK_MAIN_BUILD \
  SPEECHSDK_BUILD_TYPE \
  SPEECHSDK_SEMVER2 \
  SPEECHSDK_SEMVER2NOMETA \
  SPEECHSDK_SIGN \
  SPEECHSDK_NUGET_VSTS_PUSH \
  SPEECHSDK_BUILD_AGENT_PLATFORM \
  ; \
do
  overrideVar=OVERRIDE_$var
  overrideValue="${!overrideVar}"

  vsts_setvar $var "${overrideValue:-${!var}}"
done
