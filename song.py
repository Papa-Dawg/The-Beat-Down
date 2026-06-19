#=====================================================================================================
# Title:       song.py
# Author:      nathan ramos
# Created:     5/15/2026
# Description: Python file for THE BEAT DOWN! (TM).
#=====================================================================================================
#=====================================================================================================
#                                            Libraries
#=====================================================================================================
import serial        # Handles serial connection communication with the Arduino Uno.
import pygame        # Utilized specifically to handle audio playback.
import threading     # Allows running asynchronous tasks (reading/writing data) at the same time.
import sys           # Provides access to system-specific variables.
import time          # Used for adding delays.
import os            # For getting the terminal display size, mainly.
import re            # Library used to strip out visual ANSI styling characters.
import msvcrt        # For 'Press any key' funciton.
#=====================================================================================================
#                                           Definitions
#=====================================================================================================
PORT     = 'COM5'                          # Port that controller is connected to.
BAUDRATE = 115200                          # Data transmission speed.
SONG     = 'Goodness Gracious.mp3'         # The song that I chose for the actual game.
INTRO    = 'intro.mp3'                     # Song played during menu screens.
# Colors:
RED     = "\033[91m"                       # Colors for display.
BLUE    = "\033[94m"
CYAN    = "\033[96m"
MAGENTA = "\033[95m"
YELLOW  = "\033[93m"
GREEN   = "\033[92m"
WHITE   = "\033[97m"
BOLD    = "\033[1m"
RESET   = "\033[0m"
#=====================================================================================================
#                                        Global Variables
#=====================================================================================================
recording         = False    # Tracks if raw timestamps are actively being saved to memory.
is_song_recording = False    # State flag indicating if a custom beatmap recording session is active.
beat_map          = []       # Array holding the recorded time-stamps and lanes.
running           = True     # Universal master switch logic loop control for background threads.
leaderboard_data  = []       # Array for high scores.
menu_buffer       = []       # Array used to catch strays from serial.
latest_score      = "0"      # Current score.
latest_combo      = "0"      # Current combo.
latest_multiplier = "1"      # Current multiplier.
current_track     = None     # Current track.
game_active       = False    # Tracks if actual game is running.
difficulty_text   = "MEDIUM" # For displaying difficulty level.
latest_grid       = None     # Current game board render.
latest_status     = ""       # For catching updates from controller.
#=====================================================================================================
#                                         Initializations
#=====================================================================================================
pygame.mixer.init()                                   # Initializes mixer for audio playback.
ser = serial.Serial(PORT, BAUDRATE, timeout=1)        # Initializes communication with the controller.
time.sleep(2)                                         # Delay for Arduino initialization.
#=====================================================================================================
#                                            Functions
#=====================================================================================================
#-----------------------------------------------------------------------------------------------------
# Shutdown
#-----------------------------------------------------------------------------------------------------
def shutdown():                       # Shuts everything down, gracefully.
    global running

    if not running:                   # Skips.
        return

    running = False

    pygame.mixer.music.stop()         # Stops the music.

    if ser.is_open:                   # Closes serial communication with the controller.
        ser.close()

    sys.exit()

#-----------------------------------------------------------------------------------------------------
# Song Chooser
#-----------------------------------------------------------------------------------------------------
def play_music(song, loop=True):      # Plays song in a loop, while tracking which song is playing.

    global current_track

    if current_track == song:         # Skips if current song is the same as intended song.
        return

    pygame.mixer.music.stop()         # Stops whatever might have been playing previously.
    pygame.mixer.music.load(song)     # Plays intended song.

    if loop:
        pygame.mixer.music.play(-1)   # Plays song in a loop.
    else:
        pygame.mixer.music.play()     # Plays song only once (for the game song).

    current_track = song              # Sets intended song to current song.

#-----------------------------------------------------------------------------------------------------
# Universal Framed Renderer
#-----------------------------------------------------------------------------------------------------
def render_in_box(lines_to_show):     # Calculates window size and renders graphics centered with borders.
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

