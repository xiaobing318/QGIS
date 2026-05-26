#!/usr/bin/env bash

validate_navigation_root() {
  local candidate_path=$1
  local source=$2
  local root_exists=false
  local all_children=true
  local child_json_items=()
  local child child_path child_exists

  [[ -d "$candidate_path" ]] && root_exists=true
  for child in "${REQUIRED_CHILDREN[@]}"; do
    child_path="$candidate_path/$child"
    child_exists=false
    [[ -d "$child_path" ]] && child_exists=true
    [[ "$child_exists" == true ]] || all_children=false
    child_json_items+=("{\"Name\":$(json_string "$child"),\"Path\":$(json_string "$child_path"),\"TestPath\":$(json_bool "$child_exists")}")
  done

  local test_path=false
  if [[ "$root_exists" == true && "$all_children" == true ]]; then
    test_path=true
  fi

  printf '{"Path":%s,"Source":%s,"TestPath":%s,"RootExists":%s,"RequiredChildren":[%s]}' \
    "$(json_string "$candidate_path")" \
    "$(json_string "$source")" \
    "$(json_bool "$test_path")" \
    "$(json_bool "$root_exists")" \
    "$(IFS=,; printf '%s' "${child_json_items[*]}")"
}

navigation_required_children_json() {
  local candidate_path=$1
  local child_json_items=()
  local child child_path child_exists

  for child in "${REQUIRED_CHILDREN[@]}"; do
    child_path="$candidate_path/$child"
    child_exists=false
    [[ -d "$child_path" ]] && child_exists=true
    child_json_items+=("{\"Name\":$(json_string "$child"),\"Path\":$(json_string "$child_path"),\"TestPath\":$(json_bool "$child_exists")}")
  done

  printf '[%s]' "$(IFS=,; printf '%s' "${child_json_items[*]}")"
}

