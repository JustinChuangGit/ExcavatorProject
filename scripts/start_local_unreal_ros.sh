#!/usr/bin/env bash
set -Eeuo pipefail

readonly SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly ROS_WORKSPACE="$(cd -- "${SCRIPT_DIRECTORY}/.." && pwd)"
readonly ROS_START_SCRIPT="${ROS_WORKSPACE}/scripts/start_full_sim.sh"
readonly ROS_SERVICE="excavator-ros-stack.service"
readonly LOCAL_UNREAL_SERVICE="excavator-unreal-local.service"
readonly DEFAULT_UNREAL_PROJECT="${ROS_WORKSPACE}/unreal/UnrealTest/UnrealTest.uproject"
readonly UNREAL_MAP="/Game/ExcavatorSim/Maps/Mars_ExcavationSite"
readonly -a PUBLIC_SERVICES=(
  "excavator-watchdog.timer"
  "excavator-watchdog.service"
  "cloudflared-excavator.service"
  "excavator-mission-control.service"
  "excavator-pixel-streaming.service"
  "excavator-unreal-game.service"
)

log() {
  printf '\n[%s] %s\n' "$(date '+%H:%M:%S')" "$*"
}

fail() {
  printf '\nERROR: %s\n' "$*" >&2
  exit 1
}

find_unreal_editor() {
  local user_home=""
  local candidate=""

  if [[ -n "${UNREAL_EDITOR_PATH:-}" ]]; then
    printf '%s\n' "$UNREAL_EDITOR_PATH"
    return 0
  fi

  if command -v UnrealEditor >/dev/null 2>&1; then
    command -v UnrealEditor
    return 0
  fi

  user_home="$(getent passwd "$(id -u)" | cut -d: -f6)"
  for candidate in \
    "${user_home}/Applications/UnrealEngine/Engine/Binaries/Linux/UnrealEditor" \
    "${user_home}/UnrealEngine/Engine/Binaries/Linux/UnrealEditor"; do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

wait_for_service() {
  local service="$1"
  local timeout_seconds="$2"
  local deadline=$((SECONDS + timeout_seconds))

  while (( SECONDS < deadline )); do
    if systemctl --user is-active --quiet "$service"; then
      return 0
    fi
    sleep 1
  done

  systemctl --user status "$service" --no-pager --lines=25 || true
  return 1
}

wait_for_port() {
  local port="$1"
  local timeout_seconds="$2"
  local deadline=$((SECONDS + timeout_seconds))

  while (( SECONDS < deadline )); do
    if ss -Hltn "sport = :${port}" | grep --extended-regexp --quiet ":${port}([[:space:]]|$)"; then
      return 0
    fi
    sleep 1
  done

  return 1
}

wait_for_map() {
  local started_at="$1"
  local timeout_seconds="$2"
  local deadline=$((SECONDS + timeout_seconds))

  while (( SECONDS < deadline )); do
    if journalctl --user -u "$LOCAL_UNREAL_SERVICE" \
      --since "$started_at" --no-pager 2>/dev/null |
      grep --fixed-strings --quiet 'Load map complete /Game/ExcavatorSim/Maps/Mars_ExcavationSite'; then
      return 0
    fi
    sleep 2
  done

  return 1
}

[[ -x "$ROS_START_SCRIPT" ]] || fail "Missing ROS launcher: ${ROS_START_SCRIPT}"
unreal_editor="$(find_unreal_editor || true)"
unreal_project="${UNREAL_PROJECT_PATH:-$DEFAULT_UNREAL_PROJECT}"
[[ -x "$unreal_editor" ]] || fail "UnrealEditor was not found. Set UNREAL_EDITOR_PATH to its full path."
[[ -f "$unreal_project" ]] || fail "Missing Unreal project: ${unreal_project}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export DISPLAY="${DISPLAY:-:1}"
export XAUTHORITY="${XAUTHORITY:-${XDG_RUNTIME_DIR}/gdm/Xauthority}"

log "Disabling the public/web runtime for local mode"
systemctl --user stop "${PUBLIC_SERVICES[@]}" 2>/dev/null || true
systemctl --user reset-failed excavator-unreal-game.service >/dev/null 2>&1 || true

log "Starting the ROS 2 excavator stack"
if systemctl --user cat "$ROS_SERVICE" >/dev/null 2>&1; then
  systemctl --user start "$ROS_SERVICE"
else
  systemd-run --user \
    --unit="$ROS_SERVICE" \
    --collect \
    --service-type=exec \
    --working-directory="$ROS_WORKSPACE" \
    --property=Restart=on-failure \
    --property=RestartSec=3 \
    "$ROS_START_SCRIPT"
fi

wait_for_service "$ROS_SERVICE" 45 || fail "ROS service did not become active."
wait_for_port 9090 90 || fail "ROS started, but rosbridge did not open port 9090."
printf '  ready  ROS and rosbridge\n'

if systemctl --user is-active --quiet "$LOCAL_UNREAL_SERVICE"; then
  printf '  ready  existing local Unreal game\n'
else
  systemctl --user stop "$LOCAL_UNREAL_SERVICE" >/dev/null 2>&1 || true
  systemctl --user reset-failed "$LOCAL_UNREAL_SERVICE" >/dev/null 2>&1 || true
  systemctl --user import-environment DISPLAY XAUTHORITY XDG_RUNTIME_DIR >/dev/null

  unreal_started_at="$(date '+%Y-%m-%d %H:%M:%S')"
  log "Starting Unreal locally without the website or Pixel Streaming"
  systemd-run --user \
    --unit="$LOCAL_UNREAL_SERVICE" \
    --collect \
    --service-type=exec \
    --working-directory="$ROS_WORKSPACE" \
    --setenv="DISPLAY=${DISPLAY}" \
    --setenv="XAUTHORITY=${XAUTHORITY}" \
    --setenv="XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR}" \
    --property=Restart=on-failure \
    --property=RestartSec=5 \
    --property=KillMode=control-group \
    "$unreal_editor" \
    "$unreal_project" \
    "$UNREAL_MAP" \
    -game \
    -windowed \
    -ResX=1600 \
    -ResY=900 \
    -nosound \
    -ExecCmds=DisableAllScreenMessages \
    -log

  wait_for_service "$LOCAL_UNREAL_SERVICE" 60 || fail "Local Unreal service did not start."
  wait_for_map "$unreal_started_at" 180 || fail "Unreal started, but the Mars map did not finish loading."
  printf '  ready  Unreal Mars map\n'
fi

log "Local simulator is ready"
printf '  ROS:             active\n'
printf '  Unreal:          active\n'
printf '  Website:         stopped\n'
printf '  Pixel Streaming: stopped\n'
printf '  Public tunnel:   stopped\n'
printf '  Watchdog:        stopped for local mode\n'
