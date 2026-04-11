#!/bin/bash
# flash_all.sh - InksPet firmware flash & device monitoring tool
# Ported from EleksCava flash tool, adapted for InksPet hardware.
# Features: flash firmware, log capture, firmware backup, long-term monitoring.

SCRIPT_VERSION="v2.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
GRAY='\033[0;37m'
NC='\033[0m'

# Defaults
SKIP_FILESYSTEM=false
BUILD_ENV="esp32dev"
ERASE_FLASH=false
LOG_ONLY=false
MONITOR_MODE=false
BACKUP_ONLY=false
USE_CONFIG=false
CONFIG_FILE=".flash_config"
MAX_RETRIES=3
RETRY_DELAY=5

# Baud rate candidates (high → low)
FLASH_BAUD_RATES=(460800 230400 115200 57600 9600)
OPTIMAL_BAUD_RATE=""

# ============================================
# Config management
# ============================================

load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
        echo -e "${GREEN}✅ Loaded config: $CONFIG_FILE${NC}"
        echo "   Build env: $BUILD_ENV"
        echo "   Default port: ${DEFAULT_PORT:-not set}"
        return 0
    fi
    return 1
}

save_config() {
    cat > "$CONFIG_FILE" <<EOF
# InksPet Flash Tool Configuration
BUILD_ENV=$BUILD_ENV
SKIP_FILESYSTEM=$SKIP_FILESYSTEM
ERASE_FLASH=$ERASE_FLASH
DEFAULT_PORT=$PORT
LAST_UPDATED=$(date)
EOF
    echo -e "${GREEN}💾 Config saved to $CONFIG_FILE${NC}"
}

# ============================================
# Dependency & port checks
# ============================================

check_dependencies() {
    echo -e "${BLUE}🔍 Checking required tools...${NC}"
    local missing_tools=()

    for tool in pio esptool.py; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        echo -e "${RED}❌ Missing tools: ${missing_tools[*]}${NC}"
        echo -e "${YELLOW}Install the following:${NC}"
        for tool in "${missing_tools[@]}"; do
            case $tool in
                "pio")       echo "  - PlatformIO: https://platformio.org/install" ;;
                "esptool.py") echo "  - esptool: pip install esptool" ;;
            esac
        done
        return 1
    fi

    echo -e "${GREEN}✅ All required tools installed${NC}"
    return 0
}

check_port() {
    local port=$1

    if [ ! -e "$port" ]; then
        echo -e "${RED}❌ Port $port not found${NC}"
        echo ""
        echo -e "${YELLOW}💡 List available ports:${NC}"
        echo "   ls /dev/cu.usb*"
        echo ""
        echo -e "${YELLOW}💡 Currently available serial devices:${NC}"
        local available_ports
        available_ports=$(ls /dev/cu.usb* 2>/dev/null)
        if [ -n "$available_ports" ]; then
            echo "$available_ports" | while read -r p; do echo "   $p"; done
        else
            echo "   (no USB serial devices found)"
        fi
        return 1
    fi

    if [ ! -r "$port" ] || [ ! -w "$port" ]; then
        echo -e "${RED}❌ Port $port not accessible, check permissions${NC}"
        echo -e "${YELLOW}💡 You may need: sudo chmod 666 $port${NC}"
        return 1
    fi

    echo -e "${GREEN}✅ Port $port OK${NC}"
    return 0
}

# ============================================
# Cross-platform timeout helper
# ============================================

run_with_timeout() {
    local timeout_seconds=$1
    local command="$2"

    if command -v timeout >/dev/null 2>&1; then
        timeout "$timeout_seconds" $command
        return $?
    fi
    if command -v gtimeout >/dev/null 2>&1; then
        gtimeout "$timeout_seconds" $command
        return $?
    fi

    $command &
    local cmd_pid=$!
    (sleep "$timeout_seconds" && kill -TERM "$cmd_pid" 2>/dev/null) &
    local sleep_pid=$!
    wait "$cmd_pid" 2>/dev/null
    local exit_code=$?
    kill "$sleep_pid" 2>/dev/null
    wait "$sleep_pid" 2>/dev/null
    return $exit_code
}

# ============================================
# Connection diagnostics
# ============================================

find_optimal_baud_rate() {
    local test_port=$1
    echo -e "${BLUE}🔧 Auto-detecting best baud rate...${NC}"

    for baud in "${FLASH_BAUD_RATES[@]}"; do
        echo -e "${YELLOW}   Testing baud: $baud...${NC}"
        if run_with_timeout 8 "esptool.py --port $test_port --baud $baud chip_id" > /dev/null 2>&1; then
            echo -e "${GREEN}✅ Stable baud rate found: $baud${NC}"
            OPTIMAL_BAUD_RATE=$baud
            return 0
        fi
    done

    echo -e "${RED}❌ All baud rates failed${NC}"
    return 1
}

fix_flash_communication() {
    echo -e "${BLUE}🔧 Diagnosing flash communication...${NC}"

    if find_optimal_baud_rate "$PORT"; then
        echo -e "${GREEN}✅ Stable connection detected${NC}"
        return 0
    fi

    echo -e "${YELLOW}   Trying alternative reset methods...${NC}"
    local reset_commands=(
        "esptool.py --port $PORT --before default_reset --after hard_reset chip_id"
        "esptool.py --port $PORT --before no_reset --after soft_reset chip_id"
        "esptool.py --port $PORT --before no_reset --after no_reset chip_id"
    )

    for cmd in "${reset_commands[@]}"; do
        if run_with_timeout 8 "$cmd" > /dev/null 2>&1; then
            echo -e "${GREEN}✅ Connected after reset${NC}"
            return 0
        fi
    done

    echo -e "${CYAN}💡 Manually enter download mode:${NC}"
    echo "   1. Hold the BOOT button"
    echo "   2. Press the RST button"
    echo "   3. Release RST, keep holding BOOT for 3 seconds"
    echo "   4. Release BOOT"
    echo ""
    echo -e "${YELLOW}Press Enter when ready, Ctrl+C to cancel...${NC}"
    read -r

    if find_optimal_baud_rate "$PORT"; then
        echo -e "${GREEN}✅ Connected after manual reset${NC}"
        return 0
    fi

    echo -e "${RED}❌ Still cannot establish stable connection${NC}"
    return 1
}

