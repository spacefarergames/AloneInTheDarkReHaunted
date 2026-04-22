#!/bin/bash
# Ensure WSLg env is set (some shell invocations drop these)
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/mnt/wslg/runtime-dir}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DISPLAY="${DISPLAY:-:0}"
export PULSE_SERVER="${PULSE_SERVER:-unix:/mnt/wslg/PulseServer}"
cd /mnt/d/FITD/build/linux/Fitd
nohup ./Tatou > /tmp/tatou_run.log 2>&1 &
PID=$!
echo "pid=$PID"
sleep 8
if kill -0 $PID 2>/dev/null; then
  echo "RUNNING (pid $PID)"
else
  echo "EXITED with status $?"
fi
echo "--- log tail ---"
tail -60 /tmp/tatou_run.log
