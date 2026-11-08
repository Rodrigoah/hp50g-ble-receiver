# HP 50g key codes

Codes returned by `0 WAIT` on the HP 50g, measured with the `KEYS` program
(`« STD -34 CF DO 0 WAIT PR1 DROP UNTIL 0 END »`).

The `TECLA` program sends each press as `K` + code (e.g. `K36.1`) so the ESP32
can tell key presses apart from printed numbers:
`« STD -34 CF CLLCD "ENVIANDO" 2 DISP "PULSACIONES" 3 DISP "ON = SALIR" 6 DISP DO 0 WAIT →STR "K" SWAP + PR1 DROP UNTIL 0 END »`

Format is `RC.P`: row, column, shift plane (`.1` unshifted, `.2` left-shift,
`.3` right-shift). Only unshifted codes are listed. The "Sent to PC" column
mirrors `keymap[]` in `src/main.cpp`; "—" means not mapped (ignored).

| Code  | Calculator key | Sent to PC  |
|-------|----------------|-------------|
| 11.1  | F1             | F1          |
| 12.1  | F2             | F2          |
| 13.1  | F3             | F3          |
| 14.1  | F4             | F4          |
| 15.1  | F5             | F5          |
| 16.1  | F6             | F6          |
| 21.1  | APPS           | —           |
| 22.1  | MODE           | —           |
| 23.1  | TOOL           | —           |
| 25.1  | ▲ up arrow     | Up arrow    |
| 31.1  | VAR            | —           |
| 32.1  | STO            | —           |
| 33.1  | NXT            | —           |
| 34.1  | ◄ left arrow   | Left arrow  |
| 35.1  | ▼ down arrow   | Down arrow  |
| 36.1  | ► right arrow  | Right arrow |
| 41.1  | HIST           | —           |
| 42.1  | EVAL           | —           |
| 43.1  | ' apostrophe   | `'`         |
| 44.1  | SYMB           | —           |
| 45.1  | Backspace      | Backspace   |
| 51.1  | Y^x            | `^`         |
| 52.1  | √x             | —           |
| 53.1  | SIN            | —           |
| 54.1  | COS            | —           |
| 55.1  | TAN            | —           |
| 61.1  | EEX            | `e`         |
| 62.1  | +/-            | —           |
| 63.1  | X              | `x`         |
| 64.1  | 1/X            | —           |
| 65.1  | ÷              | `/`         |
| 72.1  | 7              | `7`         |
| 73.1  | 8              | `8`         |
| 74.1  | 9              | `9`         |
| 75.1  | ×              | `*`         |
| 82.1  | 4              | `4`         |
| 83.1  | 5              | `5`         |
| 84.1  | 6              | `6`         |
| 85.1  | −              | `-`         |
| 92.1  | 1              | `1`         |
| 93.1  | 2              | `2`         |
| 94.1  | 3              | `3`         |
| 95.1  | +              | `+`         |
| 102.1 | 0              | `0`         |
| 103.1 | .              | `.`         |
| 104.1 | SPC            | Space       |
| 105.1 | ENTER          | Enter       |