detect_device_info() {
    echo -e "${BLUE}🔍 Detecting device...${NC}"
    local chip_info
    local temp_file="/tmp/esptool_output_$$"

    # Method 1: flash_id
    if run_with_timeout 10 "esptool.py --port $PORT flash_id" > "$temp_file" 2>&1; then
        chip_info=$(cat "$temp_file")
        if echo "$chip_info" | grep -q "ESP32"; then
            local chip_type mac_addr
            chip_type=$(echo "$chip_info" | grep "Chip is" | sed 's/Chip is //' | awk '{print $1}' || echo "ESP32")
            mac_addr=$(echo "$chip_info" | grep "MAC:" | awk '{print $2}' || echo "")
            echo -e "${GREEN}✅ Device detected: $chip_type${NC}"
            [ -n "$mac_addr" ] && echo -e "${CYAN}   MAC: $mac_addr${NC}"
            rm -f "$temp_file"
            return 0
        fi
    fi

    # Method 2: read_mac
    echo -e "${YELLOW}   Trying read_mac...${NC}"
    if run_with_timeout 10 "esptool.py --port $PORT --baud 115200 read_mac" > "$temp_file" 2>&1; then
        chip_info=$(cat "$temp_file")
        if echo "$chip_info" | grep -q -E "(ESP32|MAC:)"; then
            local chip_type mac_addr
            chip_type=$(echo "$chip_info" | grep "Chip is" | sed 's/Chip is //' | awk '{print $1}' || echo "ESP32")
            mac_addr=$(echo "$chip_info" | grep "MAC:" | head -1 | awk '{print $2}' || echo "")
            echo -e "${GREEN}✅ Device detected: $chip_type${NC}"
            [ -n "$mac_addr" ] && echo -e "${CYAN}   MAC: $mac_addr${NC}"
            rm -f "$temp_file"
            return 0
        fi
    fi

    # Method 3: chip_id
    echo -e "${YELLOW}   Trying chip_id...${NC}"
    if run_with_timeout 10 "esptool.py --port $PORT chip_id" > "$temp_file" 2>&1; then
        chip_info=$(cat "$temp_file")
        if echo "$chip_info" | grep -q "ESP32"; then
            local chip_type
            chip_type=$(echo "$chip_info" | grep "Chip is" | sed 's/Chip is //' | awk '{print $1}' || echo "ESP32")
            echo -e "${GREEN}✅ Device detected: $chip_type${NC}"
            rm -f "$temp_file"
            return 0
        fi
    fi

    echo -e "${RED}❌ Cannot connect to device. Check:${NC}"
    echo "   1. Device is connected"
    echo "   2. Port path is correct"
    echo "   3. USB driver is installed"
    echo "   4. Port not in use by another process"
    echo "   5. Device is in download mode"

    if [ -f "$temp_file" ]; then
        local error_info
        error_info=$(head -3 "$temp_file" | tr '\n' ' ' | sed 's/  */ /g')
        [ -n "$error_info" ] && echo -e "${YELLOW}   Last error: $error_info${NC}"
        rm -f "$temp_file"
    fi

    echo -e "${CYAN}💡 Tip: try holding BOOT while pressing RST${NC}"
    return 1
}

# ============================================
# Retry & progress helpers
# ============================================

execute_with_retry() {
    local command="$1"
    local description="$2"
    local retry_count=0

    while [ $retry_count -lt $MAX_RETRIES ]; do
        echo -e "${BLUE}🔄 $description (attempt $((retry_count + 1))/$MAX_RETRIES)${NC}"

        if eval "$command"; then
            echo -e "${GREEN}✅ $description succeeded${NC}"
            return 0
        fi

        retry_count=$((retry_count + 1))
        if [ $retry_count -lt $MAX_RETRIES ]; then
            echo -e "${YELLOW}⏳ Retrying in ${RETRY_DELAY}s...${NC}"
            sleep $RETRY_DELAY
        fi
    done

    echo -e "${RED}❌ $description failed after $MAX_RETRIES attempts${NC}"
    return 1
}

show_progress() {
    local current=$1
    local total=$2
    local description="$3"

    local progress=$((current * 100 / total))
    local filled=$((progress / 5))
    local empty=$((20 - filled))

    printf "\r${CYAN}🔄 [%s%s] %d%% %s${NC}" \
        "$(printf '%*s' $filled | tr ' ' '█')" \
        "$(printf '%*s' $empty | tr ' ' '░')" \
        "$progress" \
        "$description"

    [ $current -eq $total ] && echo
}

# ============================================
# Log management
# ============================================

setup_log_directory() {
    if [ ! -d "logs" ]; then
        mkdir -p logs
        echo -e "${GREEN}📁 Created log directory: logs/${NC}"
    fi
}

manage_logs() {
    local log_dir="logs"
    local max_logs=10

    [ ! -d "$log_dir" ] && return

    cd "$log_dir" 2>/dev/null || return

    local log_count
    log_count=$(ls -1 serial_*.log 2>/dev/null | wc -l)
    if [ "$log_count" -gt "$max_logs" ]; then
        ls -t serial_*.log 2>/dev/null | tail -n +$((max_logs + 1)) | xargs rm -f
        echo -e "${YELLOW}🗑️  Cleaned up $((log_count - max_logs + 1)) old log files${NC}"
    fi

    # Compress logs older than 7 days
    find . -name "serial_*.log" -mtime +7 -exec gzip {} \; 2>/dev/null

    cd - >/dev/null
}

get_log_filename() {
    local timestamp
    timestamp=$(date +"%Y%m%d_%H%M%S")
    echo "logs/serial_${timestamp}.log"
}

# ============================================
# Monitor mode — event detection & dashboard
# ============================================

