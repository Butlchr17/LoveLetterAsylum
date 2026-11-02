#!/usr/bin/env bash
# Install the Ubuntu packages needed to build and run Love Letter Asylum.
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This script needs apt-get. It supports Ubuntu and Debian." >&2
    exit 1
fi

packages=(
    build-essential
    ca-certificates
    cmake
    g++
    git
    libvulkan1
    libxcb1
    mesa-vulkan-drivers
)

sudo apt-get update
sudo apt-get install -y "${packages[@]}"

cmake_version="$(cmake --version | awk 'NR == 1 { print $3 }')"
if ! printf '%s\n%s\n' "3.25" "${cmake_version}" | sort -C -V; then
    echo "cmake ${cmake_version} is older than 3.25." >&2
    exit 1
fi

if [[ ! -e /usr/lib/x86_64-linux-gnu/libvulkan.so.1 ]]; then
    echo "libvulkan.so.1 is missing from /usr/lib/x86_64-linux-gnu." >&2
    exit 1
fi

if [[ ! -e /usr/lib/x86_64-linux-gnu/libxcb.so.1 ]]; then
    echo "libxcb.so.1 is missing from /usr/lib/x86_64-linux-gnu." >&2
    exit 1
fi

if ! compgen -G "/usr/share/vulkan/icd.d/*.json" >/dev/null; then
    echo "No Vulkan driver was found in /usr/share/vulkan/icd.d." >&2
    exit 1
fi

echo "Love Letter Asylum dependencies are installed."
echo "cmake ${cmake_version}"
echo "g++ $(g++ -dumpversion)"
