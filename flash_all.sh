#!/bin/bash
# flash_all.sh - InksPet firmware flash tool
# Based on EleksCava flash tool v2.0, adapted for InksPet project

SCRIPT_VERSION="v1.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Config
BUILD_ENV="esp32dev"
SKIP_FILESYSTEM=false
ERASE_FLASH=false
LOG_ONLY=false
MONITOR_MODE=false
MAX_RETRIES=3
RETRY_DELAY=5
FLASH_BAUD_RATES=(460800 230400 115200 57600)
OPTIMAL_BAUD_RATE=""

# Cross-platform timeout helper
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

# Auto-detect best baud rate
find_optimal_baud_rate() {
    local test_port=$1
    echo -e "${BLUE}🔧 Auto-detecting best baud rate...${NC}"

    for baud in "${FLASH_BAUD_RATES[@]}"; do
        echo -e "${YELLOW}   Testing $baud...${NC}"
        if run_with_timeout 8 "esptool.py --port $test_port --baud $baud chip_id" > /dev/null 2>&1; then
            echo -e "${GREEN}✅ Stable baud rate: $baud${NC}"
            OPTIMAL_BAUD_RATE=$baud
            return 0
        fi
    done

    echo -e "${RED}❌ All baud rates failed${NC}"
    return 1
}

# Check port
check_port() {
    local port=$1
    if [ ! -e "$port" ]; then
        echo -e "${RED}❌ Port $port not found${NC}"
        echo -e "${YELLOW}💡 Available ports:${NC}"
        ls /dev/cu.usb* 2>/dev/null || echo "   (no USB serial devices found)"
        return 1
    fi
    echo -e "${GREEN}✅ Port $port OK${NC}"
    return 0
}

# Detect device
detect_device_info() {
    echo -e "${BLUE}🔍 Detecting device...${NC}"
    local temp_file="/tmp/esptool_output_$$"

    if run_with_timeout 10 "esptool.py --port $PORT flash_id" > "$temp_file" 2>&1; then
        if grep -q "ESP32" "$temp_file"; then
            local chip_type=$(grep "Chip is" "$temp_file" | sed 's/Chip is //' | awk '{print $1}')
            local mac_addr=$(grep "MAC:" "$temp_file" | awk '{print $2}')
            echo -e "${GREEN}✅ Device: $chip_type${NC}"
            [ -n "$mac_addr" ] && echo -e "${CYAN}   MAC: $mac_addr${NC}"
            rm -f "$temp_file"
            return 0
        fi
    fi

    rm -f "$temp_file"
    echo -e "${RED}❌ Cannot connect to device${NC}"
    echo -e "${CYAN}💡 Tips:${NC}"
    echo "   1. Hold BOOT button and press RST"
    echo "   2. Release RST, keep holding BOOT for 3s"
    echo "   3. Release BOOT and try again"
    return 1
}

# Retry wrapper
execute_with_retry() {
    local command="$1"
    local description="$2"
    local retry_count=0

    while [ $retry_count -lt $MAX_RETRIES ]; do
        echo -e "${BLUE}🔄 $description (attempt $((retry_count + 1))/$MAX_RETRIES)${NC}"
        if eval "$command"; then
            echo -e "${GREEN}✅ $description OK${NC}"
            return 0
        fi
        retry_count=$((retry_count + 1))
        [ $retry_count -lt $MAX_RETRIES ] && sleep $RETRY_DELAY
    done

    echo -e "${RED}❌ $description failed after $MAX_RETRIES attempts${NC}"
    return 1
}

