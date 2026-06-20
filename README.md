# THE BEAT DOWN! 🥁🚹

**THE BEAT DOWN!™** is an immersive, retro arcade-style rhythm game that bridges a custom hardware controller with an interactive desktop user interface. 

The system operates on a split-runtime architecture to bypass the 32KB flash memory constraints of the ATmega328P microcontroller: an **Arduino Uno** handles rapid hardware polling, sensor reads, and LED/LCD updates, while a companion **Python terminal engine** executes real-time graphics rendering, manages your custom beatmaps, and streams synchronized audio.

---

## 🏗️ System Architecture

The workload is split across a hardware-software pipeline connected via a serial bus optimized at a high-speed **115200 Baud rate** for dynamic rendering.

```
+---------------------------------------+         USART Serial        +---------------------------------------+
|          HARDWARE CONTROLLER          |      (115200 Baud Rate)     |             PC SYSTEM HOST            |
|  - ATmega328P Microcontroller         |  ========================>  |  - Python Script (song.py)            |
|  - 3-Lane Button Tapping Array        |  <========================  |  - Pygame Audio Core Subsystem        |
|  - Tri-Sensor Suite (Debug/Modifiers) |                             |  - Framed Terminal Interface Rendering|
|  - LCD & LED Feedback Indicator Setup |                             |  - Custom Beatmap File Storage        |
+---------------------------------------+                             +---------------------------------------+
```

### 1. Hardware Integration & Sensor Suite
The custom control deck incorporates a physical layout mapped directly to gameplay variables:
* **3-Button Array:** Tactile micro-switches connected to pins **D2, D3, and D4**, mapped to the Left, Middle, and Right music lanes.
* **Potentiometer (Analog Input):** Connected to Port A. Maps raw voltage data to three distinct game difficulty tiers: **Easy**, **Medium**, and **Difficult**.
* **Photo-resistor (Analog Input):** Connected to Port A. Measures ambient light levels. *(Note: Relegated to the real-time sensor diagnostic menu due to upstream timer/audio pause state complexities)*.
* **Digital Accelerometer (ADXL345 via TWI/I2C):** Connected to Port A (Pins A4/A5). Evaluates physical X-axis tilt to display a "Ready" notification on the LCD and activate a **2x Score Multiplier** power-up.
* **Output Display & Indicators:** An **LCD Display** provides secondary text feedback (e.g., power-up status), while a **Blue LED** flashes for successful notes and a **Red LED** lights up for misses or faulty timing windows.

### 2. Software Framework (`song.py`)
The desktop client handles performance-heavy tasks:
* **Dynamic Terminal Interface:** Renders a vertical scrolling note matrix using an `o` visual graphic to simulate falling notes down columns, enabling users to anticipate button presses.
* **Audio Engine:** Streams stutter-free background tracks (`intro.mp3`) during menus and loads full-track audio (`Goodness Gracious.mp3`) during live gameplay loops.

---

## 📂 Project Structure

```text
├── song.py               # Core PC engine (Serial interface, UI rendering, audio playback)
├── beatmap.txt             # CSV file containing timestamped delta time paired with target lane integers
├── intro.mp3               # Audio track looped during the start screen and main menu
└── Goodness Gracious.mp3   # The primary interactive gameplay music track
```

---

## 📊 Process Flowcharts

### 1. Hardware-to-PC Flowchart
This flowchart visualizes how data streams from your hardware inputs through USART serial communication into the core Python game engine modules.

```mermaid
graph TD
    %% Style Definitions
    classDef hardware fill:#f9f,stroke:#333,stroke-width:2px;
    classDef serial fill:#bbf,stroke:#333,stroke-width:2px,stroke-dasharray: 5 5;
    classDef software fill:#bfb,stroke:#333,stroke-width:2px;

    %% Hardware Components
    subgraph HW ["Hardware Controller (ATmega328P)"]
        A[3-Button Array Pins D2-D4] -->|Digital Input| E(USART Serial Buffer)
        B[Potentiometer Port A] -->|ADC Voltage Reads| E
        C[Photo-resistor Port A] -->|ADC Light Level Data| E
        D[ADXL345 Accelerometer Pins A4/A5] -->|TWI / I2C Protocol| E
    end

    %% Serial Protocol Link
    E -->|115200 Baud Serial Stream| F[song.py Serial Parser]

    %% Python PC Engine Components
    subgraph SW ["PC Host System (Python Engine)"]
        F -->|Input Taps| G[Scrolling Note Matrix Update]
        F -->|Potentiometer Value| H[Difficulty Selector Logic]
        F -->|Tilt Trigger| I[Score Multiplier & LCD Update]
        F -->|Light Readings| J[Sensor Debug Diagnostic Panel]
        
        G --> K[Terminal Interface Renderer]
        H --> K
        I --> K
        J --> K
        L[Pygame Audio Core] -->|Synced Audio Stream| K
    end

    %% Class Assigning
    class A,B,C,D,E hardware;
    class F serial;
    class G,H,I,J,K,L software;
```