#-----------------------------------------------------------------------------------------------------
# Beat Map Sender
#-----------------------------------------------------------------------------------------------------
def send_beatmap():                             # Sends beatmap to the controller.

    MAX_BEATS = 475                             # Max amount of tappable beats for a song.

    if not os.path.exists('beatmap.txt'):       # If no beatmap already:

        render_in_box([
            "No beatmap.txt found."
        ])

        ser.write(b'LOAD\n')
        ser.flush()
        return

    with open('beatmap.txt', 'r') as f:         # Opens and reads the beatmap data.
        all_lines = [line.strip() for line in f if line.strip()]

    beat_lines = []

    for line in all_lines:                      # Parses through each line to extract the data.

        if ',' not in line:
            continue

        parts = line.split(',')

        if len(parts) != 2:
            continue

        timestamp = parts[0].strip()
        lane      = parts[1].strip()

        if timestamp.isdigit() and lane.isdigit():
            beat_lines.append(f"{timestamp},{lane}")

    beat_lines = beat_lines[:MAX_BEATS]         # Packs extracted data into array, but only up to max beats.

    render_in_box([
        "Sending Beat Map...",
        f"{len(beat_lines)} beats loaded."
    ])

    for line in beat_lines:                     # Sends the data to the controller.
        ser.write((line + '\n').encode())
        time.sleep(0.001)                       # To prevent buffer overflow.

    ser.write(b'LOAD\n')                        # Sends instruction to load data.
    ser.flush()                                 # Flushes the channel.

#-----------------------------------------------------------------------------------------------------
# Leaderboard Renderer
#-----------------------------------------------------------------------------------------------------
def render_high_scores_ui():                    # Display function for showing the high scores.
    lines = []

    #-------------------------------------------------------------------------------------------------
    # Background Pattern
    #-------------------------------------------------------------------------------------------------

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    lines.append("")

    #-------------------------------------------------------------------------------------------------
    # Title
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Header Row
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Score Rows
    #-------------------------------------------------------------------------------------------------

    for i, row in enumerate(leaderboard_data):

        rank = row['rank']
        initials = row['initials']
        score = row['score']

        if i == 0:
            row_color = YELLOW + BOLD 
        elif i == 1:
            row_color = CYAN + BOLD 
        elif i == 2:
            row_color = MAGENTA + BOLD 
        else:
            row_color = WHITE

        lines.append(
            f"{row_color}"
            f"║  #{rank:<2} "
            f"║      {initials:^3}      "
            f"║     {int(score):>8} pts      ║"
            f"{RESET}"
        )

    #-------------------------------------------------------------------------------------------------
    # Footer
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Bottom Pattern
    #-------------------------------------------------------------------------------------------------

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

#-----------------------------------------------------------------------------------------------------
# Game Board Renderer
#-----------------------------------------------------------------------------------------------------
def draw_game_board(matrix_str, status_msg=""):     # Display function for the game board.

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

        #---------------------------------------------------------------------------------------------
        # Animated Background Pattern
        #---------------------------------------------------------------------------------------------

        for i in range(3):

            if i % 2 == 0:
                bg = ".     .     .     .     .     ."
            else:
                bg = "   .     .     .     .     ."

            board.append(f"{BLUE}{bg}{RESET}")

        board.append("")

        #---------------------------------------------------------------------------------------------
        # HUD BOX
        #---------------------------------------------------------------------------------------------

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

        #---------------------------------------------------------------------------------------------
        # Header
        #---------------------------------------------------------------------------------------------
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

        #---------------------------------------------------------------------------------------------
        # Note Grid
        #---------------------------------------------------------------------------------------------

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

        #---------------------------------------------------------------------------------------------
        # Tap Zone
        #---------------------------------------------------------------------------------------------
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

        #---------------------------------------------------------------------------------------------
        # Status
        #---------------------------------------------------------------------------------------------

        if latest_status:

            board.append("")
            board.append(
                f"{YELLOW}{BOLD}STATUS:{RESET} "
                f"{WHITE}{latest_status}{RESET}"
            )

        board.append("")

        #---------------------------------------------------------------------------------------------
        # Bottom Background
        #---------------------------------------------------------------------------------------------

        for i in range(3):

            if i % 2 == 0:
                bg = ".     .     .     .     .     ."
            else:
                bg = "   .     .     .     .     ."

            board.append(f"{BLUE}{bg}{RESET}")

        render_in_box(board)

    except Exception:
        pass