init_monitor_stats() {
    MONITOR_START_TIME=$(date +%s)
    MONITOR_START_DATE=$(date +"%Y-%m-%d %H:%M:%S")
    MONITOR_STATS_FILE="logs/monitor_stats_$(date +%Y%m%d).json"
    MONITOR_EVENTS_FILE="logs/monitor_events_$(date +%Y%m%d).log"

    COUNT_REBOOTS=0
    COUNT_CRASHES=0
    COUNT_MEMORY_ERRORS=0
    COUNT_WATCHDOG=0
    COUNT_WIFI_DISCONNECT=0
    COUNT_OTHER_ERRORS=0
    COUNT_SSL_FAILURES=0
    COUNT_AGENT_EVENTS=0
    COUNT_PERMISSION_REQUESTS=0
    COUNT_DISPLAY_REFRESH=0
    COUNT_API_SUCCESS=0
    COUNT_API_FAILURES=0

    REBOOT_TIMESTAMPS=()
    CRASH_TIMESTAMPS=()
    MEMORY_TIMESTAMPS=()
    WATCHDOG_TIMESTAMPS=()
    WIFI_TIMESTAMPS=()
    ERROR_TIMESTAMPS=()
    SSL_TIMESTAMPS=()
    AGENT_TIMESTAMPS=()
    API_FAIL_TIMESTAMPS=()

    REBOOT_DETAILS=()
    CRASH_DETAILS=()
    MEMORY_DETAILS=()
    WATCHDOG_DETAILS=()
    WIFI_DETAILS=()
    ERROR_DETAILS=()
    SSL_DETAILS=()
    AGENT_DETAILS=()
    API_FAIL_DETAILS=()

    {
        echo ""
        echo "========================================"
        echo "Monitor session started: $MONITOR_START_DATE"
        echo "Port: $PORT"
        echo "========================================"
    } >> "$MONITOR_EVENTS_FILE"
}

detect_event_type() {
    local line="$1"
    local timestamp
    local full_timestamp
    timestamp=$(date +"%H:%M:%S")
    full_timestamp=$(date +"%Y-%m-%d %H:%M:%S")

    # Device reboot
    if echo "$line" | grep -qE "(Boot|Restarting|ESP-ROM|rst:0x|CPU0 reset reason|boot:0x)"; then
        ((COUNT_REBOOTS++))
        REBOOT_TIMESTAMPS+=("$timestamp")
        local reset_reason
        reset_reason=$(echo "$line" | grep -oE "rst:0x[0-9a-f]+" || echo "unknown")
        REBOOT_DETAILS+=("$reset_reason")
        echo "[$full_timestamp] [REBOOT] $line" >> "$MONITOR_EVENTS_FILE"
        return 1
    fi

    # System crash
    if echo "$line" | grep -qE "(Panic|abort|Exception|Stack canary|LoadProhibited|StoreProhibited|IllegalInstruction)"; then
        ((COUNT_CRASHES++))
        CRASH_TIMESTAMPS+=("$timestamp")
        local crash_type
        crash_type=$(echo "$line" | grep -oE "(Panic|abort|Exception)" | head -1 || echo "unknown")
        CRASH_DETAILS+=("$crash_type")
        echo "[$full_timestamp] [CRASH] $line" >> "$MONITOR_EVENTS_FILE"
        return 2
    fi

    # Memory error
    if echo "$line" | grep -qE "(alloc failed|Out of memory|heap_caps|CORRUPT HEAP|malloc failed|free heap)"; then
        ((COUNT_MEMORY_ERRORS++))
        MEMORY_TIMESTAMPS+=("$timestamp")
        MEMORY_DETAILS+=("$(echo "$line" | cut -c1-80)")
        echo "[$full_timestamp] [MEMORY] $line" >> "$MONITOR_EVENTS_FILE"
        return 3
    fi

    # Watchdog timeout
    if echo "$line" | grep -qE "(Task watchdog|WDT|watchdog timeout|esp_task_wdt)"; then
        ((COUNT_WATCHDOG++))
        WATCHDOG_TIMESTAMPS+=("$timestamp")
        WATCHDOG_DETAILS+=("$(echo "$line" | cut -c1-80)")
        echo "[$full_timestamp] [WATCHDOG] $line" >> "$MONITOR_EVENTS_FILE"
        return 4
    fi

    # WiFi disconnect
    if echo "$line" | grep -qE "(WiFi disconnected|Connection lost|AP disconnect|wifi:disconnect|Disconnected from AP)"; then
        ((COUNT_WIFI_DISCONNECT++))
        WIFI_TIMESTAMPS+=("$timestamp")
        local disconnect_reason
        disconnect_reason=$(echo "$line" | grep -oE "reason:[0-9]+" || echo "unknown")
        WIFI_DETAILS+=("$disconnect_reason")
        echo "[$full_timestamp] [WIFI_DISCONNECT] $line" >> "$MONITOR_EVENTS_FILE"
        return 5
    fi

    # SSL failures
    if echo "$line" | grep -qE "(ssl_client.*_handle_error|BIGNUM.*Memory|start_ssl_client.*-[0-9])"; then
        ((COUNT_SSL_FAILURES++))
        SSL_TIMESTAMPS+=("$timestamp")
        SSL_DETAILS+=("$(echo "$line" | grep -oE "\(-?[0-9]+\).*" | head -1 | cut -c1-60)")
        echo "[$full_timestamp] [SSL_FAIL] $line" >> "$MONITOR_EVENTS_FILE"
        return 7
    fi

    # InksPet-specific: agent state events
    if echo "$line" | grep -qE "AgentStateManager|/api/agent/state|Agent state:"; then
        ((COUNT_AGENT_EVENTS++))
        AGENT_TIMESTAMPS+=("$timestamp")
        AGENT_DETAILS+=("$(echo "$line" | cut -c1-80)")
        echo "[$full_timestamp] [AGENT] $line" >> "$MONITOR_EVENTS_FILE"
        return 0
    fi

    # InksPet-specific: permission requests
    if echo "$line" | grep -qE "PermissionManager|permission request|approval"; then
        ((COUNT_PERMISSION_REQUESTS++))
        echo "[$full_timestamp] [PERMISSION] $line" >> "$MONITOR_EVENTS_FILE"
        return 0
    fi

    # InksPet-specific: display refresh
    if echo "$line" | grep -qE "DisplayManager|refresh|e-paper"; then
        ((COUNT_DISPLAY_REFRESH++))
        return 0
    fi

    # API success/failure
    if echo "$line" | grep -qE "HTTP Code: 200|Data update successful|200 OK"; then
        ((COUNT_API_SUCCESS++))
        return 0
    fi
    if echo "$line" | grep -qE "HTTP Code: -1|HTTP Code: 4[0-9][0-9]|HTTP Code: 5[0-9][0-9]|HTTP -1"; then
        ((COUNT_API_FAILURES++))
        API_FAIL_TIMESTAMPS+=("$timestamp")
        API_FAIL_DETAILS+=("$(echo "$line" | cut -c1-80)")
        echo "[$full_timestamp] [API_FAIL] $line" >> "$MONITOR_EVENTS_FILE"
        return 8
    fi

    # Other errors
    if echo "$line" | grep -qE "(ERROR|FAIL|Fatal|ASSERT|Critical)"; then
        ((COUNT_OTHER_ERRORS++))
        ERROR_TIMESTAMPS+=("$timestamp")
        ERROR_DETAILS+=("$(echo "$line" | cut -c1-80)")
        echo "[$full_timestamp] [ERROR] $line" >> "$MONITOR_EVENTS_FILE"
        return 6
    fi

    return 0
}

