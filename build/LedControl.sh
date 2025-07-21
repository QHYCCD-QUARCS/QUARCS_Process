#!/bin/bash

LED="ACT"
COMMAND=$1

# 检查参数数量
if [ "$COMMAND" == "flash" ] && [ $# -ne 3 ]; then
    echo "错误：参数不足。用法：./LedControl.sh <COMMAND> <SPEED> <MODEL>"
    exit 1
elif [ "$COMMAND" != "flash" ] && [ $# -ne 1 ]; then
    echo "错误：参数不足。用法：./LedControl.sh <COMMAND>"
    exit 1
fi

# 如果COMMAND是'flash'，赋值SPEED和MODEL
if [ "$COMMAND" == "flash" ]; then
    SPEED=$2
    MODEL=$3
fi

# 检查COMMAND参数
if [ "$COMMAND" != "disable" ] && [ "$COMMAND" != "restore" ] && [ "$COMMAND" != "flash" ]; then
    echo "错误：无效的COMMAND参数。有效的值包括 'disable', 'restore', 'flash'"
    exit 1
fi

# 如果COMMAND是'flash'，检查SPEED参数
if [ "$COMMAND" == "flash" ] && [ "$SPEED" != "fast" ] && [ "$SPEED" != "slow" ]; then
    echo "错误：无效的SPEED参数。有效的值包括 'fast', 'slow'"
    exit 1
fi

function open_led {
    if [ $MODEL -eq 4 ]; then
        echo 1 | sudo tee /sys/class/leds/$LED/brightness > /dev/null
    elif [ $MODEL -eq 5 ]; then
        echo 0 | sudo tee /sys/class/leds/$LED/brightness > /dev/null
    fi
}

function close_led {
    if [ $MODEL -eq 4 ]; then
        echo 0 | sudo tee /sys/class/leds/$LED/brightness > /dev/null
    elif [ $MODEL -eq 5 ]; then
        echo 1 | sudo tee /sys/class/leds/$LED/brightness > /dev/null
    fi
}

function fast_flash_led {
    while true; do
        echo "fast_flash_led"
        open_led
        sleep 0.5
        close_led
        sleep 0.5
    done
}

function slow_flash_led {
    while true; do
        echo "slow_flash_led"
        open_led
        sleep 2
        close_led
        sleep 1
    done
}

function disable_trigger {
    echo none | sudo tee /sys/class/leds/$LED/trigger > /dev/null
}

function restore_trigger {
    echo mmc0 | sudo tee /sys/class/leds/$LED/trigger > /dev/null
}

if [ "$COMMAND" == "disable" ]; then
    disable_trigger
elif [ "$COMMAND" == "restore" ]; then
    restore_trigger
elif [ "$COMMAND" == "flash" ]; then
    if [ "$SPEED" == "fast" ]; then
        fast_flash_led
    elif [ "$SPEED" == "slow" ]; then
        slow_flash_led
    fi
fi