resolve_qgis_compilation_navigation() {
  local explicit_path=$1
  local default_search_mode=$2
  shift
  shift
  local search_roots=("$@")
  local candidates=()
  local messages=()
  local candidate
  local valid_count=0
  local selected_path=""
  local selected_source=""
  local selected_children="[]"

  if [[ -n "$explicit_path" ]]; then
    candidate=$(validate_navigation_root "$explicit_path" "Parameter")
    candidates+=("$candidate")
    if printf '%s' "$candidate" | grep -q '"TestPath":true'; then
      selected_path=$explicit_path
      selected_source="Parameter"
      selected_children=$(navigation_required_children_json "$explicit_path")
      NAVIGATION_PATH="$selected_path"
      NAVIGATION_SOURCE="$selected_source"
      NAVIGATION_TEST_PATH=true
      NAVIGATION_STATUS="Ok"
      NAVIGATION_JSON="{\"Path\":$(json_string "$selected_path"),\"Source\":$(json_string "$selected_source"),\"TestPath\":true,\"RequiredChildren\":$selected_children}"
    else
      messages+=("{\"Level\":\"Error\",\"Code\":\"ExplicitNavigationInvalid\",\"Text\":$(json_string "显式 QGISCompilationNavigation 路径不存在或缺少必需子目录：$explicit_path")}")
      NAVIGATION_STATUS="NotFound"
      NAVIGATION_JSON="{\"Path\":\"\",\"Source\":\"\",\"TestPath\":false,\"RequiredChildren\":[]}"
    fi
    CANDIDATES_JSON="[$(IFS=,; printf '%s' "${candidates[*]}")]"
    NAVIGATION_MESSAGES_JSON="[$(IFS=,; printf '%s' "${messages[*]}")]"
    return
  fi
  if [[ -n "${QGIS_COMPILATION_NAVIGATION:-}" ]]; then
    candidate=$(validate_navigation_root "$QGIS_COMPILATION_NAVIGATION" "Environment:QGIS_COMPILATION_NAVIGATION")
    candidates+=("$candidate")
    if printf '%s' "$candidate" | grep -q '"TestPath":true'; then
      selected_path=$QGIS_COMPILATION_NAVIGATION
      selected_source="Environment:QGIS_COMPILATION_NAVIGATION"
      selected_children=$(navigation_required_children_json "$QGIS_COMPILATION_NAVIGATION")
      NAVIGATION_PATH="$selected_path"
      NAVIGATION_SOURCE="$selected_source"
      NAVIGATION_TEST_PATH=true
      NAVIGATION_STATUS="Ok"
      NAVIGATION_JSON="{\"Path\":$(json_string "$selected_path"),\"Source\":$(json_string "$selected_source"),\"TestPath\":true,\"RequiredChildren\":$selected_children}"
      CANDIDATES_JSON="[$(IFS=,; printf '%s' "${candidates[*]}")]"
      NAVIGATION_MESSAGES_JSON="[$(IFS=,; printf '%s' "${messages[*]}")]"
      return
    fi
    messages+=("{\"Level\":\"Warning\",\"Code\":\"EnvironmentNavigationInvalid\",\"Text\":$(json_string "QGIS_COMPILATION_NAVIGATION 不存在或缺少必需子目录：$QGIS_COMPILATION_NAVIGATION")}")
  fi
  if [[ ${#search_roots[@]} -eq 0 ]]; then
    case "$default_search_mode" in
      None|Disabled) search_roots=() ;;
      AllFileSystems) search_roots=("/") ;;
      *)
        messages+=("{\"Level\":\"Warning\",\"Code\":\"UnknownDefaultSearchMode\",\"Text\":$(json_string "未知 DefaultSearchMode '$default_search_mode'，回退到 AllFileSystems。")}")
        search_roots=("/")
        ;;
    esac
  fi

  local find_prune=( \( )
  local prune_name
  for prune_name in "${PRUNE_DIRECTORY_NAMES[@]}"; do
    find_prune+=( -name "$prune_name" -o )
  done
  find_prune+=( -name "__qgis_noop_prune_sentinel__" \) -prune -o -type d -name QGISCompilationNavigation -print )

  local root
  for root in "${search_roots[@]}"; do
    if [[ ! -d "$root" ]]; then
      messages+=("{\"Level\":\"Warning\",\"Code\":\"SearchRootMissing\",\"Text\":$(json_string "搜索根目录不存在：$root")}")
      continue
    fi
    while IFS= read -r candidate_path; do
      candidates+=("$(validate_navigation_root "$candidate_path" "FilesystemScan")")
      [[ ${#candidates[@]} -ge $MAX_CANDIDATES ]] && break
    done < <(find "$root" "${find_prune[@]}" 2>/dev/null)
    [[ ${#candidates[@]} -ge $MAX_CANDIDATES ]] && break
  done

  if [[ ${#candidates[@]} -ge $MAX_CANDIDATES ]]; then
    messages+=("{\"Level\":\"Warning\",\"Code\":\"CandidateLimitReached\",\"Text\":$(json_string "候选数量达到上限 $MAX_CANDIDATES，后续目录未继续扫描。")}")
  fi

  for candidate in "${candidates[@]}"; do
    if printf '%s' "$candidate" | grep -q '"TestPath":true'; then
      valid_count=$((valid_count + 1))
      selected_path=$(printf '%s' "$candidate" | sed -n 's/^{"Path":"\([^"]*\)".*/\1/p')
      selected_source=$(printf '%s' "$candidate" | sed -n 's/^{"Path":"[^"]*","Source":"\([^"]*\)".*/\1/p')
      selected_children=$(printf '%s' "$candidate" | sed -n 's/.*"RequiredChildren":\(\[.*\]\).*/\1/p')
    fi
  done

  if [[ $valid_count -eq 1 ]]; then
    NAVIGATION_PATH="$selected_path"
    NAVIGATION_SOURCE="$selected_source"
    NAVIGATION_TEST_PATH=true
    NAVIGATION_STATUS="Ok"
    NAVIGATION_JSON="{\"Path\":$(json_string "$selected_path"),\"Source\":$(json_string "$selected_source"),\"TestPath\":true,\"RequiredChildren\":$selected_children}"
  elif [[ $valid_count -gt 1 ]]; then
    NAVIGATION_PATH=""
    NAVIGATION_SOURCE=""
    NAVIGATION_TEST_PATH=false
    NAVIGATION_STATUS="Ambiguous"
    NAVIGATION_JSON="{\"Path\":\"\",\"Source\":\"\",\"TestPath\":false,\"RequiredChildren\":[]}"
  else
    NAVIGATION_PATH=""
    NAVIGATION_SOURCE=""
    NAVIGATION_TEST_PATH=false
    NAVIGATION_STATUS="NotFound"
    NAVIGATION_JSON="{\"Path\":\"\",\"Source\":\"\",\"TestPath\":false,\"RequiredChildren\":[]}"
  fi

  CANDIDATES_JSON="[$(IFS=,; printf '%s' "${candidates[*]}")]"
  NAVIGATION_MESSAGES_JSON="[$(IFS=,; printf '%s' "${messages[*]}")]"
}
