# How to Build (Linux)

Build the **RG34XX-H Refresh Switcher** on a Linux PC using Docker. The resulting program is an **AArch64/ARM64** executable for MustardOS, not a native x86-64 Linux application.

## Requirements

- Docker (with permission to run `docker`)
- `zip` and `file` on the Linux host
- The project files: `Dockerfile`, `main.c`, `engine.h`, `flash.h`, `timing.h`, and `mux_launch.sh` (either at the project root or in `RG34XXSwitcher/`)

## 1. Build the executable

From the project directory:

```sh
docker build -t rg34xx-builder .

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src \
  rg34xx-builder \
  aarch64-linux-gnu-gcc \
    -std=gnu11 -O2 -Wall -Wextra -D_GNU_SOURCE \
    -I/usr/include/SDL2 -D_REENTRANT \
    main.c -o rg34xx-tool -lSDL2 -lm
```

Verify the architecture:

```sh
file rg34xx-tool
```

The output must identify an **AArch64 (ARM64) ELF** executable.

## 2. Create the `.muxapp` package

The `.muxapp` is a ZIP archive containing the application folder. Its internal folder name must match the path used by `mux_launch.sh` (`RG34XXSwitcher`).

```sh
# Find the launcher in either location.
if [ -f mux_launch.sh ]; then
  LAUNCHER=mux_launch.sh
else
  LAUNCHER=RG34XXSwitcher/mux_launch.sh
fi

test -f "$LAUNCHER" || { echo "mux_launch.sh not found" >&2; exit 1; }

# Recreate a clean packaging directory.
rm -rf build/muxapp
mkdir -p build/muxapp/RG34XXSwitcher
cp rg34xx-tool build/muxapp/RG34XXSwitcher/
cp "$LAUNCHER" build/muxapp/RG34XXSwitcher/mux_launch.sh
chmod +x build/muxapp/RG34XXSwitcher/rg34xx-tool \
         build/muxapp/RG34XXSwitcher/mux_launch.sh

rm -f RG34XXSwitcher.muxapp
(cd build/muxapp && zip -r ../../RG34XXSwitcher.muxapp RG34XXSwitcher/)
```

Check the archive:

```sh
unzip -l RG34XXSwitcher.muxapp
sha256sum RG34XXSwitcher.muxapp
```

Expected contents:

```text
RG34XXSwitcher/
├── mux_launch.sh
└── rg34xx-tool
```

## 3. Install on MustardOS

Copy `RG34XXSwitcher.muxapp` to the device's **ARCHIVE** folder and install it through the MustardOS **Archive Manager**. Reboot the device after applying display timing changes.

> **Warning:** The application writes to the SD card's boot area. It was tested on an **RG34XX-H with MustardOS Andromeda 2606.0**. Make a full SD card backup before using it. Other firmware versions and RG34XX variants (including SP models) are not verified and may require changes to the code.

## Notes

- Do not commit `rg34xx-tool`, `build/`, or `*.muxapp` to Git; attach the `.muxapp` to a **GitHub Release** instead.
- The Dockerfile supplies the ARM64 cross-compiler and SDL2 development libraries.
- These commands describe the project's existing flat source layout. The build should be tested on the target device before publishing a release.
