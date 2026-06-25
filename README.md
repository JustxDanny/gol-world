# gol-world — Conway's Game of Life (ghost-border edition)

![ci](https://github.com/JustxDanny/gol-world/actions/workflows/ci.yml/badge.svg)

A terminal Game of Life in C with **ncurses**, drawn inside a centred bordered
window. This variation keeps the board in a heap-allocated `World` with a one-cell
**ghost border**: the real edges are copied into that border before each tick, so
the board wraps like a torus and neighbour counting never needs modulo maths. Two
buffers are swapped by **exchanging pointers** — no memory is copied between
generations.

```
 +--------------------------------------------------------+
 |               O                                        |
 |                O                                       |
 |              OOO                                       |
 |                                                        |
 +--------------------------------------------------------+
 gen:37  pop:62  delay:120ms  A:+ Z:- Space:quit
```

## Rules

Conway's standard **B3/S23** on an 80×25 **toroidal** board: a living cell with
2–3 neighbours survives, a dead cell with exactly 3 neighbours is born, all else
dies.

## Build & run

```sh
cd src
make
./game_of_life < ../seeds/gosper_gun.txt
```

The starting board is read from **standard input**; any of `* O o X x # @ + 1`
marks a living cell. Ready-made patterns are in [`seeds/`](seeds).

## Controls

| Key | Action |
|-----|--------|
| `A` | speed up |
| `Z` | slow down |
| `Space` | quit |

A bare run (no input) starts with a single glider.

## Patterns (`seeds/`)

`block` (still life) · `blinker_toad` / `pulsar` (oscillators) · `glider` / `lwss`
(spaceships) · `gosper_gun` (glider gun) · `r_pentomino` (methuselah).

## How it works (architecture)

- **State:** a `World { int h, w; char* cur; char* nxt; }`, each buffer
  `(h+2)·(w+2)` bytes including a ghost border.
- **Wrap:** `wrap_borders()` copies the four edges and corners into the opposite
  ghost cells before each tick, so the inner loop reads neighbours directly.
- **No-copy step:** after building the next board, `cur` and `nxt` pointers are
  swapped (ping-pong).
- **Input handover:** the seed is read from stdin, then the controlling terminal is
  reopened so ncurses can read live keys.
- **Tested:** a hidden headless mode (set `GOL_FRAMES`) runs headless so CI can valgrind the
  allocation path for leaks and assert known populations.

## Tests

GitHub Actions runs **clang-format-18** (Google style), **cppcheck 2.13**, a
`-Wall -Wextra -Werror` build, a **valgrind** leak check on the heap path, and
population invariants on every push. See
[`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## Family

One of three independent takes on the same task:
[gol-classic](https://github.com/JustxDanny/gol-classic) ·
[gol-world](https://github.com/JustxDanny/gol-world) ·
[gol-automaton](https://github.com/JustxDanny/gol-automaton).