# Help
show_help() {
    echo -e "${CYAN}InksPet Firmware Flash Tool $SCRIPT_VERSION${NC}"
    echo ""
    echo "Usage: $0 <port> [options]"
    echo ""
    echo "Arguments:"
    echo "  port           Serial port (e.g. /dev/cu.usbserial-20120)"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help"
    echo "  -m, --monitor  Monitor mode (serial log only)"
    echo "  -f, --fw-only  Flash firmware only (skip filesystem)"
    echo "  -e, --erase    Erase flash before writing"
    echo ""
    echo "Examples:"
    echo "  $0 /dev/cu.usbserial-20120           # Full flash (firmware + web portal)"
    echo "  $0 /dev/cu.usbserial-20120 -e        # Erase + full flash"
    echo "  $0 /dev/cu.usbserial-20120 -f        # Firmware only"
    echo "  $0 /dev/cu.usbserial-20120 -m        # Serial monitor"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help) show_help; exit 0 ;;
        -m|--monitor) MONITOR_MODE=true; LOG_ONLY=true; shift ;;
        -f|--fw-only) SKIP_FILESYSTEM=true; shift ;;
        -e|--erase) ERASE_FLASH=true; shift ;;
        -*) echo -e "${RED}❌ Unknown option: $1${NC}"; show_help; exit 1 ;;
        *)
            if [ -z "$PORT" ]; then PORT=$1; else echo -e "${RED}❌ Extra argument: $1${NC}"; exit 1; fi
            shift ;;
    esac
done

# Validate port
if [ -z "$PORT" ]; then
    echo -e "${RED}❌ Please specify a serial port${NC}"
    echo ""
    show_help
    exit 1
fi

# Banner
echo -e "${PURPLE}================================================${NC}"
echo -e "${CYAN}    InksPet Firmware Flash Tool $SCRIPT_VERSION${NC}"
echo -e "${CYAN}    AI Agent Desktop Companion${NC}"
echo -e "${PURPLE}================================================${NC}"
echo -e "${BLUE}🔌 Port: $PORT${NC}"

# Check port
if ! check_port "$PORT"; then
    exit 1
fi

# Detect device
if ! detect_device_info; then
    echo -e "${YELLOW}⚠️  Device detection failed. Try fixing connection...${NC}"
    if ! find_optimal_baud_rate "$PORT"; then
        echo -e "${CYAN}💡 Enter download mode manually:${NC}"
        echo "   1. Hold BOOT button"
        echo "   2. Press RST button"
        echo "   3. Release RST, keep BOOT for 3 seconds"
        echo "   4. Release BOOT"
        echo ""
        echo -e "${YELLOW}Press Enter when ready, or Ctrl+C to cancel...${NC}"
        read
        if ! find_optimal_baud_rate "$PORT"; then
            echo -e "${RED}❌ Still cannot connect. Check hardware.${NC}"
            exit 1
        fi
    fi
fi

# Monitor mode
if [ "$MONITOR_MODE" = true ]; then
    echo ""
    echo -e "${BLUE}📊 Starting serial monitor...${NC}"
    echo -e "${YELLOW}⏹️  Press Ctrl+C to exit${NC}"

    mkdir -p logs
    LOG_FILE="logs/serial_$(date +%Y%m%d_%H%M%S).log"
    echo "===== InksPet Serial Log: $(date) =====" > "$LOG_FILE"

    pio device monitor --port "$PORT" --baud 115200 --filter direct | tee -a "$LOG_FILE"

    echo "" >> "$LOG_FILE"
    echo "===== End: $(date) =====" >> "$LOG_FILE"
    echo -e "${GREEN}📁 Log saved: ${LOG_FILE}${NC}"
    exit 0
fi

# Flash mode summary
echo ""
echo -e "${PURPLE}────────────────────────────────────────${NC}"
echo -e "${CYAN}📋 Flash plan:${NC}"
[ "$ERASE_FLASH" = true ] && echo "   1. Erase flash (full wipe)"
echo "   $([ "$ERASE_FLASH" = true ] && echo "2" || echo "1"). Build firmware"
echo "   $([ "$ERASE_FLASH" = true ] && echo "3" || echo "2"). Upload firmware"
if [ "$SKIP_FILESYSTEM" = false ]; then
    echo "   $([ "$ERASE_FLASH" = true ] && echo "4" || echo "3"). Build & upload web portal (LittleFS)"