#-----------------------------------------------------------------------------------------------------
# Start Screen Renderer
#-----------------------------------------------------------------------------------------------------
def render_start_screen():

    logo = [
        f"{CYAN}{BOLD}████████╗██╗  ██╗███████╗    ██████╗ ███████╗ █████╗ ████████╗   ██████╗  ██████╗ ██╗    ██╗███╗   ██╗{RESET}",
        f"{CYAN}{BOLD}╚══██╔══╝██║  ██║██╔════╝    ██╔══██╗██╔════╝██╔══██╗╚══██╔══╝   ██╔══██╗██╔═══██╗██║    ██║████╗  ██║{RESET}",
        f"{BLUE}{BOLD}   ██║   ███████║█████╗      ██████╔╝█████╗  ███████║   ██║      ██║  ██║██║   ██║██║ █╗ ██║██╔██╗ ██║{RESET}",
        f"{BLUE}{BOLD}   ██║   ██╔══██║██╔══╝      ██╔══██╗██╔══╝  ██╔══██║   ██║      ██║  ██║██║   ██║██║███╗██║██║╚██╗██║{RESET}",
        f"{MAGENTA}{BOLD}   ██║   ██║  ██║███████╗    ██████╔╝███████╗██║  ██║   ██║      ██████╔╝╚██████╔╝╚███╔███╔╝██║ ╚████║{RESET}",
        f"{MAGENTA}{BOLD}   ╚═╝   ╚═╝  ╚═╝╚══════╝    ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝      ╚═════╝  ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═══╝{RESET}",
    ]

    for i in range(len(logo)):
        render_in_box(logo[:i+1])
        time.sleep(0.08)

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

#-----------------------------------------------------------------------------------------------------
# Main Menu Renderer
#-----------------------------------------------------------------------------------------------------
def render_main_menu():

    global difficulty_text

    lines = []

    #-------------------------------------------------------------------------------------------------
    # Diamond Dot Background
    #-------------------------------------------------------------------------------------------------

    for i in range(8):

        if i % 2 == 0:
            bg = ".     .     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    #-------------------------------------------------------------------------------------------------
    # Main Menu Box
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Bottom Background
    #-------------------------------------------------------------------------------------------------
    for i in range(8):

        if i % 2 == 0:
            bg = ".     .     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

#-----------------------------------------------------------------------------------------------------
# Sensor Data Renderer
#-----------------------------------------------------------------------------------------------------
def render_sensor_debug_ui(x, pot, light):

    lines = []

    #-------------------------------------------------------------------------------------------------
    # Background Pattern
    #-------------------------------------------------------------------------------------------------

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    lines.append("")

    #-------------------------------------------------------------------------------------------------
    # Title Box
    #-------------------------------------------------------------------------------------------------
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

    #-------------------------------------------------------------------------------------------------
    # Sensor Values
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Footer
    #-------------------------------------------------------------------------------------------------

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

    #-------------------------------------------------------------------------------------------------
    # Bottom Pattern
    #-------------------------------------------------------------------------------------------------

    for i in range(4):

        if i % 2 == 0:
            bg = ".     .     .     .     .     ."
        else:
            bg = "   .     .     .     .     ."

        lines.append(f"{BLUE}{bg}{RESET}")

    render_in_box(lines)

