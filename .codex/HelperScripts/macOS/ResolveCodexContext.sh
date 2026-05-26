#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CONFIG_PATH="$SCRIPT_ROOT/ResolveCodexContext.json"
OUTPUT_JSON_PATH=""
NO_WRITE_FILE=true
PLATFORM_INFO_SCRIPT="ResolvePlatformInfo.sh"
QGIS_COMPILATION_NAVIGATION_SCRIPT="ResolveQGISCompilationNavigation.sh"

# shellcheck source=../Shared/ShellJsonHelpers.sh
. "$SCRIPT_ROOT/../Shared/ShellJsonHelpers.sh"

DEFAULT_ENABLED_SCRIPTS=("$PLATFORM_INFO_SCRIPT" "$QGIS_COMPILATION_NAVIGATION_SCRIPT")
ALLOWED_ENABLED_SCRIPTS=("$PLATFORM_INFO_SCRIPT" "$QGIS_COMPILATION_NAVIGATION_SCRIPT")
ENABLED_SCRIPTS=()

print_help() {
  cat <<'EOF'
ResolveCodexContext.sh

Usage:
  bash .codex/HelperScripts/macOS/ResolveCodexContext.sh [--help]
  bash .codex/HelperScripts/macOS/ResolveCodexContext.sh [--config <path>] [--outputJsonPath <path>]

Parameters:
  --help                  Show this help message and exit.
  --config <path>         Read the specified JSON config file. Defaults to ResolveCodexContext.json next to this script.
  --outputJsonPath <path> Write the JSON result to the specified path. Existing files are overwritten. Defaults to terminal output only.

Config:
  EnabledScripts controls which discovery scripts run. Allowed values:
    ResolvePlatformInfo.sh
    ResolveQGISCompilationNavigation.sh

EOF
}

