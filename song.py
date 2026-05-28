import serial
import pygame
import threading
import sys
import time
import os
import re
import msvcrt

PORT     = 'COM5'
BAUDRATE = 115200
SONG     = 'Goodness Gracious.mp3'
INTRO    = 'intro.mp3'

pygame.mixer.init()
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

time.sleep(2)

# =====================================================================
# Global State
# =====================================================================
recording         = False
is_song_recording = False
beat_map          = []
running           = True
leaderboard_data  = []
menu_buffer       = []
latest_score      = "0"
latest_combo      = "0"
latest_multiplier = "1"
current_track     = None
game_active       = False
difficulty_text   = "MEDIUM"

# Colors:
RED     = "\033[91m"
BLUE    = "\033[94m"
CYAN    = "\033[96m"
MAGENTA = "\033[95m"
YELLOW  = "\033[93m"
GREEN   = "\033[92m"
WHITE   = "\033[97m"

BOLD    = "\033[1m"
RESET   = "\033[0m"

# Persistent game display state
latest_grid       = None
latest_status     = ""

# =====================================================================
# Shutdown
# =====================================================================
def shutdown():
    global running

    if not running:
        return

    running = False

    pygame.mixer.music.stop()

    if ser.is_open:
        ser.close()

    sys.exit()

# =====================================================================
# Song Chooser
# =====================================================================
def play_music(song, loop=True):

    global current_track

    if current_track == song:
        return

    pygame.mixer.music.stop()
    pygame.mixer.music.load(song)

    if loop:
        pygame.mixer.music.play(-1)
    else:
        pygame.mixer.music.play()

    current_track = song

