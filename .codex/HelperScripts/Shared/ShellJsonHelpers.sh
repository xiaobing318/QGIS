#!/usr/bin/env bash

json_escape() {
  local value=${1-}
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  value=${value//$'\n'/\\n}
  value=${value//$'\r'/\\r}
  value=${value//$'\t'/\\t}
  printf '%s' "$value"
}

json_string() {
  printf '"%s"' "$(json_escape "${1-}")"
}

json_bool() {
  if [[ "${1-}" == "true" || "${1-}" == "1" ]]; then
    printf 'true'
  else
    printf 'false'
  fi
}

config_string() {
  local config_path=$1
  local key=$2
  local default_value=${3-}
  local line
  line=$(grep -m 1 -E "\"$key\"[[:space:]]*:" "$config_path" 2>/dev/null || true)
  if [[ -z "$line" ]]; then
    printf '%s' "$default_value"
    return
  fi
  line=${line#*:}
  line=${line%%,*}
  line=${line//\"/}
  line=$(printf '%s' "$line" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
  printf '%s' "$line"
}

config_has_key() {
  local config_path=$1
  local key=$2
  grep -q -E "\"$key\"[[:space:]]*:" "$config_path" 2>/dev/null
}

config_key_is_array() {
  local config_path=$1
  local key=$2
  awk -v key="\"$key\"" '
    $0 ~ key && $0 ~ /:/ {
      sub(/^.*:/, "")
      if ($0 ~ /\[/) exit 0
      exit 1
    }
    END {
      if (NR == 0) exit 1
    }
  ' "$config_path" 2>/dev/null
}

config_array_values() {
  local config_path=$1
  local key=$2
  awk -v key="\"$key\"" '
    function emit_values(text, value_count, values, i, value) {
      gsub(/^[^\[]*\[/, "", text)
      gsub(/\].*$/, "", text)
      value_count = split(text, values, ",")
      for (i = 1; i <= value_count; i++) {
        value = values[i]
        gsub(/[",]/, "", value)
        gsub(/^[ \t]+|[ \t]+$/, "", value)
        if (value != "") print value
      }
    }
    $0 ~ key && $0 ~ /\[/ && $0 ~ /\]/ {
      emit_values($0)
      next
    }
    $0 ~ key && $0 ~ /\[/ {
      in_array=1
      next
    }
    in_array && /\]/ {
      in_array=0
      next
    }
    in_array {
      gsub(/[",]/, "")
      gsub(/^[ \t]+|[ \t]+$/, "")
      if ($0 != "") print $0
    }
  ' "$config_path" 2>/dev/null || true
}

join_json_arrays() {
  local first_array=true
  local array_json
  printf '['
  for array_json in "$@"; do
    array_json=${array_json#[}
    array_json=${array_json%]}
    [[ -z "$array_json" ]] && continue
    if [[ "$first_array" == true ]]; then
      first_array=false
    else
      printf ','
    fi
    printf '%s' "$array_json"
  done
  printf ']'
}

join_json_items() {
  local first=true
  local item
  for item in "$@"; do
    if [[ "$first" == true ]]; then
      first=false
    else
      printf ',\n'
    fi
    printf '  %s' "$item"
  done
}
