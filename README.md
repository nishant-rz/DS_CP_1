# RESQNET Mission Control — DS_CP_1

A small wildfire and satellite telemetry demo with a C data structures engine, a Python telemetry generator and HTTP server, and a browser dashboard.

## Files in the project

- `dsa_cp.c` — C engine (sorting, searching, hash table, trees, graph routing, and JSON export).
- `fetch_satellite.py` — generates `telemetry.csv` and `sat_meta.json`, then runs the compiled C engine.
- `server.py` — serves the dashboard and triggers a satellite pass from the browser.
- `index.html` — dashboard UI.
- `satellite_rescue_management_literature_and_gap_analysis.txt` — currently empty; it is not read by the program. Add references here if your submission requires a bibliography.

The generated files (`dsa_cp.exe`, `telemetry.csv`, `sat_meta.json`, and `state.json`) do not need to be included in the ZIP. The executable is built locally, and the data files are generated when you run the project.

## Requirements

- Windows 10/11 (instructions below), Python 3.9 or newer, and a C compiler such as GCC from MinGW-w64/MSYS2.
- A modern browser.
- Internet access for live satellite/weather lookups and for the map tiles, fonts, and Leaflet library. The Python script has fallback sample values when its data requests fail; map imagery and external UI assets may not load offline.
- No third-party Python packages are required; the Python code uses the standard library.

## Build and run

1. Extract the ZIP to a folder. Keep `dsa_cp.c`, `fetch_satellite.py`, `server.py`, and `index.html` together.
2. Open Command Prompt or PowerShell and change to the extracted project folder, for example:

   ```powershell
   cd "C:\path\to\DS_CP_1"
   ```

3. Compile the C engine with GCC:

   ```powershell
   gcc dsa_cp.c -o dsa_cp.exe -lm
   ```

   If Windows says `gcc` is not recognized, install/configure a GCC toolchain (for example MinGW-w64/MSYS2) and reopen the terminal.

4. Start the web server:

   ```powershell
   py server.py
   ```

   If the `py` launcher is unavailable, use `python server.py`.

5. Open **http://localhost:8000** in your browser. Click **Replay Pass** for the bundled/replay hotspot scenario or **Live Pass** to use live-derived telemetry. The first pass creates the data files and refreshes the dashboard.
6. Keep the terminal/server open while using the dashboard. Press **Ctrl+C** in the terminal to stop it.

## Files generated at runtime

- `telemetry.csv` — sector telemetry created by `fetch_satellite.py`.
- `sat_meta.json` — satellite scene metadata (or fallback metadata) created by `fetch_satellite.py`.
- `state.json` — dashboard state exported by `dsa_cp.exe` after reading the CSV.

To generate these before opening the dashboard, run the following from the project folder after compiling:

```powershell
py fetch_satellite.py replay
```

Use `py fetch_satellite.py live` for live-derived telemetry. The dashboard's buttons also trigger the Python script through `server.py`.

## Optional: interactive C menu

To use the C engine's console menu instead of its automatic mode:

```powershell
.\dsa_cp.exe
```

The menu reads `telemetry.csv` if it exists and writes `state.json`. Generate telemetry first with `py fetch_satellite.py replay` if you want the menu to use a fresh pass.

## Packaging for group mates

Include `README.md` and the five project files listed above in the ZIP. You may omit generated output files and the compiled executable; each person can compile and generate them by following the steps above. Keep the folder structure flat, with files at the top level rather than inside an extra nested copy of the folder.