# =====================================================================
# Universal Framed Renderer
# =====================================================================
def render_in_box(lines_to_show):
    try:
        columns, rows = os.get_terminal_size()
    except OSError:
        columns, rows = 120, 40

    usable_rows = rows - 2
    usable_cols = columns - 2

    trimmed_lines = []

    for line in lines_to_show:
        trimmed_lines.append(line[:usable_cols])

    top_padding = max(0, (usable_rows - len(trimmed_lines)) // 2)
    bottom_padding = max(0, usable_rows - len(trimmed_lines) - top_padding)

    final_output = []

    final_output.append("╔" + ("═" * usable_cols) + "╗")

    for _ in range(top_padding):
        final_output.append("║" + (" " * usable_cols) + "║")

    for line in trimmed_lines:

        ansi_escape = re.compile(r'\x1B\[[0-?]*[ -/]*[@-~]')

        visible_length = len(ansi_escape.sub('', line))

        left_pad = max(0, (usable_cols - visible_length) // 2)
        right_pad = max(0, usable_cols - visible_length - left_pad)

        final_output.append(
            "║" +
            (" " * left_pad) +
            line +
            (" " * right_pad) +
            "║"
        )

    for _ in range(bottom_padding):
        final_output.append("║" + (" " * usable_cols) + "║")

    final_output.append("╚" + ("═" * usable_cols) + "╝")

    print("\033[2J\033[H" + "\n".join(final_output), end="")

# =====================================================================
# Beat Map Sender
# =====================================================================
def send_beatmap():

    MAX_BEATS = 475

    if not os.path.exists('beatmap.txt'):

        render_in_box([
            "No beatmap.txt found."
        ])

        ser.write(b'LOAD\n')
        ser.flush()
        return

    with open('beatmap.txt', 'r') as f:
        all_lines = [line.strip() for line in f if line.strip()]

    beat_lines = []

    for line in all_lines:

        if ',' not in line:
            continue

        parts = line.split(',')

        if len(parts) != 2:
            continue

        timestamp = parts[0].strip()
        lane      = parts[1].strip()

        if timestamp.isdigit() and lane.isdigit():
            beat_lines.append(f"{timestamp},{lane}")

    beat_lines = beat_lines[:MAX_BEATS]

    render_in_box([
        "Sending Beat Map...",
        f"{len(beat_lines)} beats loaded."
    ])

    for line in beat_lines:
        ser.write((line + '\n').encode())
        time.sleep(0.001)

    ser.write(b'LOAD\n')
    ser.flush()

# =====================================================================
# Leaderboard Renderer
# =====================================================================
def render_high_scores_ui():

    lines = []

    # =====================================================
    # Background Pattern
    # =====================================================

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    lines.append("")

    # =====================================================
    # Title
    # =====================================================

    lines.append(
        f"{MAGENTA}{BOLD}"
        "╔══════════════════════════════════════════════╗"
        f"{RESET}"
    )

    lines.append(
        f"{MAGENTA}{BOLD}"
        "║                 HIGH SCORES                  ║"
        f"{RESET}"
    )

    lines.append(
        f"{MAGENTA}{BOLD}"
        "╠══════╦═══════════════╦═══════════════════════╣"
        f"{RESET}"
    )

    # =====================================================
    # Header Row
    # =====================================================

    lines.append(
        f"{MAGENTA}{BOLD}"
        "║ RANK ║    PLAYER     ║         SCORE         ║"
        f"{RESET}"
    )

    lines.append(
        f"{MAGENTA}"
        "╠══════╬═══════════════╬═══════════════════════╣"
        f"{RESET}"
    )

    # =====================================================
    # Score Rows
    # =====================================================

    for i, row in enumerate(leaderboard_data):

        rank = row['rank']
        initials = row['initials']
        score = row['score']

        # Top 3 colors
        if i == 0:
            row_color = YELLOW + BOLD      # Gold
        elif i == 1:
            row_color = CYAN + BOLD        # Silver-ish
        elif i == 2:
            row_color = MAGENTA + BOLD     # Bronze-ish / neon
        else:
            row_color = WHITE

        lines.append(
            f"{row_color}"
            f"║  #{rank:<2} "
            f"║      {initials:^3}      "
            f"║     {int(score):>8} pts      ║"
            f"{RESET}"
        )

    # =====================================================
    # Footer
    # =====================================================

    lines.append(
        f"{WHITE}"
        "╚══════╩═══════════════╩═══════════════════════╝"
        f"{RESET}"
    )

    lines.append("")

    lines.append(
        f"{YELLOW}{BOLD}"
        "PRESS [2] ANYTIME TO VIEW AGAIN"
        f"{RESET}"
    )

    lines.append("")

    # =====================================================
    # Bottom Pattern
    # =====================================================

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

# =====================================================================
# Game Board Renderer
# =====================================================================
def draw_game_board(matrix_str, status_msg=""):

    global latest_grid
    global latest_status

    latest_grid = matrix_str

    if status_msg:
        latest_status = status_msg

    try:

        rows_data = [int(x) for x in matrix_str.split(',')]

        if len(rows_data) != 8:
            return

        board = []

        # =====================================================
        # Animated Background Pattern
        # =====================================================

        for i in range(3):

            if i % 2 == 0:
                bg = ".     .     .     .     .     ."
            else:
                bg = "   .     .     .     .     ."

            board.append(f"{BLUE}{bg}{RESET}")

        board.append("")

        # =====================================================
        # HUD BOX
        # =====================================================

        board.append(
            f"{MAGENTA}{BOLD}"
            "╔══════════════════════════════════════╗"
            f"{RESET}"
        )

        board.append(
            f"{MAGENTA}║{RESET} "
            f"{YELLOW}{BOLD}SCORE:{RESET} {WHITE}{latest_score:<8}"
            f"{CYAN}{BOLD}COMBO:{RESET} x{latest_combo:<3}"
            f"{GREEN}{BOLD}MULT:{RESET} x{latest_multiplier:<3}"
            f"{' ' * 1}"
            f"{MAGENTA}║{RESET}"
        )

        board.append(
            f"{MAGENTA}{BOLD}"
            "╚══════════════════════════════════════╝"
            f"{RESET}"
        )

        board.append("")

        # =====================================================
        # Header
        # =====================================================

        board.append(
            f"{MAGENTA}╔════════════╦════════════╦════════════╗{RESET}"
        )

        board.append(
            f"{MAGENTA}║{CYAN}{BOLD}    LEFT    "
            f"{MAGENTA}║{CYAN}{BOLD}   MIDDLE   "
            f"{MAGENTA}║{CYAN}{BOLD}   RIGHT    "
            f"{MAGENTA}║{RESET}"
        )

        board.append(
            f"{MAGENTA}╠════════════╬════════════╬════════════╣{RESET}"
        )

        # =====================================================
        # Note Grid
        # =====================================================

        for row_bits in rows_data:

            left  = f"{GREEN}⬤{RESET}" if (row_bits & 0x04) else " "
            mid   = f"{YELLOW}⬤{RESET}" if (row_bits & 0x02) else " "
            right = f"{CYAN}⬤{RESET}" if (row_bits & 0x01) else " "

            board.append(
                f"{MAGENTA}║{RESET}            "
                f"{MAGENTA}║{RESET}            "
                f"{MAGENTA}║{RESET}            "
                f"{MAGENTA}║{RESET}"
            )

            board.append(
                f"{MAGENTA}║{RESET}     {left}      "
                f"{MAGENTA}║{RESET}     {mid}      "
                f"{MAGENTA}║{RESET}     {right}      "
                f"{MAGENTA}║{RESET}"
            )

        # =====================================================
        # Tap Zone
        # =====================================================

        board.append(
            f"{MAGENTA}╠════════════╩════════════╩════════════╣{RESET}"
        )

        board.append(
            f"{MAGENTA}║{RED}{BOLD}       TAP WHEN NOTES REACH HERE      "
            f"{MAGENTA}║{RESET}"
        )

        board.append(
            f"{MAGENTA}╚══════════════════════════════════════╝{RESET}"
        )

        # =====================================================
        # Status
        # =====================================================

        if latest_status:

            board.append("")
            board.append(
                f"{YELLOW}{BOLD}STATUS:{RESET} "
                f"{WHITE}{latest_status}{RESET}"
            )

        board.append("")

        # =====================================================
        # Bottom Background
        # =====================================================

        for i in range(3):

            if i % 2 == 0:
                bg = ".     .     .     .     .     ."
            else:
                bg = "   .     .     .     .     ."

            board.append(f"{BLUE}{bg}{RESET}")

        render_in_box(board)

    except Exception:
        pass

def render_start_screen():

    logo = [
        f"{CYAN}{BOLD}████████╗██╗  ██╗███████╗    ██████╗ ███████╗ █████╗ ████████╗   ██████╗  ██████╗ ██╗    ██╗███╗   ██╗{RESET}",
        f"{CYAN}{BOLD}╚══██╔══╝██║  ██║██╔════╝    ██╔══██╗██╔════╝██╔══██╗╚══██╔══╝   ██╔══██╗██╔═══██╗██║    ██║████╗  ██║{RESET}",
        f"{BLUE}{BOLD}   ██║   ███████║█████╗      ██████╔╝█████╗  ███████║   ██║      ██║  ██║██║   ██║██║ █╗ ██║██╔██╗ ██║{RESET}",
        f"{BLUE}{BOLD}   ██║   ██╔══██║██╔══╝      ██╔══██╗██╔══╝  ██╔══██║   ██║      ██║  ██║██║   ██║██║███╗██║██║╚██╗██║{RESET}",
        f"{MAGENTA}{BOLD}   ██║   ██║  ██║███████╗    ██████╔╝███████╗██║  ██║   ██║      ██████╔╝╚██████╔╝╚███╔███╔╝██║ ╚████║{RESET}",
        f"{MAGENTA}{BOLD}   ╚═╝   ╚═╝  ╚═╝╚══════╝    ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝      ╚═════╝  ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═══╝{RESET}",
    ]

    # Animate line-by-line reveal
    for i in range(len(logo)):
        render_in_box(logo[:i+1])
        time.sleep(0.08)

    # Flashing prompt loop
    while True:

        render_in_box(
            logo +
            [
                "",
                f"{GREEN}{BOLD}Created by Papa Dawg{RESET}",
                "",
                f"{YELLOW}{BOLD}PRESS ANY KEY TO START{RESET}"
            ]
        )

        time.sleep(0.5)

        render_in_box(
            logo +
            [
                "",
                f"{GREEN}Created by Papa Dawg{RESET}",
                "",
                ""
            ]
        )

        time.sleep(0.5)

        if msvcrt.kbhit():
            msvcrt.getch()
            break

def render_main_menu():

    global difficulty_text

    lines = []

    # =========================================================
    # Diamond Dot Background
    # =========================================================

    for i in range(8):

        if i % 2 == 0:
            bg = ".     .     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    # =========================================================
    # Main Menu Box
    # =========================================================

    lines.extend([

        "",

        f"{MAGENTA}{BOLD}╔══════════════════════════════════════╗{RESET}",
        f"{MAGENTA}{BOLD}║             MAIN MENU                ║{RESET}",
        f"{MAGENTA}{BOLD}╠══════════════════════════════════════╣{RESET}",

        f"{BLUE}{BOLD}║   [1]  Start Game                    ║{RESET}",
        f"{CYAN}{BOLD}║   [2]  View High Scores              ║{RESET}",
        f"{GREEN}{BOLD}║   [3]  Record Custom Beatmap         ║{RESET}",
        f"{YELLOW}{BOLD}║   [4]  Check Sensor Data             ║{RESET}",
        f"{RED}{BOLD}║   [5]  Exit                          ║{RESET}",

        f"{MAGENTA}{BOLD}╚══════════════════════════════════════╝{RESET}",

        "",

        f"{CYAN}╔══════════════════════════════════════╗{RESET}",
        f"{CYAN}║          DIFFICULTY LEVEL            ║{RESET}",
        f"{CYAN}╠══════════════════════════════════════╣{RESET}",
        f"{YELLOW}║              {difficulty_text:^10}              ║{RESET}",
        f"{CYAN}╚══════════════════════════════════════╝{RESET}",

        ""
    ])

    # =========================================================
    # Bottom Background
    # =========================================================

    for i in range(8):

        if i % 2 == 0:
            bg = ".     .     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

def render_sensor_debug_ui(x, pot, light):

    lines = []

    # =====================================================
    # Background Pattern
    # =====================================================

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    lines.append("")

    # =====================================================
    # Title Box
    # =====================================================

    lines.append(
        f"{CYAN}{BOLD}"
        "╔══════════════════════════════════════╗"
        f"{RESET}"
    )

    lines.append(
        f"{CYAN}{BOLD}"
        "║             SENSOR DEBUG             ║"
        f"{RESET}"
    )

    lines.append(
        f"{CYAN}"
        "╠══════════════════════════════════════╣"
        f"{RESET}"
    )

    # =====================================================
    # Sensor Values
    # =====================================================

    lines.append(
        f"{YELLOW}║    Tilt X Value:   {WHITE}{x:<16}  "
        f"{YELLOW}║{RESET}"
    )

    lines.append(
        f"{GREEN}║    Potentiometer:  {WHITE}{pot:<16}  "
        f"{GREEN}║{RESET}"
    )

    lines.append(
        f"{MAGENTA}║    Light Sensor:   {WHITE}{light:<16}  "
        f"{MAGENTA}║{RESET}"
    )

    # =====================================================
    # Footer
    # =====================================================

    lines.append(
        f"{RED}"
        "╚══════════════════════════════════════╝"
        f"{RESET}"
    )

    lines.append("")

    lines.append(
        f"{WHITE}Press any key to return to the main menu.{RESET}"
    )

    lines.append("")

    # =====================================================
    # Bottom Pattern
    # =====================================================

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

# =====================================================================
# Arduino Reader Thread
# =====================================================================
def read_from_arduino():

    global recording
    global beat_map
    global running
    global is_song_recording
    global leaderboard_data
    global latest_status
    global game_active
    global difficulty_text

    global latest_score
    global latest_combo
    global latest_multiplier

    while running:

        try:

            if ser.in_waiting > 0:

                line = ser.readline().decode(
                    'utf-8',
                    errors='replace'
                ).strip()

                if not line:
                    continue

                # =====================================================
                # GRID
                # =====================================================
                if line.startswith("GRID:"):

                    matrix_data = line.replace("GRID:", "")

                    draw_game_board(
                        matrix_data,
                        latest_status
                    )

                    continue

                # =====================================================
                # SCORE BOX
                # =====================================================
                elif line.startswith("HUD:"):

                    try:
                        _, data = line.split(":", 1)

                        parts = data.strip().split(",")

                        if len(parts) != 3:
                            continue

                        score, combo, mult = parts

                    except ValueError:
                        continue

                    latest_score      = score
                    latest_combo      = combo
                    latest_multiplier = mult

                    if latest_grid:
                        draw_game_board(latest_grid, latest_status)

                    continue

                # =====================================================
                # STATUS
                # =====================================================
                elif line.startswith("STATUS:"):

                    latest_status = line.replace("STATUS:", "").strip()

                    if latest_grid:
                        draw_game_board(
                            latest_grid,
                            latest_status
                        )

                    continue

                elif line == "PRESS ENTER":
                    render_in_box([
                        "Press ENTER to continue..."
                    ])
                    continue
                
                # =====================================================
                # Main Menu
                # =====================================================
                elif line == "SHOW_MENU":

                    render_main_menu()

                    continue

                elif line.startswith("DIFFICULTY:"):

                    difficulty_text = line.replace("DIFFICULTY:", "").strip()

                    render_main_menu()

                    continue

                # =====================================================
                # LEADERBOARD
                # =====================================================
                elif line == "LEADERBOARD_START":

                    leaderboard_data = []
                    continue

                elif line.startswith("SCORE_ROW:"):

                    parts = line.split(":")

                    if len(parts) == 4:

                        leaderboard_data.append({
                            "rank": parts[1],
                            "initials": parts[2],
                            "score": parts[3]
                        })

                    continue

                elif line == "LEADERBOARD_END":

                    render_high_scores_ui()
                    continue

                # =====================================================
                # SENSOR DATA
                # =====================================================

                elif line == "SENSOR_DEBUG_START":

                    render_sensor_debug_ui("0", "0", "0")

                    continue

                elif line.startswith("SENSOR:"):

                    _, data = line.split(":", 1)

                    x, pot, light = data.split(",")

                    render_sensor_debug_ui(x, pot, light)

                    continue

                elif line == "SENSOR_DEBUG_END":

                    render_main_menu()

                    continue

                # =====================================================
                # GAME OVER
                # =====================================================
                elif line.startswith("GAME_OVER:"):

                    game_active = False
                    play_music(INTRO)

                    _, score, is_high_score = line.split(":")

                    lines = []

                    # =================================================
                    # Background Pattern
                    # =================================================

                    for i in range(4):

                        if i % 2 == 0:
                            bg = ".     .     .     .     .     ."
                        else:
                            bg = "   .     .     .     .     ."

                        lines.append(f"{BLUE}{bg}{RESET}")

                    lines.append("")

                    # =================================================
                    # Title Box
                    # =================================================

                    lines.append(
                        f"{RED}{BOLD}"
                        "╔══════════════════════════════════════════════╗"
                        f"{RESET}"
                    )

                    lines.append(
                        f"{RED}{BOLD}"
                        "║                  GAME OVER                   ║"
                        f"{RESET}"
                    )

                    lines.append(
                        f"{RED}{BOLD}"
                        "╠══════════════════════════════════════════════╣"
                        f"{RESET}"
                    )

                    # =================================================
                    # Score Display
                    # =================================================

                    lines.append(
                        f"{YELLOW}{BOLD}"
                        f"║                 FINAL SCORE:                 ║"
                        f"{RESET}"
                    )

                    lines.append(
                        f"{GREEN}{BOLD}"
                        f"║                 {score:^6} pts                   ║"
                        f"{RESET}"
                    )

                    lines.append(
                        f"{CYAN}{BOLD}"
                        f"║                                              ║"
                        f"{RESET}"
                    )

                    # =================================================
                    # High Score Celebration
                    # =================================================

                    if is_high_score == "1":

                        lines.append(
                            f"{BLUE}{BOLD}"
                            "║              ★ NEW HIGH SCORE! ★             ║"
                            f"{RESET}"
                        )

                        lines.append(
                            f"{MAGENTA}"
                            "║          Enter your initials below           ║"
                            f"{RESET}"
                        )

                    else:

                        lines.append(
                            f"{BLUE}"
                            "║            Nice run! Try again!              ║"
                            f"{RESET}"
                        )

                    # =================================================
                    # Footer
                    # =================================================

                    lines.append(
                        f"{MAGENTA}{BOLD}"
                        "╚══════════════════════════════════════════════╝"
                        f"{RESET}"
                    )

                    lines.append("")

                    # =================================================
                    # Bottom Background
                    # =================================================

                    for i in range(4):

                        if i % 2 == 0:
                            bg = ".     .     .     .     .     ."
                        else:
                            bg = "   .     .     .     .     ."

                        lines.append(f"{BLUE}{bg}{RESET}")

                    render_in_box(lines)

                    continue

                # =====================================================
                # START GAME
                # =====================================================
                elif line == "START":

                    latest_status = ""

                    game_active = True

                    play_music(SONG, loop=False)

                    continue

                # =====================================================
                # LOAD BEATMAP
                # =====================================================
                elif line == "READY_FOR_BEATMAP" and not game_active:

                    send_beatmap()
                    ser.reset_input_buffer()
                    continue

                # =====================================================
                # RECORDING
                # =====================================================
                elif line == "Recording! Tap along to the song.":

                    is_song_recording = True

                    render_in_box([
                        "Recording Mode Active",
                        "Tap Along To The Song"
                    ])

                    continue

                elif line == "BEATMAP_START":

                    recording = True
                    beat_map = []

                    continue

                elif line == "BEATMAP_END":

                    recording = False
                    is_song_recording = False

                    with open('beatmap.txt', 'w') as f:

                        for entry in beat_map:
                            f.write(entry + '\n')

                    render_in_box([
                        "Beat Map Saved!",
                        f"{len(beat_map)} beats recorded."
                    ])

                    continue

                elif recording:

                    beat_map.append(line)
                    continue

                # =====================================================
                # PAUSE / RESUME
                # =====================================================
                elif line == "PAUSE":

                    pygame.mixer.music.pause()
                    continue

                elif line == "RESUME":

                    pygame.mixer.music.unpause()
                    continue

                # =====================================================
                # EXIT
                # =====================================================
                elif "Goodbye" in line:

                    shutdown()

                # =====================================================
                # EVERYTHING ELSE
                # =====================================================
                else:

                    # Accumulate menu / informational text
                    menu_buffer.append(line)

                    # Detect end of menu prompt
                    if "Enter your choice" in line:
                        render_in_box(menu_buffer)
                        menu_buffer.clear()

        except (serial.SerialException, OSError):
            break

# =====================================================================
# Arduino Writer Thread
# =====================================================================
def write_to_arduino():

    while running:

        try:

            user_input = input()

            if not running:
                break

            ser.write((user_input + '\n').encode())

        except EOFError:
            break

# =====================================================================
# Threads
# =====================================================================
read_thread = threading.Thread(
    target=read_from_arduino,
    daemon=True
)

write_thread = threading.Thread(
    target=write_to_arduino,
    daemon=True
)


play_music(INTRO)
render_start_screen()

read_thread.start()
write_thread.start()

# =====================================================================
# Main Loop
# =====================================================================
try:

    while running:

        # =====================================================
        # Recording mode auto-stop
        # =====================================================

        if is_song_recording:

            if not pygame.mixer.music.get_busy():

                ser.write(b's\n')
                is_song_recording = False

        time.sleep(0.1)

except KeyboardInterrupt:

    shutdown()