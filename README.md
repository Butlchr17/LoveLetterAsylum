# Love Letter Asylum

Love Letter Asylum is a C++ game aimed at Steam. It is a dark, humorous horror game set in a mental asylum, inspired by Darkest Dungeon.

A session follows one path. New Game asks for one of five JSON save slots, then plays a short intro. Party selection is next. The party has four slots from left to right. The player places exactly two of the five starter colors, Red, Blue, Green, Yellow, and White. Empty slots are allowed. Keep this party starts the tutorial.

The tutorial is one walkable room. Left and right, or A and D, move the party. The rightmost occupied character leads. Each other character keeps a short gap behind the character ahead. On a turn, only the leader moves until they have passed the next character by that gap. That character then turns and follows. A mark on the leading side of each body flips when that character turns.

The room draws an expedition board along the bottom. The top bar names the room and tracks the walk. The board shows the four party slots, empty skill plates, and an empty pack. A chart at the bottom right draws five rooms linked by halls. Only the first room can be walked. The other four are a placeholder layout. Escape pauses. Pause can continue, open options, or return to the main menu. Options covers display, audio, controls, and access. Audio levels are saved, and no sound plays yet. Credits has no handler yet.

Continue loads a save. Exactly two different starter colors resume in the tutorial room. Any other save opens party selection.

Modules talk to each other through contracts. A module can be left out without breaking the others. Drawing is Vulkan. The window is an XCB surface loaded from `libxcb.so.1`.

## Packages

The game builds and runs on Ubuntu x86-64. These packages cover the compiler, CMake, Git, the Vulkan loader, a Vulkan driver, and the XCB library the window loads at runtime:

| Package | Why it is needed |
| --- | --- |
| `build-essential` | Compiler, linker, and Make |
| `g++` | C++20 compiler |
| `cmake` | Configure and build. Version 3.25 or newer |
| `git` | Clone the repository and publish changes |
| `ca-certificates` | Download Vulkan-Headers over HTTPS |
| `libvulkan1` | `libvulkan.so.1` in `/usr/lib/x86_64-linux-gnu` |
| `mesa-vulkan-drivers` | A Vulkan driver the loader can use |
| `libxcb1` | `libxcb.so.1`, loaded when the window opens |

Vulkan-Headers v1.4.341 is not a system package. CMake downloads it on the first configure, so that step needs network access. The window code matches the libxcb 1.8 layouts on x86-64. A running X11 display is also required, and `DISPLAY` must be set.

From the repository root, this installs the packages and checks that CMake, the loader, XCB, and a Vulkan driver are present:

```bash
chmod +x scripts/install-deps.sh
./scripts/install-deps.sh
```

The script runs this install:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    g++ \
    git \
    libvulkan1 \
    libxcb1 \
    mesa-vulkan-drivers
```

## Build and run

Install the packages above first.

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
./build/loveletter
```

Run the game from the repository root. Saves go in `saves/` and device settings go in `settings/`. Both directories are gitignored.

The tests:

```bash
./build/save_slots_test
./build/walk_test
./build/settings_test
./build/text_fit_test
./build/party_test
./build/dungeon_hud_test
```

Each test exits 0 when it passes. `text_fit_test` also prints `ALL_OK`.

## Contributions

Story, characters, and new mechanics come from the project owner. On-screen text can use only the bitmap font: letters, digits, period, and slash.

Clone the repository, make a branch, then build and test before you push:

```bash
git clone https://github.com/Butlchr17/LoveLetterAsylum.git
cd LoveLetterAsylum
chmod +x scripts/install-deps.sh
./scripts/install-deps.sh
git checkout -b my-change
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
./build/save_slots_test
./build/walk_test
./build/settings_test
./build/text_fit_test
./build/party_test
./build/dungeon_hud_test
./build/loveletter
git add .
git commit
git push -u origin HEAD
```

`git add .` skips `build/`, `saves/`, and `settings/` because they are gitignored. Open a pull request on GitHub after the push.
