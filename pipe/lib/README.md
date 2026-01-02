# External Libraries

## Dawn (WebGPU)

Clone and build:
```bash
cd lib
git clone --depth 1 https://dawn.googlesource.com/dawn
./dawn.sh
```

Prerequisites (Ubuntu/Debian):
```bash
sudo apt-get install libxrandr-dev libxinerama-dev libxcursor-dev \
    mesa-common-dev libx11-xcb-dev pkg-config python3
```

After build:
- Headers: `lib/dawn/install/include`
- Libraries: `lib/dawn/install/lib`