#-----------------------------------------------------------------------------------------------------
# Arduino Reader Thread
#-----------------------------------------------------------------------------------------------------
def read_from_arduino():            # Updates the game based on input from the controller.

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

                #-------------------------------------------------------------------------------------
                # Grid
                #-------------------------------------------------------------------------------------
                if line.startswith("GRID:"):

                    matrix_data = line.replace("GRID:", "")

                    draw_game_board(
                        matrix_data,
                        latest_status
                    )

                    continue

                #-------------------------------------------------------------------------------------
                # Score Box
                #-------------------------------------------------------------------------------------
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

                #-------------------------------------------------------------------------------------
                # Status
                #-------------------------------------------------------------------------------------
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
                
                #-------------------------------------------------------------------------------------
                # Main Menu
                #-------------------------------------------------------------------------------------
                elif line == "SHOW_MENU":

                    render_main_menu()

                    continue

                elif line.startswith("DIFFICULTY:"):

                    difficulty_text = line.replace("DIFFICULTY:", "").strip()

                    render_main_menu()

                    continue

                #-------------------------------------------------------------------------------------
                # Leaderboard
                #-------------------------------------------------------------------------------------
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

                #-------------------------------------------------------------------------------------
                # Sensor Data
                #-------------------------------------------------------------------------------------
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

                #-------------------------------------------------------------------------------------
                # Game Over
                #-------------------------------------------------------------------------------------
                elif line.startswith("GAME_OVER:"):

                    game_active = False
                    play_music(INTRO)

                    _, score, is_high_score = line.split(":")

                    lines = []

                    #---------------------------------------------------------------------------------
                    # Background Pattern
                    #---------------------------------------------------------------------------------
                    for i in range(4):

                        if i % 2 == 0:
                            bg = ".     .     .     .     .     ."
                        else:
                            bg = "   .     .     .     .     ."

                        lines.append(f"{BLUE}{bg}{RESET}")

                    lines.append("")

                    #---------------------------------------------------------------------------------
                    # Title Box
                    #---------------------------------------------------------------------------------

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

                    #---------------------------------------------------------------------------------
                    # Score Display
                    #---------------------------------------------------------------------------------

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

                    #---------------------------------------------------------------------------------
                    # High Score Celebration
                    #---------------------------------------------------------------------------------
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

                    #---------------------------------------------------------------------------------
                    # Footer
                    #---------------------------------------------------------------------------------

                    lines.append(
                        f"{MAGENTA}{BOLD}"
                        "╚══════════════════════════════════════════════╝"
                        f"{RESET}"
                    )

                    lines.append("")

                    #---------------------------------------------------------------------------------
                    # Bottom Background
                    #---------------------------------------------------------------------------------

                    for i in range(4):

                        if i % 2 == 0:
                            bg = ".     .     .     .     .     ."
                        else:
                            bg = "   .     .     .     .     ."

                        lines.append(f"{BLUE}{bg}{RESET}")

                    render_in_box(lines)

                    continue

                #-------------------------------------------------------------------------------------
                # Start Game
                #-------------------------------------------------------------------------------------
                elif line == "START":

                    latest_status = ""

                    game_active = True

                    play_music(SONG, loop=False)

                    continue

                #-------------------------------------------------------------------------------------
                # Load Beatmap
                #-------------------------------------------------------------------------------------
                elif line == "READY_FOR_BEATMAP" and not game_active:

                    send_beatmap()
                    ser.reset_input_buffer()
                    continue

                #-------------------------------------------------------------------------------------
                # Recording
                #-------------------------------------------------------------------------------------
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

                #-------------------------------------------------------------------------------------
                # Pause / Resume
                #-------------------------------------------------------------------------------------
                elif line == "PAUSE":

                    pygame.mixer.music.pause()
                    continue

                elif line == "RESUME":

                    pygame.mixer.music.unpause()
                    continue

                #-------------------------------------------------------------------------------------
                # Exit
                #-------------------------------------------------------------------------------------
                elif "Goodbye" in line:

                    shutdown()

                #-------------------------------------------------------------------------------------
                # Everything Else
                #-------------------------------------------------------------------------------------
                else:

                    menu_buffer.append(line)

                    if "Enter your choice" in line:
                        render_in_box(menu_buffer)
                        menu_buffer.clear()

        except (serial.SerialException, OSError):
            break

#-----------------------------------------------------------------------------------------------------
# Arduino Writer Thread
#-----------------------------------------------------------------------------------------------------
def write_to_arduino():        # Sends input from keyboard input to arduino.

    while running:

        try:

            user_input = input()

            if not running:
                break

            ser.write((user_input + '\n').encode())

        except EOFError:
            break

#-----------------------------------------------------------------------------------------------------
# Threads
#-----------------------------------------------------------------------------------------------------

# These threads are basically background functions for communicating with the controller while the main loop is running.

read_thread = threading.Thread(  # For receiving data from the controller.
    target=read_from_arduino,
    daemon=True                  # Sets as a background process that will terminate when program ends.
)

write_thread = threading.Thread( # For sending data to the controller.
    target=write_to_arduino,
    daemon=True                  # Sets as a background process that will terminate when program ends.
)


play_music(INTRO)                # Plays Intro.mp3 on startup.
render_start_screen()            # Renders the start screen.

read_thread.start()              # Starts the reading background process.
write_thread.start()             # Starts the writing background process.

#-----------------------------------------------------------------------------------------------------
# Main Loop
#-----------------------------------------------------------------------------------------------------
try:

    while running:               # The main loop keeping the program running.

        #---------------------------------------------------------------------------------------------
        # Recording mode auto-stop
        #---------------------------------------------------------------------------------------------

        if is_song_recording:    # If a beatmap is being recorded:

            if not pygame.mixer.music.get_busy():   # Checks is song has ended.

                ser.write(b's\n')                   # Tells controller recording has stopped. 
                is_song_recording = False           # Sets recording flag to false.

        time.sleep(0.1)          # Safety delay.

except KeyboardInterrupt:        # Ctrl + C

    shutdown()                   # Shuts it all down gracefully.
#=====================================================================================================
#                            End of File
#=====================================================================================================