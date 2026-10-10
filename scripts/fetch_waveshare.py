"""Download/cache the Waveshare 1.43-inch board support files.

Runs in two contexts:
* PlatformIO extra_scripts (SCons does NOT define ``__file__``)
* Standalone: python scripts/fetch_waveshare.py
"""
from pathlib import Path
import os
import urllib.error
import urllib.request

# PlatformIO's SConscript execution uses Import("env"); it does not provide
# __file__ like a normally imported/executed Python module.
# Keep standalone invocation supported too.
if "Import" in globals():
    Import("env")

REPO = "thelastoutpostworkshop/waveshare_esp32s3_1.43_amoled_lvgl9"
BRANCH = "main"
DRIVER_FILES = (
    "amoled.cpp", "amoled.h",
    "FT3168.cpp", "FT3168.h",
    "board_config.h",
    "low_level_amoled.c", "low_level_amoled.h",
    "i2c.c", "i2c.h",
    "qmi8658c.cpp", "qmi8658c.h",
)


def project_root():
    """Resolve from PlatformIO's environment; only standalone uses __file__."""
    pio_env = globals().get("env")
    if pio_env is not None:
        return Path(pio_env.subst("$PROJECT_DIR")).resolve()
    script_file = globals().get("__file__")
    if script_file:
        return Path(script_file).resolve().parents[1]
    raise RuntimeError("Cannot determine project directory: missing PlatformIO env")


def ensure_drivers(target=None, opener=urllib.request.urlopen):
    dest = Path(target) if target is not None else project_root() / "src" / "vendor"
    dest.mkdir(parents=True, exist_ok=True)
    for name in DRIVER_FILES:
        output = dest / name
        if output.exists() and output.stat().st_size > 0:
            continue
        url = f"https://raw.githubusercontent.com/{REPO}/{BRANCH}/{name}"
        print(f"[Waveshare] Downloading {name}")
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "ai-usage-monitor-pio/1.1"})
            with opener(request, timeout=30) as response:
                data = response.read()
            if not data or b"<html" in data[:200].lower():
                raise ValueError("Unexpected empty/HTML response")
            tmp = dest / f"{name}.part"
            tmp.write_bytes(data)
            os.replace(tmp, output)
        except (urllib.error.URLError, OSError, ValueError) as exc:
            raise RuntimeError(
                f"Cannot fetch {name} from {url}: {exc}. "
                f"Copy the required board driver files from {REPO} to {dest}, "
                "or run again with Internet access."
            ) from exc
    print("[Waveshare] Board drivers present.")


# Pre-build execution must populate src/vendor before compiler source discovery.
ensure_drivers()
