Import("env")  # type: ignore
import re, shutil, os
from datetime import datetime, timezone


def recursive_purge(dir, pattern):
    if os.path.isdir(dir):
        for f in os.listdir(dir):
            if os.path.isdir(os.path.join(dir, f)):
                recursive_purge(os.path.join(dir, f), pattern)
            elif re.search(pattern, os.path.join(dir, f)):
                os.remove(os.path.join(dir, f))


regex_date  = r'(^\s*#define\s+NUKI_REST_BRIDGE_DATE\s+)"[^"]*"(.*)$'
regex_build = r'(^\s*#define\s+NUKI_REST_BRIDGE_BUILD\s+)"[^"]*"(.*)$'
regex_hw = r'(^\s*#define\s+NUKI_REST_BRIDGE_HW\s+)"[^"]*"(.*)$'
content_new = ""
file_content = ""

# board mapping
board_id = env.get("BOARD") # type: ignore
board_name_map = {
    "olimex-esp32-poe-iso-wroom": "OLIMEX ESP32-POE-ISO",
    "olimex-esp32-poe-iso-wrover": "OLIMEX ESP32-POE-ISO",
    "waveshare-esp32-s3-eth": "WAVESHARE ESP32-S3-ETH",
    "waveshare-esp32-s3-poe-eth": "WAVESHARE ESP32-S3-POE-ETH",
    "lilygo-t-eth-poe": "LILYGO T-ETH-POE",
    "jared-esp32-poe": "JARED ESP32-POE",
    "wt32-eth01": "WT32-ETH01",
    "wesp32": "wESP32",
}

hw_name = board_name_map.get(board_id, "UNKNOWN")


# Determine Git-Branch
try:
    import subprocess
    git_branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).decode("utf-8").strip()
except Exception:
    git_branch = "unknown"

print(f">>> [INFO] Build-Start: {datetime.now().isoformat()} | GIT-Branch: {git_branch} | Board: {hw_name}")

with open("src/Config.h", "r") as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex_build, r'\1"' + git_branch + r'"\2', file_content, flags=re.M)
    content_new = re.sub(regex_date, r'\1"' + datetime.now(timezone.utc).strftime("%Y-%m-%d") + r'"\2', content_new, flags=re.M)
    content_new = re.sub(regex_hw,  r'\1"' + hw_name + r'"\2', content_new, flags=re.M)

if content_new != file_content:
    with open("src/Config.h", "w") as writefile:
      writefile.write(content_new)

recursive_purge("managed_components", ".component_hash")

board = env.get("BOARD_MCU")  # type: ignore

if os.path.exists("sdkconfig." + board):
    f1 = 0
    f2 = 0
    f3 = 0
    f4 = os.path.getmtime("sdkconfig." + board)

    if os.path.exists("sdkconfig.defaults." + board):
        f1 = os.path.getmtime("sdkconfig.defaults." + board)

    if os.path.exists("sdkconfig.release.defaults"):
        f2 = os.path.getmtime("sdkconfig.release.defaults")

    if os.path.exists("sdkconfig.defaults"):
        f3 = os.path.getmtime("sdkconfig.defaults")

    if f1 > f4 or f2 > f4 or f3 > f4:
        os.remove("sdkconfig." + board)

if os.path.exists("sdkconfig." + board + "_dbg"):
    f1 = 0
    f2 = 0
    f3 = 0
    f4 = os.path.getmtime("sdkconfig." + board + "_dbg")

    if os.path.exists("sdkconfig.defaults." + board):
        f1 = os.path.getmtime("sdkconfig.defaults." + board)

    if os.path.exists("sdkconfig.debug.defaults"):
        f2 = os.path.getmtime("sdkconfig.debug.defaults")

    if os.path.exists("sdkconfig.defaults"):
        f3 = os.path.getmtime("sdkconfig.defaults")

    if f1 > f4 or f2 > f4 or f3 > f4:
        os.remove("sdkconfig." + board + "_dbg")