is_absolute_path() {
  local value=${1-}
  [[ "$value" = /* || "$value" =~ ^[A-Za-z]:[\\/] ]]
}

resolve_user_path() {
  local value=$1
  if is_absolute_path "$value"; then
    printf '%s' "$value"
  else
    printf '%s/%s' "$(pwd -P)" "$value"
  fi
}

write_context_output() {
  local context_json=$1
  local output_json_path=${2-}
  if [[ -n "$output_json_path" ]]; then
    local resolved_output
    resolved_output=$(resolve_user_path "$output_json_path")
    mkdir -p "$(dirname "$resolved_output")"
    printf '%s\n' "$context_json" > "$resolved_output"
  fi
  printf '%s\n' "$context_json"
}

write_invalid_input_and_exit() {
  local code=$1
  local message=$2
  local output_json_path=${3-}
  local json
  json="{\"Status\":\"InvalidInput\",\"Messages\":[{\"Level\":\"Error\",\"Code\":$(json_string "$code"),\"Text\":$(json_string "$message")}]}";
  write_context_output "$json" "$output_json_path"
  exit 2
}

assert_path_argument() {
  local name=$1
  local value=${2-}
  if [[ -z "$value" ]]; then
    write_invalid_input_and_exit "ParameterValidationFailed" "$name requires a non-empty value."
  fi
  if [[ "$value" == --* ]]; then
    write_invalid_input_and_exit "ParameterValidationFailed" "$name requires a value, but received another parameter name."
  fi
}

validate_output_json_path() {
  local value=$1
  local resolved_output
  local output_directory
  resolved_output=$(resolve_user_path "$value")
  if [[ -d "$resolved_output" ]]; then
    write_invalid_input_and_exit "OutputPathInvalid" "--outputJsonPath must be a file path, not a directory: $resolved_output"
  fi
  output_directory=$(dirname "$resolved_output")
  if [[ -e "$output_directory" && ! -d "$output_directory" ]]; then
    write_invalid_input_and_exit "OutputPathInvalid" "--outputJsonPath parent exists but is not a directory: $output_directory"
  fi
}

script_name_allowed() {
  local name=$1
  local allowed
  for allowed in "${ALLOWED_ENABLED_SCRIPTS[@]}"; do
    [[ "$name" == "$allowed" ]] && return 0
  done
  return 1
}

script_enabled() {
  local name=$1
  local enabled
  for enabled in "${ENABLED_SCRIPTS[@]}"; do
    [[ "$name" == "$enabled" ]] && return 0
  done
  return 1
}

load_enabled_scripts() {
  local value
  local seen="|"
  ENABLED_SCRIPTS=()

  if config_has_key "$CONFIG_PATH" "EnabledScripts"; then
    if ! config_key_is_array "$CONFIG_PATH" "EnabledScripts"; then
      write_invalid_input_and_exit "EnabledScriptsInvalid" "EnabledScripts must be an array of strings." "$OUTPUT_JSON_PATH"
    fi
    while IFS= read -r value; do
      [[ -n "$value" ]] && ENABLED_SCRIPTS+=("$value")
    done < <(config_array_values "$CONFIG_PATH" "EnabledScripts")
  else
    ENABLED_SCRIPTS=("${DEFAULT_ENABLED_SCRIPTS[@]}")
  fi

  for value in "${ENABLED_SCRIPTS[@]}"; do
    if ! script_name_allowed "$value"; then
      write_invalid_input_and_exit "EnabledScriptsInvalid" "Unsupported EnabledScripts value: $value" "$OUTPUT_JSON_PATH"
    fi
    if [[ "$seen" == *"|$value|"* ]]; then
      write_invalid_input_and_exit "EnabledScriptsInvalid" "Duplicate EnabledScripts value: $value" "$OUTPUT_JSON_PATH"
    fi
    seen="$seen$value|"
  done
}

if [[ $# -eq 1 && "${1-}" == "--help" ]]; then
  print_help
  exit 0
fi

seen_config=false
seen_output_json_path=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help)
      write_invalid_input_and_exit "ParameterValidationFailed" "--help cannot be combined with other parameters."
      ;;
    --config)
      if [[ "$seen_config" == true ]]; then
        write_invalid_input_and_exit "ParameterValidationFailed" "Duplicate --config parameter."
      fi
      if [[ $# -lt 2 ]]; then
        write_invalid_input_and_exit "ParameterValidationFailed" "--config requires a value."
      fi
      assert_path_argument "--config" "$2"
      CONFIG_PATH=$(resolve_user_path "$2")
      seen_config=true
      shift 2
      ;;
    --outputJsonPath)
      if [[ "$seen_output_json_path" == true ]]; then
        write_invalid_input_and_exit "ParameterValidationFailed" "Duplicate --outputJsonPath parameter."
      fi
      if [[ $# -lt 2 ]]; then
        write_invalid_input_and_exit "ParameterValidationFailed" "--outputJsonPath requires a value."
      fi
      assert_path_argument "--outputJsonPath" "$2"
      validate_output_json_path "$2"
      OUTPUT_JSON_PATH=$2
      NO_WRITE_FILE=false
      seen_output_json_path=true
      shift 2
      ;;
    *)
      write_invalid_input_and_exit "ParameterValidationFailed" "Unsupported parameter or positional argument: $1"
      ;;
  esac
done

if [[ ! -f "$CONFIG_PATH" ]]; then
  write_invalid_input_and_exit "ConfigNotFound" "Config file not found: $CONFIG_PATH" "$OUTPUT_JSON_PATH"
fi

. "$SCRIPT_ROOT/$PLATFORM_INFO_SCRIPT"
. "$SCRIPT_ROOT/$QGIS_COMPILATION_NAVIGATION_SCRIPT"

load_enabled_scripts

REPO_ROOT=$(pwd -P)
NAVIGATION_ROOT=""
SEARCH_ROOTS=()
REQUIRED_CHILDREN=(
  "CommonMaterials"
  "QGIS-Linux-AMD64"
  "QGIS-Linux-ARM64"
  "QGIS-macOS-ARM64"
  "QGIS-Windows-AMD64"
  "QGIS-Windows-ARM64"
)
if config_has_key "$CONFIG_PATH" "RequiredChildren"; then
  REQUIRED_CHILDREN=()
  while IFS= read -r value; do REQUIRED_CHILDREN+=("$value"); done < <(config_array_values "$CONFIG_PATH" "RequiredChildren")
fi
PRUNE_DIRECTORY_NAMES=(
  "dev"
  "System"
  "private"
  "Volumes"
  ".Spotlight-V100"
  ".Trashes"
  ".git"
  "node_modules"
)
if config_has_key "$CONFIG_PATH" "PruneDirectoryNames"; then
  PRUNE_DIRECTORY_NAMES=()
  while IFS= read -r value; do PRUNE_DIRECTORY_NAMES+=("$value"); done < <(config_array_values "$CONFIG_PATH" "PruneDirectoryNames")
fi
CONFIG_SEARCH_ROOTS=()
while IFS= read -r value; do CONFIG_SEARCH_ROOTS+=("$value"); done < <(config_array_values "$CONFIG_PATH" "SearchRoots")
MAX_CANDIDATES=$(config_string "$CONFIG_PATH" "MaxCandidates" "20")
DEFAULT_SEARCH_MODE=$(config_string "$CONFIG_PATH" "DefaultSearchMode" "AllFileSystemsAndVolumes")

if [[ ${#CONFIG_SEARCH_ROOTS[@]} -gt 0 ]]; then
  SEARCH_ROOTS=("${CONFIG_SEARCH_ROOTS[@]}")
fi

PLATFORM_INFO_JSON="{}"
NAVIGATION_PATH=""
NAVIGATION_SOURCE=""
NAVIGATION_TEST_PATH=false
CANDIDATES_JSON="[]"
NAVIGATION_MESSAGES_JSON="[]"
PLATFORM_KEY=""
NAVIGATION_STATUS="Ok"

STATUS="Ok"
FIELDS=()
MESSAGES=()

if script_enabled "$PLATFORM_INFO_SCRIPT"; then
  resolve_platform_info "$REPO_ROOT"
  FIELDS+=("\"PlatformInfo\":$PLATFORM_INFO_JSON")
  [[ -z "$PLATFORM_KEY" ]] && STATUS="Unsupported"
fi

if script_enabled "$QGIS_COMPILATION_NAVIGATION_SCRIPT"; then
  resolve_qgis_compilation_navigation "$NAVIGATION_ROOT" "$DEFAULT_SEARCH_MODE" "${SEARCH_ROOTS[@]}"
  if [[ "$STATUS" == "Ok" ]]; then
    case "$NAVIGATION_STATUS" in
      Ok|Ambiguous|NotFound) STATUS="$NAVIGATION_STATUS" ;;
      *) STATUS="NotFound" ;;
    esac
  fi
  FIELDS+=("\"Placeholders\":{\"__QGISCompilationNavigation__\":{\"Path\":$(json_string "$NAVIGATION_PATH"),\"TestPath\":$(json_bool "$NAVIGATION_TEST_PATH"),\"Source\":$(json_string "$NAVIGATION_SOURCE")}}")
  if [[ "$STATUS" != "Ok" && "$CANDIDATES_JSON" != "[]" ]]; then
    FIELDS+=("\"Candidates\":$CANDIDATES_JSON")
  fi
  if [[ "$STATUS" != "Ok" && "$NAVIGATION_MESSAGES_JSON" != "[]" ]]; then
    MESSAGES+=("$NAVIGATION_MESSAGES_JSON")
  fi
fi

if [[ ${#ENABLED_SCRIPTS[@]} -eq 0 ]]; then
  MESSAGES+=("[{\"Level\":\"Info\",\"Code\":\"NoDiscoveryScriptsEnabled\",\"Text\":\"EnabledScripts is empty; no discovery scripts were executed.\"}]")
fi

FIELDS=("\"Status\":$(json_string "$STATUS")" "${FIELDS[@]}")
if [[ ${#MESSAGES[@]} -gt 0 ]]; then
  FIELDS+=("\"Messages\":$(join_json_arrays "${MESSAGES[@]}")")
fi

CONTEXT_JSON="{
$(join_json_items "${FIELDS[@]}")
}"

if [[ "$NO_WRITE_FILE" == true ]]; then
  write_context_output "$CONTEXT_JSON"
else
  write_context_output "$CONTEXT_JSON" "$OUTPUT_JSON_PATH"
fi
