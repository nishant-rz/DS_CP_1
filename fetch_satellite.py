import csv
import json
import subprocess
import sys
import os
import urllib.request
from datetime import datetime

# =====================================================================
# OCTOBER 2026 INDONESIA PEATLAND & FOREST FIRE SECTORS
# =====================================================================
INDONESIA_SECTORS = [
    # row, col, code, areaName, lat, lon, baseline_ndvi, mappedGraphNode
    (0, 0, "IDN-KLM0", "Palangka_Raya_Airbase",     -2.2136, 113.9108, 0.62, 0),
    (0, 1, "IDN-KLM1", "Sebangau_Peat_Kalimantan",  -2.5800, 113.8800, 0.72, 4),
    (0, 2, "IDN-JVA1", "Bromo_Tengger_Semeru_NP",   -7.9425, 112.9530, 0.65, 4),
    (1, 1, "IDN-KLM2", "Tumbang_Nusa_Peat_Bridge",  -2.3850, 114.0920, 0.68, 1),
    (1, 2, "IDN-SUM1", "Ogan_Komering_Ilir_Sumatra",-3.3800, 105.1500, 0.70, 2),
    (2, 1, "IDN-KLM3", "Pulang_Pisau_Peatlands",    -2.7540, 114.2600, 0.69, 2),
    (2, 2, "IDN-PAP1", "Merauke_Savanna_Papua",     -8.4932, 140.4018, 0.64, 7),
    (3, 1, "IDN-SUM2", "Riau_Malacca_Haze_Corridor", 0.5071, 101.4478, 0.66, 5),
]

def fetch_json(url, method="GET", payload=None, timeout=6):
    req = urllib.request.Request(url, method=method)
    req.add_header("User-Agent", "RESQNET-Indonesia-Fire-DSA/2.0")
    if payload:
        req.add_header("Content-Type", "application/json")
        data = json.dumps(payload).encode("utf-8")
    else:
        data = None
    with urllib.request.urlopen(req, data=data, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))

def fetch_sentinel2_kalimantan_metadata():
    """Queries live Copernicus Sentinel-2 L2A STAC Catalog over Central Kalimantan."""
    print("[1/3] Querying Copernicus Sentinel-2 / VIIRS Catalog over Central Kalimantan...")
    stac_url = "https://earth-search.aws.element84.com/v1/search"
    payload = {
        "collections": ["sentinel-2-l2a"],
        "bbox": [113.20, -3.40, 114.60, -2.00],
        "limit": 1,
        "sortby": [{"field": "properties.datetime", "direction": "desc"}]
    }
    try:
        data = fetch_json(stac_url, method="POST", payload=payload)
        if data.get("features"):
            feat = data["features"][0]
            props = feat["properties"]
            scene_id = feat["id"]
            dt = props.get("datetime", "Unknown")
            cloud = props.get("eo:cloud_cover", 0.0)
            print(f"      -> LIVE KALIMANTAN SCENE: {scene_id} | Date: {dt} | Cloud/Smoke: {cloud:.1f}%")
            return {"scene_id": scene_id, "datetime": dt, "cloud_cover": cloud}
    except Exception as e:
        print(f"      -> STAC Notice ({e}). Using Oct 2026 VIIRS/Sentinel Kalimantan Pass.")
    return {
        "scene_id": "VIIRS_SNPP_KALIMANTAN_799HS",
        "datetime": "2026-10-22T06:45:00Z",
        "cloud_cover": 38.4
    }

def fetch_live_surface_telemetry(lat, lon):
    url = (
        f"https://api.open-meteo.com/v1/forecast?"
        f"latitude={lat}&longitude={lon}"
        f"&current=temperature_2m,precipitation,rain,shortwave_radiation"
    )
    try:
        data = fetch_json(url, timeout=4)
        curr = data.get("current", {})
        return curr.get("temperature_2m", 32.0), curr.get("rain", 0.0), curr.get("shortwave_radiation", 550.0)
    except Exception:
        return 33.0, 0.0, 600.0

