import serial
import pygame
import threading
import sys
import time
import os

PORT     = 'COM5'
BAUDRATE = 115200
SONG     = 'helden.mp3'

pygame.mixer.init()
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# Give the Arduino time to reset after serial connection opens
time.sleep(2)

print("Connected.")

# =====================================================================
# Global States
# =====================================================================
recording          = False
is_song_recording  = False
beat_map           = []
running            = True

# =====================================================================
# Shutdown
# =====================================================================
def shutdown():
    global running

    if not running:
        return

    running = False

    print("\nExiting...")

    pygame.mixer.music.stop()

    if ser.is_open:
        ser.close()

    sys.exit()

# =====================================================================
# Beat Map Sender
# =====================================================================
def send_beatmap():

    MAX_BEATS = 575

    if not os.path.exists('beatmap.txt'):
        print("No beatmap.txt found. Sending empty map.")
        ser.write(b'LOAD\n')
        ser.flush()
        return

    # -------------------------------------------------------------
    # Load beat map lines
    # -------------------------------------------------------------
    with open('beatmap.txt', 'r') as f:
        all_lines = [line.strip() for line in f if line.strip()]

    # -------------------------------------------------------------
    # Filter valid lines
    # Format: timestamp,lane
    # Example: 1532,1
    # -------------------------------------------------------------
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

    # -------------------------------------------------------------
    # Prevent AVR overflow
    # -------------------------------------------------------------
    beat_lines = beat_lines[:MAX_BEATS]

    if not beat_lines:
        print("No valid beats found. Sending empty map.")
        ser.write(b'LOAD\n')
        ser.flush()
        return

    print(f"Sending beat map ({len(beat_lines)} beats)...")

    # Clear pending TX buffer
    ser.reset_output_buffer()

    # -------------------------------------------------------------
    # Send beat lines slowly enough for AVR UART parser
    # -------------------------------------------------------------
    for line in beat_lines:

        ser.write((line + '\n').encode())

        # Small delay prevents UART overruns on AVR
        time.sleep(0.001)

    # -------------------------------------------------------------
    # Send termination token
    # -------------------------------------------------------------
    ser.write(b'LOAD\n')

    ser.flush()

    # Give AVR time to finish parsing
    time.sleep(0.5)

    print("Beat map sent successfully!")

# =====================================================================
# Arduino Reader Thread
# =====================================================================
def draw_game_board(matrix_str):
    try:
        rows = [int(x) for x in matrix_str.split(',')]
        if len(rows) != 8:
            return
            
        # Clear screen and move to home
        print("\033[2J\033[H", end="")
        
        # Increase the width by adding spaces
        # Increase the height by adding extra newlines or spacers
        output = []
        output.append("+-----------+-----------+-----------+")
        output.append("|    LEFT   |    MID    |   RIGHT   |")
        output.append("+-----------+-----------+-----------+")
        
        for row_bits in rows:
            left  = 'O' if (row_bits & 0x04) else ' '
            mid   = 'O' if (row_bits & 0x02) else ' '
            right = 'O' if (row_bits & 0x01) else ' '
            
            # Add extra vertical height with an empty spacer row
            output.append("|           |           |           |")
            output.append(f"|     {left}     |     {mid}     |     {right}     |")
            output.append("|           |           |           |")
            output.append("+-----------+-----------+-----------+")
            
        output.append("|   [TAP]   |   [TAP]   |   [TAP]   |")
        output.append("+-----------+-----------+-----------+")
        
        print("\n".join(output))
    except Exception:
        pass

def read_from_arduino():
    global recording, beat_map, running, is_song_recording

    while running:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if not line:
                    continue

                # --- NEW RENDER HOOK ---
                if not hasattr(read_from_arduino, "waiting_for_grid"):
                    read_from_arduino.waiting_for_grid = False

                if "GRID:" in line:
                    read_from_arduino.waiting_for_grid = True
                    continue # Skip to next line

                if read_from_arduino.waiting_for_grid:
                    draw_game_board(line)
                    read_from_arduino.waiting_for_grid = False
                    continue # Stops the data from printing to the console

                # --- EXISTING LOGIC ---
                if "Goodbye" in line:
                    shutdown()
                elif line == "READY_FOR_BEATMAP":
                    print("Arduino ready — sending beat map...")
                    send_beatmap()
                elif line == "START":
                    print("Starting song...")
                    pygame.mixer.music.load(SONG)
                    pygame.mixer.music.play()
                elif line == "Recording! Tap along to the song.":
                    print("Recording mode active — waiting for song to finish...")
                    is_song_recording = True
                elif line == "PAUSE":
                    pygame.mixer.music.pause()
                elif line == "RESUME":
                    pygame.mixer.music.unpause()
                elif line == "BEATMAP_START":
                    recording = True
                    beat_map = []
                    print("Receiving beat map...")
                elif line == "BEATMAP_END":
                    recording = False
                    is_song_recording = False
                    with open('beatmap.txt', 'w') as f:
                        for entry in beat_map:
                            f.write(entry + '\n')
                    print(f"Beat map saved! {len(beat_map)} beats recorded.")
                elif recording:
                    beat_map.append(line)
                    print(f"Beat: {line}")
                # --- NEW: Catch Game Over ---
                elif line == "GAME_OVER":
                    print("Displaying Results Screen...")
                    # Optional: call a function that prints a score/results screen
                    # display_results() 
                    continue
                else:
                    print(line)

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
# Start Threads
# =====================================================================
read_thread = threading.Thread(
    target=read_from_arduino,
    daemon=True
)

write_thread = threading.Thread(
    target=write_to_arduino,
    daemon=True
)

read_thread.start()
write_thread.start()

# =====================================================================
# Main Loop
# =====================================================================
try:
    while running:
        if is_song_recording:
            # Check if pygame is still playing music
            if not pygame.mixer.music.get_busy():
                print("Song finished! Sending stop signal to Arduino...")
                
                # Tell Arduino the game is over
                ser.write(b's\n')
                
                is_song_recording = False
                
        time.sleep(0.1)
except KeyboardInterrupt:
    shutdown()