### 2. Main Menu Logic
This diagram outlines the program's execution options across the 5 distinct operational modes handled by the script.

```mermaid
graph TD
    classDef state fill:#fde,stroke:#333,stroke-width:2px;
    classDef process fill:#fff,stroke:#333,stroke-width:1px;

    Start([Launch song.py]) --> Title[Animated Title Screen & Play intro.mp3]
    Title -->|Key Press| Menu{Main Menu Selection}

    %% Mode 1
    Menu -->|"[1] Start Game"| M1_Audio[Stop intro.mp3 & Start Game Track]
    M1_Audio --> M1_Load[Load Beats from beatmap.txt]
    M1_Load --> M1_Loop[Run Game Loop: Scrolling UI & Match Serial Input]
    M1_Loop --> M1_End[Game Over: Log Score, Check Leaderboard, Light LEDs]
    M1_End --> Menu

    %% Mode 2
    Menu -->|"[2] View High Scores"| M2_Scores[Render Top 9 Leaderboard Matrix]
    M2_Scores -->|Any Key| Menu

    %% Mode 3
    Menu -->|"[3] Record Custom Beatmap"| M3_Record[Stream Audio & Append Taps/Delta-Time to beatmap.txt]
    M3_Record -->|Song Finish / Max Beats Reached| Menu

    %% Mode 4
    Menu -->|"[4] Check Sensor Data"| M4_Debug[Display Real-Time Telemetry: Tilt, Pot, Light]
    M4_Debug -->|Any Key| Menu

    %% Mode 5
    Menu -->|"[5] Exit"| M5_Exit[Run Shutdown Procedure & Close COM Port]
    M5_Exit --> End([Terminate Program])

    class Title,Menu state;
```

---

## 📸 Gameplay Screenshots

### Title Screen
![Title Screen](Title%20Screen.png)

### Main Menu
![Main Menu](Main%20Menu.png)

### Live Gameplay Board
![Game Board](Game%20Board.png)

### High Scores Leaderboard
![Leaderboard](Leaderboard.png)

### Real-Time Sensor Data
![Sensor Data](Sensor%20Data.png)

---

---

## 🚀 Installation & Setup

### Prerequisites
1. **Python 3.8+** installed on your host system.
2. Core dependencies installed via pip:
   ```bash
   pip install pygame pyserial
   ```
3. An **ATmega328P / Arduino Uno development kit** loaded with your companion firmware code.

### Execution
1. Open `song.py` and modify the target `PORT` string to match your environment connection (e.g., 'COM5' on Windows, or '/dev/ttyUSB0' on Linux/macOS).
2. Connect your physical control deck to your device via USB.
3. Launch the central executable script directly from your terminal terminal:
   ```bash
   python song.py
   ```

---

## 🎮 Operational Modes

Navigate through 5 central selection profiles via the main terminal command prompt:
* **`[1] Start Game`:** Halts the menu music loop, processes target timestamps from `beatmap.txt`, updates the active scrolling grid UI, evaluates tap timing offsets, and flashes the corresponding hardware score LEDs.
* **`[2] View High Scores`:** Accesses your historical top 9 scorecard matrix alongside custom player initials.
* **`[3] Record Custom Beatmap`:** Streams selected audio tracks while capturing real-time hardware button inputs, saving structured delta-time markers into `beatmap.txt`.
* **`[4] Check Sensor Data`:** Boots a live telemetry monitoring view to troubleshoot incoming accelerometer angles, potentiometer dials, and photo-resistor light shifts instantly.
* **`[5] Exit`:** Safe-closes your listening serial COM channel, kills active Pygame audio threads, and terminates process flags cleanly.

---