def compute_spectral_indices(row, col, code, name, lat, lon, base_ndvi, gnode, mode):
    temp_c, rain_mm, rad = fetch_live_surface_telemetry(lat, lon)
    thermal_k = round(temp_c + 273.15, 1)

    nir = 0.42
    red = nir * (1.0 - base_ndvi) / (1.0 + base_ndvi)
    green = 0.15 + min(rain_mm * 0.02, 0.20)

    ndvi = (nir - red) / (nir + red)
    ndwi = (green - nir) / (green + nir)

    # October 2026 Indonesia Wildfire & Peat Crisis Replay Signatures (799 Hotspots, 22,595 ha)
    if mode == "replay":
        if code == "IDN-KLM1":   # Sebangau Peatland (Central Kalimantan - Largest Share of 22,595 ha)
            ndvi, ndwi, thermal_k = 0.14, -0.44, 369.5
        elif code == "IDN-JVA1": # Bromo Tengger Semeru National Park (Fatal Mountain Crown Blaze)
            ndvi, ndwi, thermal_k = 0.11, -0.18, 378.2
        elif code == "IDN-KLM2": # Tumbang Nusa Peat Bridge (Blocks Trans-Kalimantan Highway Node 1)
            ndvi, ndwi, thermal_k = 0.18, -0.39, 358.4
        elif code == "IDN-SUM1": # Ogan Komering Ilir (Southern Sumatra Peat Hotspot Cluster)
            ndvi, ndwi, thermal_k = 0.16, -0.36, 354.0
        elif code == "IDN-KLM3": # Pulang Pisau Peatlands (Blocks Corridor Node 2)
            ndvi, ndwi, thermal_k = 0.21, -0.33, 349.8
        elif code == "IDN-PAP1": # Merauke Southern Papua Agricultural/Forest Fire
            ndvi, ndwi, thermal_k = 0.22, -0.22, 344.5
        elif code == "IDN-SUM2": # Riau / Malacca Strait Transboundary Smoke Plume
            ndvi, ndwi, thermal_k = 0.25, -0.20, 331.0

    return {
        "row": row, "col": col, "sectorCode": code, "areaName": name,
        "lat": lat, "lon": lon,
        "ndvi": round(ndvi, 2),
        "ndviBaseline": round(base_ndvi, 2),
        "ndwi": round(ndwi, 2),
        "thermalK": round(thermal_k, 1),
        "mappedGraphNode": gnode
    }

def generate_telemetry_csv(mode="replay", csv_path="telemetry.csv"):
    stac_meta = fetch_sentinel2_kalimantan_metadata()
    print(f"[2/3] Computing VIIRS Thermal, NDVI Burn & Peat NDWI across {len(INDONESIA_SECTORS)} Indonesian Sectors...")

    rows = [compute_spectral_indices(*s, mode=mode) for s in INDONESIA_SECTORS]

    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "row", "col", "sectorCode", "areaName", "lat", "lon",
            "ndvi", "ndviBaseline", "ndwi", "thermalK", "mappedGraphNode"
        ])
        for r in rows:
            writer.writerow([
                r["row"], r["col"], r["sectorCode"], r["areaName"],
                r["lat"], r["lon"], r["ndvi"], r["ndviBaseline"],
                r["ndwi"], r["thermalK"], r["mappedGraphNode"]
            ])

    with open("sat_meta.json", "w") as mf:
        json.dump({"mode": mode, "stac": stac_meta}, mf, indent=2)

def run_c_engine():
    print("[3/3] Invoking C DSA Engine (`dsa_cp --auto`)...")
    exe = "./dsa_cp.exe" if os.name == "nt" else "./dsa_cp"
    if not os.path.exists(exe) and os.path.exists("dsa_cp.exe"):
        exe = "dsa_cp.exe"
    elif not os.path.exists(exe) and os.path.exists("dsa_cp"):
        exe = "./dsa_cp"

    try:
        res = subprocess.run([exe, "--auto"], capture_output=True, text=True)
        print(res.stdout)
    except Exception as e:
        print(f"      -> Run `./dsa_cp` manually ({e}).")

if __name__ == "__main__":
    mode = "replay"
    if len(sys.argv) > 1 and sys.argv[1].lower() in ["live", "--live"]:
        mode = "live"
    generate_telemetry_csv(mode=mode)
    run_c_engine()