fi
echo "   Last. Serial monitor"
echo -e "${PURPLE}────────────────────────────────────────${NC}"

if [ "$ERASE_FLASH" = true ]; then
    echo -e "${RED}⚠️  WARNING: This will erase ALL data on device!${NC}"
fi
echo ""
echo -e "${YELLOW}Press Enter to start, Ctrl+C to cancel...${NC}"
read

START_TIME=$(date +%s)

# Step: Erase flash
if [ "$ERASE_FLASH" = true ]; then
    echo ""
    erase_baud=${OPTIMAL_BAUD_RATE:-115200}
    if ! execute_with_retry "esptool.py --port $PORT --baud $erase_baud --before default_reset --after hard_reset erase_flash" "Erase flash"; then
        echo -e "${RED}❌ Erase failed${NC}"
        exit 1
    fi
fi

# Step: Build firmware
echo ""
if ! execute_with_retry "pio run -e $BUILD_ENV" "Build firmware"; then
    exit 1
fi

# Step: Upload firmware
echo ""
upload_command="pio run -e $BUILD_ENV -t upload --upload-port $PORT"
if ! execute_with_retry "$upload_command" "Upload firmware"; then
    echo -e "${YELLOW}⚠️  PlatformIO upload failed, trying manual esptool...${NC}"

    firmware_bin=".pio/build/$BUILD_ENV/firmware.bin"
    bootloader_bin=".pio/build/$BUILD_ENV/bootloader.bin"
    partitions_bin=".pio/build/$BUILD_ENV/partitions.bin"
    upload_baud=${OPTIMAL_BAUD_RATE:-115200}

    if [ -f "$firmware_bin" ]; then
        manual_cmd="esptool.py --port $PORT --baud $upload_baud --before default_reset --after hard_reset write_flash"
        [ -f "$bootloader_bin" ] && manual_cmd="$manual_cmd 0x1000 $bootloader_bin"
        [ -f "$partitions_bin" ] && manual_cmd="$manual_cmd 0x8000 $partitions_bin"
        manual_cmd="$manual_cmd 0x10000 $firmware_bin"

        if ! execute_with_retry "$manual_cmd" "Manual firmware upload"; then
            echo -e "${RED}❌ Firmware upload failed${NC}"
            exit 1
        fi
    else
        echo -e "${RED}❌ Firmware binary not found: $firmware_bin${NC}"
        exit 1
    fi
fi

# Step: Build & upload filesystem
if [ "$SKIP_FILESYSTEM" = false ]; then
    echo ""
    if ! execute_with_retry "pio run -e $BUILD_ENV -t buildfs && pio run -e $BUILD_ENV -t uploadfs --upload-port $PORT" "Build & upload web portal"; then
        echo -e "${YELLOW}⚠️  Filesystem upload failed (web portal may be unavailable)${NC}"
    fi
fi

# Done
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
MINUTES=$((DURATION / 60))
SECONDS=$((DURATION % 60))

echo ""
echo -e "${GREEN}🎉 Flash complete! Total time: ${MINUTES}m${SECONDS}s${NC}"
echo ""

# Start serial monitor
echo -e "${BLUE}📊 Starting serial monitor...${NC}"
echo -e "${YELLOW}⏹️  Press Ctrl+C to exit${NC}"
echo ""

mkdir -p logs
LOG_FILE="logs/serial_$(date +%Y%m%d_%H%M%S).log"
echo "===== InksPet Serial Log: $(date) =====" > "$LOG_FILE"

pio device monitor --port "$PORT" --baud 115200 --filter direct | tee -a "$LOG_FILE"

echo "" >> "$LOG_FILE"
echo "===== End: $(date) =====" >> "$LOG_FILE"
echo -e "${GREEN}📁 Log saved: ${LOG_FILE}${NC}"