calculate_stability_metrics() {
    local current_time runtime downtime uptime_percentage total_failures mtbf
    current_time=$(date +%s)
    runtime=$((current_time - MONITOR_START_TIME))

    downtime=$((COUNT_REBOOTS * 30 + COUNT_CRASHES * 45))
    uptime_percentage=100
    if [ $runtime -gt 0 ]; then
        uptime_percentage=$(( (runtime - downtime) * 100 / runtime ))
        [ $uptime_percentage -lt 0 ] && uptime_percentage=0
    fi

    total_failures=$((COUNT_REBOOTS + COUNT_CRASHES))
    if [ $total_failures -gt 0 ]; then
        mtbf=$((runtime / total_failures))
    else
        mtbf=$runtime
    fi

    echo "$uptime_percentage:$mtbf"
}

show_monitor_dashboard() {
    local current_time runtime hours minutes seconds
    current_time=$(date +%s)
    runtime=$((current_time - MONITOR_START_TIME))
    hours=$((runtime / 3600))
    minutes=$(((runtime % 3600) / 60))
    seconds=$((runtime % 60))

    local last_reboot="none" last_crash="none" last_memory="none"
    local last_watchdog="none" last_wifi="none" last_ssl="none" last_agent="none"
    [ ${#REBOOT_TIMESTAMPS[@]} -gt 0 ]   && last_reboot="${REBOOT_TIMESTAMPS[${#REBOOT_TIMESTAMPS[@]}-1]}"
    [ ${#CRASH_TIMESTAMPS[@]} -gt 0 ]    && last_crash="${CRASH_TIMESTAMPS[${#CRASH_TIMESTAMPS[@]}-1]}"
    [ ${#MEMORY_TIMESTAMPS[@]} -gt 0 ]   && last_memory="${MEMORY_TIMESTAMPS[${#MEMORY_TIMESTAMPS[@]}-1]}"
    [ ${#WATCHDOG_TIMESTAMPS[@]} -gt 0 ] && last_watchdog="${WATCHDOG_TIMESTAMPS[${#WATCHDOG_TIMESTAMPS[@]}-1]}"
    [ ${#WIFI_TIMESTAMPS[@]} -gt 0 ]     && last_wifi="${WIFI_TIMESTAMPS[${#WIFI_TIMESTAMPS[@]}-1]}"
    [ ${#SSL_TIMESTAMPS[@]} -gt 0 ]      && last_ssl="${SSL_TIMESTAMPS[${#SSL_TIMESTAMPS[@]}-1]}"
    [ ${#AGENT_TIMESTAMPS[@]} -gt 0 ]    && last_agent="${AGENT_TIMESTAMPS[${#AGENT_TIMESTAMPS[@]}-1]}"

    local metrics uptime_percentage mtbf mtbf_min
    metrics=$(calculate_stability_metrics)
    uptime_percentage=$(echo "$metrics" | cut -d: -f1)
    mtbf=$(echo "$metrics" | cut -d: -f2)
    mtbf_min=$((mtbf / 60))

    clear
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} ${GREEN}InksPet Device Monitor${NC} — Uptime: ${hours}h${minutes}m${seconds}s            ${CYAN}║${NC}"
    echo -e "${CYAN}╠══════════════════════════════════════════════════════════╣${NC}"
    echo -e "${CYAN}║${NC} 📊 ${YELLOW}Error counters:${NC}                                      ${CYAN}║${NC}"
    printf "${CYAN}║${NC}   • Reboots:       ${RED}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_REBOOTS" "$last_reboot"
    printf "${CYAN}║${NC}   • Crashes:       ${RED}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_CRASHES" "$last_crash"
    printf "${CYAN}║${NC}   • Memory errors: ${YELLOW}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_MEMORY_ERRORS" "$last_memory"
    printf "${CYAN}║${NC}   • Watchdog:      ${YELLOW}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_WATCHDOG" "$last_watchdog"
    printf "${CYAN}║${NC}   • WiFi drops:    ${BLUE}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_WIFI_DISCONNECT" "$last_wifi"
    printf "${CYAN}║${NC}   • SSL failures:  ${YELLOW}%-3d${NC} (last: %-10s)              ${CYAN}║${NC}\n" "$COUNT_SSL_FAILURES" "$last_ssl"
    printf "${CYAN}║${NC}   • Other errors:  ${PURPLE}%-3d${NC}                                  ${CYAN}║${NC}\n" "$COUNT_OTHER_ERRORS"
    echo -e "${CYAN}╠══════════════════════════════════════════════════════════╣${NC}"
    echo -e "${CYAN}║${NC} 🤖 ${YELLOW}InksPet activity:${NC}                                    ${CYAN}║${NC}"
    printf "${CYAN}║${NC}   • Agent events:     ${GREEN}%-4d${NC} (last: %-10s)           ${CYAN}║${NC}\n" "$COUNT_AGENT_EVENTS" "$last_agent"
    printf "${CYAN}║${NC}   • Permission reqs:  ${GREEN}%-4d${NC}                             ${CYAN}║${NC}\n" "$COUNT_PERMISSION_REQUESTS"
    printf "${CYAN}║${NC}   • Display refresh:  ${GREEN}%-4d${NC}                             ${CYAN}║${NC}\n" "$COUNT_DISPLAY_REFRESH"
    echo -e "${CYAN}╠══════════════════════════════════════════════════════════╣${NC}"
    local api_total=$((COUNT_API_SUCCESS + COUNT_API_FAILURES))
    local api_rate=0
    [ $api_total -gt 0 ] && api_rate=$((COUNT_API_SUCCESS * 100 / api_total))
    printf "${CYAN}║${NC} 🔄 API: ${GREEN}%-4d${NC}OK ${RED}%-3d${NC}fail (success rate: ${GREEN}%3d%%${NC})      ${CYAN}║${NC}\n" "$COUNT_API_SUCCESS" "$COUNT_API_FAILURES" "$api_rate"
    echo -e "${CYAN}╠══════════════════════════════════════════════════════════╣${NC}"
    echo -e "${CYAN}║${NC} 📈 Stability: ${GREEN}${uptime_percentage}%${NC} | MTBF: ${GREEN}${mtbf_min}${NC} min                ${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${YELLOW}⏹️  Press Ctrl+C to stop and generate report${NC}"
}

save_monitor_stats() {
    local current_time runtime metrics uptime_percentage mtbf
    current_time=$(date +%s)
    runtime=$((current_time - MONITOR_START_TIME))
    metrics=$(calculate_stability_metrics)
    uptime_percentage=$(echo "$metrics" | cut -d: -f1)
    mtbf=$(echo "$metrics" | cut -d: -f2)

    cat > "$MONITOR_STATS_FILE" <<EOF
{
  "start_time": "$MONITOR_START_DATE",
  "last_update": "$(date +"%Y-%m-%d %H:%M:%S")",
  "total_runtime_seconds": $runtime,
  "events": {
    "reboots": {
      "count": $COUNT_REBOOTS,
      "timestamps": [$(printf '"%s",' "${REBOOT_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "crashes": {
      "count": $COUNT_CRASHES,
      "timestamps": [$(printf '"%s",' "${CRASH_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "memory_errors": {
      "count": $COUNT_MEMORY_ERRORS,
      "timestamps": [$(printf '"%s",' "${MEMORY_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "watchdog_timeouts": {
      "count": $COUNT_WATCHDOG,
      "timestamps": [$(printf '"%s",' "${WATCHDOG_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "wifi_disconnects": {
      "count": $COUNT_WIFI_DISCONNECT,
      "timestamps": [$(printf '"%s",' "${WIFI_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "other_errors": {
      "count": $COUNT_OTHER_ERRORS,
      "timestamps": [$(printf '"%s",' "${ERROR_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "ssl_failures": {
      "count": $COUNT_SSL_FAILURES,
      "timestamps": [$(printf '"%s",' "${SSL_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "agent_events": {
      "count": $COUNT_AGENT_EVENTS,
      "timestamps": [$(printf '"%s",' "${AGENT_TIMESTAMPS[@]}" | sed 's/,$//')]
    },
    "permission_requests": $COUNT_PERMISSION_REQUESTS,
    "display_refreshes": $COUNT_DISPLAY_REFRESH,
    "api_requests": {
      "success": $COUNT_API_SUCCESS,
      "failures": $COUNT_API_FAILURES,
      "failure_timestamps": [$(printf '"%s",' "${API_FAIL_TIMESTAMPS[@]}" | sed 's/,$//')]
    }
  },
  "stability_metrics": {
    "uptime_percentage": $uptime_percentage,
    "mean_time_between_failures": $mtbf
  }
}
EOF
}

generate_monitor_report() {
    local current_time runtime hours minutes metrics uptime_percentage report_file
    current_time=$(date +%s)
    runtime=$((current_time - MONITOR_START_TIME))
    hours=$((runtime / 3600))
    minutes=$(((runtime % 3600) / 60))
    metrics=$(calculate_stability_metrics)
    uptime_percentage=$(echo "$metrics" | cut -d: -f1)
    report_file="logs/monitor_report_$(date +%Y%m%d_%H%M%S).txt"

    local main_issue="no significant issues"
    local recommendation="device running normally"

    if [ $COUNT_CRASHES -gt 0 ]; then
        main_issue="system crashes ($COUNT_CRASHES)"
        recommendation="check for memory corruption or stack overflow"
    elif [ $COUNT_REBOOTS -gt 5 ]; then
        main_issue="frequent reboots ($COUNT_REBOOTS)"
        recommendation="check power stability and temperature"
    elif [ $COUNT_MEMORY_ERRORS -gt 3 ]; then
        main_issue="memory pressure ($COUNT_MEMORY_ERRORS)"
        recommendation="optimize heap usage, check for leaks"
    elif [ $COUNT_WIFI_DISCONNECT -gt 10 ]; then
        main_issue="unstable WiFi ($COUNT_WIFI_DISCONNECT drops)"
        recommendation="check signal strength or router settings"
    elif [ $COUNT_WATCHDOG -gt 2 ]; then
        main_issue="watchdog timeouts ($COUNT_WATCHDOG)"
        recommendation="look for long-blocking tasks"
    elif [ $COUNT_SSL_FAILURES -gt 5 ]; then
        main_issue="SSL handshake failures ($COUNT_SSL_FAILURES)"
        recommendation="check heap fragmentation, reduce concurrent SSL"
    elif [ $COUNT_API_FAILURES -gt 10 ]; then
        main_issue="API failures ($COUNT_API_FAILURES)"
        recommendation="check network connectivity and API availability"
    fi

    cat > "$report_file" <<EOF
════════════════════════════════════════════════════════════
📊 InksPet Device Monitor Report
════════════════════════════════════════════════════════════
Generated: $(date +"%Y-%m-%d %H:%M:%S")
Port: $PORT
────────────────────────────────────────────────────────────

📈 Session statistics
────────────────────────────────────────────────────────────
Duration:    ${hours}h ${minutes}m
Stability:   ${uptime_percentage}%
Main issue:  $main_issue

📊 Error breakdown
────────────────────────────────────────────────────────────
Reboots:         $COUNT_REBOOTS
Crashes:         $COUNT_CRASHES
Memory errors:   $COUNT_MEMORY_ERRORS
Watchdog TO:     $COUNT_WATCHDOG
WiFi drops:      $COUNT_WIFI_DISCONNECT
SSL failures:    $COUNT_SSL_FAILURES
Other errors:    $COUNT_OTHER_ERRORS

🤖 InksPet activity
────────────────────────────────────────────────────────────
Agent events:        $COUNT_AGENT_EVENTS
Permission requests: $COUNT_PERMISSION_REQUESTS
Display refreshes:   $COUNT_DISPLAY_REFRESH

🔄 API health
────────────────────────────────────────────────────────────
API success:  $COUNT_API_SUCCESS
API failures: $COUNT_API_FAILURES

💡 Recommendation
────────────────────────────────────────────────────────────
$recommendation

📁 Related files
────────────────────────────────────────────────────────────
Stats (JSON):  $MONITOR_STATS_FILE
Event log:     $MONITOR_EVENTS_FILE
════════════════════════════════════════════════════════════
EOF

    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}📊 Monitor session summary${NC}"
    echo -e "${GREEN}────────────────────────────────────────────────────────────${NC}"
    echo -e "Duration:   ${CYAN}${hours}h ${minutes}m${NC}"
    echo -e "Stability:  ${CYAN}${uptime_percentage}%${NC}"
    echo -e "Main issue: ${YELLOW}$main_issue${NC}"
    echo -e "Advice:     ${YELLOW}$recommendation${NC}"
    echo ""
    echo -e "Full report: ${CYAN}$report_file${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
}

run_monitor_mode() {
    echo ""
    echo -e "${BLUE}🔍 Initializing monitor system...${NC}"

    if [ ! -c "$PORT" ]; then
        echo -e "${RED}❌ Port $PORT not accessible${NC}"
        exit 1
    fi

    init_monitor_stats
    save_monitor_stats

    echo -e "${GREEN}✅ Monitor ready${NC}"
    echo -e "${CYAN}📁 Stats file:  $MONITOR_STATS_FILE${NC}"
    echo -e "${CYAN}📁 Events log:  $MONITOR_EVENTS_FILE${NC}"
    echo ""
    echo -e "${YELLOW}Dashboard starts in 5 seconds...${NC}"
    echo -e "${GRAY}Port: $PORT | Baud: 115200${NC}"
    sleep 5

    trap 'echo ""; save_monitor_stats; generate_monitor_report; exit 0' INT TERM

    local temp_output="/tmp/monitor_output_$$"
    local display_counter=0

    (
        if [[ "$OSTYPE" == "darwin"* ]]; then
            stty -f "$PORT" 115200 raw -echo 2>/dev/null || true
        else
            stty -F "$PORT" 115200 raw -echo 2>/dev/null || true
        fi

        cat "$PORT" 2>/dev/null | while IFS= read -r line; do
            detect_event_type "$line"
            ((display_counter++))
            if [ $((display_counter % 50)) -eq 0 ]; then
                show_monitor_dashboard
                save_monitor_stats
                display_counter=0
            fi
            echo "$line" >> "$temp_output"
        done
    ) &

    local monitor_pid=$!
    sleep 2
    show_monitor_dashboard

    while kill -0 $monitor_pid 2>/dev/null; do
        sleep 5
        show_monitor_dashboard
        save_monitor_stats
    done

    rm -f "$temp_output"
    wait $monitor_pid
}

# ============================================
# Help & argument parsing
# ============================================

show_help() {
    echo -e "${CYAN}InksPet Firmware Flash Tool $SCRIPT_VERSION${NC}"
    echo ""
    echo "Usage: $0 <port> [options]"
    echo ""
    echo "Arguments:"
    echo "  port           Serial device path (e.g. /dev/cu.usbserial-20120)"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help"
    echo "  -c, --config   Use settings from config file"
    echo "  -v, --version  Show version info"
    echo "  -m, --monitor  Jump straight into long-term monitor mode"
    echo ""
    echo "Examples:"
    echo "  $0 /dev/cu.usbserial-20120"
    echo "  $0 /dev/cu.usbserial-20120 --config"
    echo "  $0 /dev/cu.usbserial-20120 --monitor"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)    show_help; exit 0 ;;
        -v|--version) echo "InksPet Flash Tool $SCRIPT_VERSION"; exit 0 ;;
        -c|--config)  USE_CONFIG=true; shift ;;
        -m|--monitor) MONITOR_MODE=true; LOG_ONLY=true; shift ;;
        -*)
            echo -e "${RED}❌ Unknown option: $1${NC}"
            show_help
            exit 1
            ;;
        *)
            if [ -z "$PORT" ]; then
                PORT=$1
            else
                echo -e "${RED}❌ Extra argument: $1${NC}"
                show_help
                exit 1
            fi
            shift
            ;;
    esac
done

# Fall back to config file's default port
if [ -z "$PORT" ]; then
    if load_config && [ -n "$DEFAULT_PORT" ]; then
        echo -e "${YELLOW}🔌 Using default port from config: $DEFAULT_PORT${NC}"
        PORT=$DEFAULT_PORT
    else
        echo -e "${RED}❌ Please specify a serial port${NC}"
        echo ""
        show_help
        exit 1
    fi
fi

# ============================================
# Main flow
# ============================================

echo -e "${PURPLE}================================================${NC}"
echo -e "${CYAN}    InksPet Firmware Flash Tool $SCRIPT_VERSION${NC}"
echo -e "${CYAN}    AI Agent Desktop Companion${NC}"
echo -e "${PURPLE}================================================${NC}"
echo -e "${BLUE}🔌 Port: $PORT${NC}"

check_dependencies || exit 1
check_port "$PORT" || exit 1

[ "$USE_CONFIG" = true ] && load_config

if ! detect_device_info; then
    echo -e "${YELLOW}⚠️  Device detection failed, trying to fix connection...${NC}"
    if ! fix_flash_communication; then
        echo -e "${YELLOW}⚠️  Continue anyway? [y/N]${NC}"
        read -r continue_anyway
        if [[ ! "$continue_anyway" =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Cancelled.${NC}"
            exit 1
        fi
    else
        echo -e "${BLUE}🔍 Re-detecting device...${NC}"
        detect_device_info
    fi
fi

# ============================================
# Operation mode menu
# ============================================

if [ "$MONITOR_MODE" != true ]; then
    echo ""
    echo -e "${CYAN}Select operation mode:${NC}"
    echo "  1. Flash firmware / filesystem (normal mode)"
    echo "  2. Log device output only (no flashing)"
    echo "  3. Backup current firmware"
    echo "  4. Long-term monitor mode (event stats & analysis)"
    echo -n "Choose [1-4]: "
    read -r OPERATION_MODE

    case $OPERATION_MODE in
        2)
            LOG_ONLY=true
            echo -e "${GREEN}✅ Selected: log-only${NC}"
            ;;
        3)
            BACKUP_ONLY=true
            echo -e "${GREEN}✅ Selected: backup firmware${NC}"
            ;;
        4)
            MONITOR_MODE=true
            LOG_ONLY=true
            echo -e "${GREEN}✅ Selected: long-term monitor mode${NC}"
            ;;
        *)
            LOG_ONLY=false
            BACKUP_ONLY=false
            MONITOR_MODE=false
            echo -e "${GREEN}✅ Selected: normal flash mode${NC}"
            ;;
    esac
else
    echo ""
    echo -e "${GREEN}✅ Command-line selected: long-term monitor mode${NC}"
fi

# ============================================
# Backup mode
# ============================================

if [ "$BACKUP_ONLY" = true ]; then
    echo ""
    backup_dir="backups/$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir"

    echo -e "${BLUE}📦 Backing up firmware to $backup_dir${NC}"
    if execute_with_retry "esptool.py --port $PORT read_flash 0x0 0x400000 $backup_dir/firmware_backup.bin" "Firmware backup"; then
        echo -e "${GREEN}✅ Backup complete: $backup_dir/firmware_backup.bin${NC}"
    else
        echo -e "${RED}❌ Backup failed${NC}"
        exit 1
    fi
    exit 0
fi

# ============================================
# Flash options menu (skip if log-only)
# ============================================

if [ "$LOG_ONLY" = false ]; then
    echo ""
    echo -e "${CYAN}Select flash mode:${NC}"
    echo "  1. Full flash (firmware + LittleFS web portal)"
    echo "  2. Firmware only (skip LittleFS)"
    echo -n "Choose [1-2]: "
    read -r FLASH_MODE

    case $FLASH_MODE in
        1)
            SKIP_FILESYSTEM=false
            echo -e "${GREEN}✅ Selected: full flash${NC}"
            ;;
        2)
            SKIP_FILESYSTEM=true
            echo -e "${GREEN}✅ Selected: firmware only${NC}"
            ;;
        *)
            echo -e "${YELLOW}⚠️  Invalid choice, defaulting to full flash${NC}"
            SKIP_FILESYSTEM=false
            ;;
    esac

    echo ""
    echo -e "${CYAN}Erase flash before writing?${NC}"
    echo "  1. Yes — wipe all data (recommended for first-time flash or when debugging)"
    echo "  2. No  — preserve existing data (for firmware upgrades)"
    echo -n "Choose [1-2]: "
    read -r ERASE_OPTION

    case $ERASE_OPTION in
        1)
            ERASE_FLASH=true
            echo -e "${GREEN}✅ Selected: erase flash${NC}"
            ;;
        2)
            ERASE_FLASH=false
            echo -e "${GREEN}✅ Selected: preserve data${NC}"
            ;;
        *)
            echo -e "${YELLOW}⚠️  Invalid choice, defaulting to preserve data${NC}"
            ERASE_FLASH=false
            ;;
    esac

    save_config

    # Summary
    echo ""
    echo -e "${PURPLE}================================================${NC}"
    echo -e "${CYAN}📋 Plan:${NC}"
    echo "  1. Clean build artifacts"
    if [ "$ERASE_FLASH" = true ]; then
        echo "  2. Erase device flash"
        echo "  3. Build firmware (env: $BUILD_ENV)"
        echo "  4. Upload firmware"
        if [ "$SKIP_FILESYSTEM" = false ]; then
            echo "  5. Build & upload LittleFS web portal"
            echo "  6. Monitor device log"
        else
            echo "  5. Monitor device log"
        fi
    else
        echo "  2. Build firmware (env: $BUILD_ENV)"
        echo "  3. Upload firmware"
        if [ "$SKIP_FILESYSTEM" = false ]; then
            echo "  4. Build & upload LittleFS web portal"
            echo "  5. Monitor device log"
        else
            echo "  4. Monitor device log"
        fi
    fi
    echo -e "${PURPLE}================================================${NC}"

    if [ "$ERASE_FLASH" = true ]; then
        echo -e "${RED}⚠️  WARNING: this will erase ALL data on the device!${NC}"
    else
        echo -e "${YELLOW}ℹ️  Note: existing data will be preserved${NC}"
    fi
elif [ "$MONITOR_MODE" = true ]; then
    echo ""
    echo -e "${PURPLE}================================================${NC}"
    echo -e "${CYAN}📋 Plan:${NC}"
    echo "  1. Long-term device monitoring"
    echo "  2. Auto-tally error and exception events"
    echo "  3. Generate stability analysis report"
    echo -e "${PURPLE}================================================${NC}"
else
    echo ""
    echo -e "${PURPLE}================================================${NC}"
    echo -e "${CYAN}📋 Plan:${NC}"
    echo "  1. Log device output to file"
    echo -e "${PURPLE}================================================${NC}"
fi

echo ""
echo -e "${YELLOW}Press Enter to start, Ctrl+C to cancel...${NC}"
read -r

START_TIME=$(date +%s)

setup_log_directory
manage_logs

# ============================================
# Log-only mode skips flashing entirely
# ============================================

if [ "$LOG_ONLY" = true ]; then
    CURRENT_STEP=1
    echo ""
    show_progress 1 1 "Preparing to log..."
else
    # Step 1: Clean
    echo ""
    show_progress 1 6 "Cleaning build artifacts..."
    if ! execute_with_retry "pio run -e $BUILD_ENV -t clean" "Clean build"; then
        exit 1
    fi

    # Step 2: Erase (optional)
    if [ "$ERASE_FLASH" = true ]; then
        show_progress 2 6 "Erasing device flash..."

        local_erase_baud=${OPTIMAL_BAUD_RATE:-115200}
        echo -e "${CYAN}   Using baud: $local_erase_baud${NC}"
        erase_command="esptool.py --port $PORT --baud $local_erase_baud --before default_reset --after hard_reset erase_flash"

        if ! execute_with_retry "$erase_command" "Erase flash"; then
            echo -e "${YELLOW}⚠️  Trying slower baud rates...${NC}"
            for try_baud in 57600 9600; do
                echo -e "${YELLOW}   Trying baud: $try_baud${NC}"
                erase_slow="esptool.py --port $PORT --baud $try_baud --before default_reset --after hard_reset erase_flash"
                if execute_with_retry "$erase_slow" "Erase flash (baud:$try_baud)"; then
                    break
                fi
            done

            echo -e "${RED}❌ Erase failed, check hardware or try manual download mode${NC}"
            exit 1
        fi
        CURRENT_STEP=3
    else
        echo -e "${YELLOW}[skip] preserving flash data${NC}"
        CURRENT_STEP=2
    fi

    # Step 3: Build firmware
    show_progress $CURRENT_STEP 6 "Building firmware (env: $BUILD_ENV)..."
    if ! execute_with_retry "pio run -e $BUILD_ENV" "Build firmware"; then
        exit 1
    fi
    CURRENT_STEP=$((CURRENT_STEP + 1))

    # Step 4: Upload firmware
    show_progress $CURRENT_STEP 6 "Uploading firmware..."

    upload_command="pio run -e $BUILD_ENV -t upload --upload-port $PORT"
    if ! execute_with_retry "$upload_command" "Upload firmware"; then
        echo -e "${YELLOW}⚠️  PlatformIO upload failed, trying manual esptool...${NC}"

        firmware_bin=".pio/build/$BUILD_ENV/firmware.bin"
        bootloader_bin=".pio/build/$BUILD_ENV/bootloader.bin"
        partitions_bin=".pio/build/$BUILD_ENV/partitions.bin"
        boot_app0_bin=".pio/build/$BUILD_ENV/boot_app0.bin"

        if [ ! -f "$firmware_bin" ]; then
            echo -e "${RED}❌ Firmware binary not found: $firmware_bin${NC}"
            exit 1
        fi

        upload_baud=${OPTIMAL_BAUD_RATE:-115200}
        echo -e "${CYAN}   Using baud: $upload_baud${NC}"

        # Try multiple strategies, from fewest params to most conservative.
        # Order matters: the reference project's minimal form is tried first.
        strategies=()
        build_manual_cmd() {
            local baud=$1
            local cmd="esptool.py --port $PORT --baud $baud --before default_reset --after hard_reset write_flash"
            [ -f "$bootloader_bin" ] && cmd="$cmd 0x1000 $bootloader_bin"
            [ -f "$partitions_bin" ] && cmd="$cmd 0x8000 $partitions_bin"
            [ -f "$boot_app0_bin" ]  && cmd="$cmd 0xe000 $boot_app0_bin"
            cmd="$cmd 0x10000 $firmware_bin"
            echo "$cmd"
        }

        strategies=(
            "$(build_manual_cmd "$upload_baud")"
            "$(build_manual_cmd 115200)"
            "$(build_manual_cmd 57600)"
            "$(build_manual_cmd 9600)"
        )

        upload_success=false
        for strategy in "${strategies[@]}"; do
            baud_used=$(echo "$strategy" | grep -o 'baud [0-9]*' | cut -d' ' -f2)
            echo -e "${YELLOW}   Manual upload attempt (baud: $baud_used)...${NC}"
            if execute_with_retry "$strategy" "Manual upload (baud:$baud_used)"; then
                upload_success=true
                break
            fi
        done

        if [ "$upload_success" = false ]; then
            echo -e "${RED}❌ All upload strategies failed, check hardware${NC}"
            echo -e "${CYAN}💡 As a last resort, use the web flasher at inkspet.com${NC}"
            exit 1
        fi
    fi
    CURRENT_STEP=$((CURRENT_STEP + 1))

    # Step 5: Build & upload filesystem
    if [ "$SKIP_FILESYSTEM" = false ]; then
        show_progress $CURRENT_STEP 6 "Building & uploading LittleFS web portal..."
        if ! execute_with_retry "pio run -e $BUILD_ENV -t buildfs && pio run -e $BUILD_ENV -t uploadfs --upload-port $PORT" "Build & upload LittleFS"; then
            echo -e "${YELLOW}⚠️  Filesystem upload failed (web portal may be unavailable)${NC}"
        fi
        CURRENT_STEP=$((CURRENT_STEP + 1))
    else
        echo -e "${YELLOW}[skip] LittleFS upload${NC}"
    fi

    show_progress 6 6 "Flash complete"
fi

# ============================================
# Timing summary
# ============================================

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
MINUTES=$((DURATION / 60))
SECONDS=$((DURATION % 60))

echo ""
if [ "$LOG_ONLY" = false ]; then
    echo -e "${GREEN}🎉 Flash complete! Total time: ${MINUTES}m${SECONDS}s${NC}"
else
    echo -e "${GREEN}✅ Ready to start logging. Setup took: ${MINUTES}m${SECONDS}s${NC}"
fi

# ============================================
# Post-flash: logging or monitor dashboard
# ============================================

LOG_FILE=$(get_log_filename)

if [ "$MONITOR_MODE" = true ]; then
    run_monitor_mode
else
    echo ""
    if [ "$LOG_ONLY" = false ]; then
        echo -e "${BLUE}📊 Starting device monitor...${NC}"
    else
        echo -e "${BLUE}📊 Starting log capture...${NC}"
    fi
    echo -e "${CYAN}📁 Log file: ${LOG_FILE}${NC}"
    echo -e "${YELLOW}⏹️  Press Ctrl+C to exit${NC}"
    sleep 2

    {
        echo "===== InksPet Serial Log: $(date) ====="
        echo "Port: $PORT"
        echo "========================================="
    } > "$LOG_FILE"

    export LANG=en_US.UTF-8
    export LC_ALL=en_US.UTF-8
    pio device monitor --port "$PORT" --baud 115200 --filter direct | tee -a "$LOG_FILE"

    {
        echo ""
        echo "===== End: $(date) ====="
    } >> "$LOG_FILE"
fi
