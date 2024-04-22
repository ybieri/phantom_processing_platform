#!/bin/bash

socket_path=$(mktemp /tmp/sockets/socket_XXXXXX)

cleanup() {
    sleep 1
    kill -TERM -$$  2>/dev/null
    rm -f "$socket_path"  
}

enforce_timeout() {
    local timeout_seconds=20
    sleep $timeout_seconds
    cleanup
    kill -TERM -$$  
}

trap cleanup EXIT

enforce_timeout &

/processor_arm "$socket_path" > /dev/null 2>&1 &

sleep 0.1  

/sensor_arm "$socket_path" 