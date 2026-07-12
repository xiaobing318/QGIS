#!/usr/bin/env bash

resolve_platform_info() {
  local repo_root=$1
  local os_name="macOS"
  local os_version
  local raw_arch
  local isa
  local platform_key=""

  if command -v sw_vers >/dev/null 2>&1; then
    os_version=$(sw_vers -productVersion 2>/dev/null || uname -sr)
  else
    os_version=$(uname -sr 2>/dev/null || printf 'macOS')
  fi

  raw_arch=$(uname -m 2>/dev/null || printf 'unknown')
  case "$raw_arch" in
    arm64|aarch64) isa="ARM64" ;;
    x86_64|amd64) isa="AMD64" ;;
    *) isa="$raw_arch" ;;
  esac

  case "$isa" in
    ARM64) platform_key="QGIS-macOS-ARM64" ;;
  esac

  PLATFORM_INFO_JSON="{\"OS\":$(json_string "$os_name"),\"OSVersion\":$(json_string "$os_version"),\"ISA\":$(json_string "$isa"),\"RawArchitecture\":$(json_string "$raw_arch")}"
  PLATFORM_KEY="$platform_key"
}
