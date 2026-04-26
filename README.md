# nesEMU

A cycle-accurate NES (Nintendo Entertainment System) emulator built in C++. Designed as a teaching tool — every internal component is exposed through live debug panels so you can watch the hardware working in real time as games run.

---

## Installation

1. Head to [Releases](https://github.com/xqill275/nesEMU/releases)
2. Download the latest release `.zip` file (current version: 0.6 — April 2026)
3. Extract the `.zip` file
4. Run the provided `.exe`

**Windows only.** Linux and macOS are not currently supported.

> **ROMs are not included and cannot be provided.** You will need to source your own ROM files. Only use ROMs for games you legally own.

---

## Controls

Default keyboard bindings. These can be changed in **Settings → Change Keybinds**.

| NES Button | Default Key |
|------------|-------------|
| A          | E           |
| B          | Q           |
| Start      | Enter       |
| Select     | Right Shift |
| D-Pad Up   | W           |
| D-Pad Down | S           |
| D-Pad Left | A           |
| D-Pad Right| D           |

| Emulator Action  | Default Key |
|------------------|-------------|
| Run / Pause      | F5          |
| Reset            | F1          |
| Step Instruction | F6          |

---

## Loading a Game

1. Launch the emulator
2. Go to **File → Open ROM...**
3. Select your `.nes` ROM file
4. Press **Space** (or **Game → Run**) to start

---

## Debug Panels

All panels can be toggled from the **View** menu.

| Panel | What it shows |
|-------|---------------|
| CPU | Live register values (A, X, Y, SP, PC) and status flags |
| Memory | Raw memory contents around the current program counter |
| Stack | Current stack contents |
| PPU | PPU register values, VRAM address, scanline and cycle counters |
| VRAM | Raw nametable memory ($2000–$27FF) |
| Pattern Tables | Live tile graphics the PPU is reading from the cartridge |
| APU | Audio channel status, enable flags, and raw register values |

---

## Game Compatibility

Games tested personally. Compatibility depends on the mapper the cartridge uses.

### Status Legend

| Status | Meaning |
|--------|---------|
| **Playable** | Fully playable from start to finish |
| **In-Game** | Runs but with noticeable issues |
| **Boots** | Reaches title screen or gameplay but is significantly broken |
| **Does Not Boot** | Fails to start |

### Tested Games

| Game | Mapper | Region | Status | Notes |
|------|--------|--------|--------|-------|
| Contra | MMC1 (1) | NTSC | Playable | |
| DigDug | NROM (0) | NTSC | Playable | |
| Dr. Mario | UNROM (2) | NTSC | Playable | |
| Duck Tales | MMC1 (1) | NTSC | Playable | |
| ExciteBike | NROM (0) | NTSC | Playable | |
| Final Fantasy | MMC1 (1) | NTSC | Playable | |
| Ghost 'n Goblins | MMC1 (1) | NTSC | Playable | |
| Mario & Yoshi | MMC1 (1) | NTSC | Playable | |
| Mega Man 2 | UNROM (2) | NTSC | Playable | |
| Metroid | MMC1 (1) | NTSC | Playable | |
| Punch-Out!! | MMC2 (9) | NTSC | In-Game | Graphical errors present |
| Super Mario Bros. | NROM (0) | NTSC | Playable | |
| Tetris | NROM (0) | NTSC | Boots | Menu works correctly, screen goes black on entering gameplay |

*This list will be updated as testing continues.*

---

## Mapper Support

| Mapper | Name | Notable Games |
|--------|------|---------------|
| 0 | NROM | Donkey Kong, Super Mario Bros., Galaga |
| 1 | MMC1 | The Legend of Zelda, Metroid |
| 2 | UNROM | Mega Man, Castlevania |
| 9 | MMC2 | Punch-Out!! |

Games using other mappers are not currently supported and will likely fail to boot.

---

## Known Limitations

- **Windows only** — no Linux or macOS support at this time
- **No save states** — battery-backed in-game saves work where supported by the mapper, but there is no save state system
- **1 player only** — controller 2 is not currently mapped

---

## Built With

- C++
- OpenGL 3.3 + GLAD
- GLFW
- Dear ImGui
- miniaudio

---

## License

This project is released for educational purposes. No ROM files are included
