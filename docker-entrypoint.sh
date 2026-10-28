#!/bin/bash
set -e

# Link novnc index.html if needed
if [ -f "/usr/share/novnc/vnc.html" ] && [ ! -f "/usr/share/novnc/index.html" ]; then
    ln -s /usr/share/novnc/vnc.html /usr/share/novnc/index.html
fi

# Check if DISPLAY is provided by host (WSLg or external X11 server)
if [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" -a "$DISPLAY" != ":1" ] && [ -e "/tmp/.X11-unix" ]; then
    echo "=========================================================="
    echo " Starting SmartSched via Host Display (DISPLAY=$DISPLAY)"
    echo "=========================================================="
    exec /app/SmartSched "$@"
fi

# Otherwise, launch headless virtual display + noVNC Web Server on port 8080
echo "=========================================================="
echo " Starting SmartSched Web-GUI via noVNC (Port 8080)..."
echo " Open your browser at: http://localhost:8080"
echo "=========================================================="

export DISPLAY=:1

# 1. Start virtual X framebuffer
Xvfb :1 -screen 0 1340x860x24 -ac +extension GLX +render -noreset &
sleep 1

# 2. Start lightweight window manager
openbox &
sleep 1

# 3. Start VNC server (display :1 -> port 5900)
x11vnc -display :1 -nopw -listen 127.0.0.1 -xkb -forever -shared &
sleep 1

# 4. Start Websockify bridge for HTML5 browser access (port 8080 -> 5900)
websockify --web=/usr/share/novnc/ 8080 127.0.0.1:5900 &
sleep 1

# 5. Launch SmartSched in foreground
exec /app/SmartSched "$@"
