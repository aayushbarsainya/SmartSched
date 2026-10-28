# ==============================================================================
# SmartSched Dockerfile - Multi-Stage Build with noVNC Web GUI Support
# ==============================================================================

# Stage 1: Compiler & Build
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsdl2-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B /build_docker -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build_docker -j$(nproc)

# ==============================================================================
# Stage 2: Minimal Runtime Environment
# ==============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libsdl2-2.0-0 \
    xvfb \
    x11vnc \
    novnc \
    websockify \
    openbox \
    procps \
    htop \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build_docker/SmartSched /app/SmartSched
COPY --from=builder /app/image_tasks /app/image_tasks
COPY --from=builder /app/real_data.txt /app/real_data.txt
COPY docker-entrypoint.sh /app/docker-entrypoint.sh

RUN chmod +x /app/docker-entrypoint.sh /app/SmartSched

# Port 8080 for HTML5 Web-GUI access (via noVNC)
EXPOSE 8080

ENTRYPOINT ["/app/docker-entrypoint.sh"]
