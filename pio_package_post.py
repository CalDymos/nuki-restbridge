""" PlatformIO POST script execution to copy artifacts and upload firmware """

Import("env") # type: ignore
import re, shutil, os, csv
from pathlib import Path
import subprocess, psutil
import serial.tools.list_ports
import tkinter as tk
from tkinter import Checkbutton, messagebox, simpledialog, ttk
import time
from glob import glob

global partOffsets
global partSizes
global build_dir
global project_dir

build_dir = env.subst("$BUILD_DIR")  # type: ignore
project_dir = env.subst("$PROJECT_DIR")  # type: ignore

def get_partition_offsets(env):
    """
    Reads partitions.csv and extracts offsets

    Returns:
        dict: {"otadata": "0x19000",
               "firmware": "0x20000", "littlefs": "0x220000"}
    """
    partitions_file = env.GetProjectOption("board_build.partitions", "partitions.csv")
    partitions_path = os.path.join(project_dir, partitions_file)

    # Set default values if `partitions.csv` does not exist
    offsets = {
        "bootloader": "0x1000",  # Standard offset for the bootloader
        "partition": "0x8000",   # Standard offset for the partition table
        "nvs": "0x9000",
        "otadata": None,
        "firmware": "0x10000",  # Default for firmware (if not found)
        "littlefs": "0x230000",
        "coredump": None
    }
    
    if not os.path.exists(partitions_path):
        print(f"[ERROR] Partition table {partitions_path} not found! Using defaults.")
        return offsets

    with open(partitions_path, "r") as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            if len(row) < 5 or row[0].startswith("#"):  # Skip comments and invalid lines
                continue
            name, type_, subtype, offset, size = row[:5]
            offset = offset.strip()

            # Bootloader is fixed, we don't need to read them
            if type_.strip() == "data" and subtype.strip() == "nvs":
                offsets["nvs"] = offset
            elif type_.strip() == "data" and subtype.strip() == "ota":
                offsets["otadata"] = offset
            elif type_.strip() == "app" and subtype.strip() == "factory":
                offsets["firmware"] = offset
            elif type_.strip().lower() == "spiffs":
                offsets["littlefs"] = offset
            elif name.strip().lower() == "coredump":
                offsets["coredump"] = offset

    # If certain values were not found, set default values
    if offsets["otadata"] is None:
        print("[WARNING] Otadata partition not found in partitions.csv! It will be skipped.")

    return offsets
    
def get_partition_sizes(env):
    """
    Reads partitions.csv and extracts sizes

    Returns:
        dict: {"bootloader": "0x1000", "partition": "0x9000", "otadata": "0x19000",
               "firmware": "0x20000", "littlefs": "0x220000"}
    """
    partitions_file = env.GetProjectOption("board_build.partitions", "partitions.csv")
    partitions_path = os.path.join(project_dir, partitions_file)

    # Set default values if `partitions.csv` does not exist
    sizes = {
        "bootloader": "0x7000",  # Standard size for the bootloader (28KB)
        "partition": "0x1000",   # Standard size for the partition table (4KB)
        "nvs": "0x5000",
        "otadata": None,
        "firmware": "0x220000",  # Default for firmware (if not found)
        "littlefs": "0x180000",
        "coredump": None
    }

    if not os.path.exists(partitions_path):
        print(f"[ERROR] Partition table {partitions_path} not found! Using defaults.")
        return sizes

    with open(partitions_path, "r") as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            if len(row) < 5 or row[0].startswith("#"):  # Skip comments and invalid lines
                continue
            name, type_, subtype, offset, size = row[:5]
            size = size.strip()

            # Bootloader is fixed, we don't need to read them
            if type_.strip() == "data" and subtype.strip() == "nvs":
                sizes["nvs"] = size
            elif type_.strip() == "data" and subtype.strip() == "ota":
                sizes["otadata"] = size
            elif type_.strip() == "app" and subtype.strip() == "factory":
                sizes["firmware"] = size
            elif type_.strip().lower() == "spiffs":
                sizes["littlefs"] = size
            elif name.strip().lower() == "coredump":
                sizes["coredump"] = size


    # If certain values were not found, set default values
    if sizes["otadata"] is None:
        print("[WARNING] Otadata partition not found in partitions.csv! It will be skipped.")

    return sizes


partOffsets:dict = get_partition_offsets(env) # type: ignore
partSizes:dict = get_partition_sizes(env) # type: ignore

def kill_serial_monitor(port):
    """ Terminates the running serial monitor using the specified port """
    
    # Terminate all processes that could block the COM port
    def kill_processes():
        blocked_processes = ["platformio.exe", "python.exe", "esptool.exe"]
        for proc in psutil.process_iter(["pid", "name", "cmdline"]):
            try:
                if any(proc_name in proc.info["name"].lower() for proc_name in blocked_processes):
                    if proc.info["cmdline"] and any(port in arg for arg in proc.info["cmdline"]):
                        print(f"[INFO] Killing process {proc.info['name']} (PID: {proc.pid}) using {port}...")
                        proc.kill()
                        proc.wait(timeout=2)
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                pass

    # 1. End blocking processes
    kill_processes()

    # 2. Try to close the serial connection
    try:
        with serial.Serial(port, baudrate=115200, timeout=1) as ser:
            ser.close()
        print(f"[INFO] Serial port {port} successfully released.")
    except serial.SerialException as e:
        print(f"[WARNING] Could not close {port}: {e}")

    # 3. Wait to make sure that the port is really free
    time.sleep(2)
        
def get_board_name(env):
    """ Gibt den richtigen Board-Namen für die Firmware-Dateien zurück """
    board_mapping = {
        'esp32-poe-iso': 'esp32poeiso',
        'wt32-eth01': 'wt32eth01',
        'T-ETH-POE': 'tethpoe',
        'esp32-s3-eth': 'esp32s3eth',
        'esp32_poe': 'esp32poe',
        'wesp32': 'wesp32'
    }

    board = env.get('BOARD')
    board = re.sub(r'_dbg$', '', board)
    
    return board_mapping.get(board, board)  # If not in the list, return default name


def create_target_dir(env):
    board = get_board_name(env)
    target_dir = env.GetProjectOption("custom_build") + '/' + board
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    return target_dir

def copy_files(source, target, env):
    file = Path(target[0].get_abspath())
    target_dir = create_target_dir(env)
    board = get_board_name(env)

    if "partitions.bin" in file.name:
        shutil.copy(file, f"{target_dir}/nuki_bridge.{file.name}")
    elif "firmware" in file.stem:
        shutil.copy(file, f"{target_dir}/nuki_bridge_{board}{file.suffix}")
    else:
        shutil.copy(file, f"{target_dir}/{file.name}")

def merge_bin(source, target, env):
    board = get_board_name(env)
    chip = env.get('BOARD_MCU')
    target_dir = create_target_dir(env)
    target_file = f"{target_dir}/webflash_nuki_bridge_{board}.bin"
    
    flash_args = [
        partOffsets["bootloader"], f"{build_dir}/bootloader.bin",
        partOffsets["partition"], f"{build_dir}/partitions.bin",
        partOffsets["firmware"], target[0].get_abspath(),
    ]
    
    # Check LittleFS and add if necessary
    littlefs_path = os.path.join(build_dir, "littlefs.bin")
    if os.path.exists(littlefs_path):
        flash_args += [partOffsets["littlefs"], littlefs_path]
    else:
        print(f"[INFO] No LittleFS image found in {littlefs_path}. Skipping in merge_bin().")
        
    cmd = f"esptool.py --chip {chip} merge_bin -o {target_file} --flash_mode dio --flash_freq keep --flash_size keep " + " ".join(flash_args)
    env.Execute(cmd)

def package_last_files(source, target, env):
    files = ["resources/boot_app0.bin"]

    target_dir = create_target_dir(env)
    for file in files:
        file = Path(file)
        shutil.copy(file, f"{target_dir}/{file.name}")

def center_window(window, width=300, height=150):
    """ Centers a Tkinter window on the screen """
    window.update_idletasks()  # Refreshes the window to get the correct sizes
    screen_width = window.winfo_screenwidth()
    screen_height = window.winfo_screenheight()
    
    x = (screen_width // 2) - (width // 2)
    y = (screen_height // 2) - (height // 2)
    
    window.geometry(f"{width}x{height}+{x}+{y}")
        
def select_serial_port():
    """ Zeigt ein Fenster mit allen verfügbaren seriellen Ports zur Auswahl """
    ports = list(serial.tools.list_ports.comports())
    
    if not ports:
        messagebox.showerror("Fehler", "Kein serieller Port gefunden! Überprüfen Sie die Verbindung.")
        return None

    # List of available ports
    port_list = [f"{port.device} - {port.description}" for port in ports]
    port_devices = [port.device for port in ports]  # Extract COM ports only
    
    def on_confirm():
        nonlocal selected_port
        selected_index = port_list.index(port_var.get())  # Index of the selected entry
        selected_port = port_devices[selected_index]  # Extract only the COM port
        root.destroy()
    
    root = tk.Tk()
    root.withdraw()  # Hide main window
    root.attributes('-topmost', True)  # Always set windows in the foreground
    
    selected_port = None
    dialog = tk.Toplevel(root)
    dialog.title("Select port")
    center_window(dialog)
    
    label = tk.Label(dialog, text="Select a serial port:")
    label.pack(pady=10)
    
    port_var = tk.StringVar(value=port_list[0])
    port_combo = ttk.Combobox(dialog, textvariable=port_var, values=port_list, state="readonly", width=40)
    port_combo.pack()
    
    button_frame = tk.Frame(dialog)
    button_frame.pack(pady=10)
    
    confirm_button = tk.Button(button_frame, text="OK", command=on_confirm)
    confirm_button.pack()
    
    root.wait_window(dialog)
    
    return selected_port if selected_port else None
    
def upload_firmware(source, target, env):
    """ Flasht die generierte Firmware automatisch auf das ESP32-Board """

    board = get_board_name(env)
    chip = env.get('BOARD_MCU')
    firmware_path = Path(target[0].get_abspath())

    # Retrieve current upload port
    upload_port = env.GetProjectOption("upload_port", "COM3")
    monitor_speed = env.GetProjectOption("monitor_speed", "115200")  # Default: 115200 Baud
    upload_speed =  env.GetProjectOption("upload_speed", "115200")  # Default: 115200 Baud
        
    # GUI dialog to confirm the upload with checkbox for flash erase
    def on_confirm():
        nonlocal erase_flash, upload_confirmed
        erase_flash = erase_var.get()
        upload_confirmed = True
        root.destroy()

    root = tk.Tk()
    root.withdraw()  # Hide main window
    root.attributes('-topmost', True)  # Always set windows in the foreground
    
    erase_flash = False
    upload_confirmed = False
    
    dialog = tk.Toplevel(root)
    dialog.title("Firmware Upload")
    center_window(dialog)
        
    
    label = tk.Label(dialog, text=f"Should the firmware be uploaded to {board}?")
    label.pack(pady=10)
    
    erase_var = tk.BooleanVar()
    erase_checkbox = Checkbutton(dialog, text="Erase Flash before upload", variable=erase_var)
    erase_checkbox.pack()

    button_frame = tk.Frame(dialog)
    button_frame.pack(pady=10)

    confirm_button = tk.Button(button_frame, text="Yes", command=on_confirm)
    confirm_button.pack(side=tk.LEFT, padx=5)
    
    cancel_button = tk.Button(button_frame, text="No", command=dialog.destroy)
    cancel_button.pack(side=tk.RIGHT, padx=5)
    
    root.wait_window(dialog)
            
    if not upload_confirmed:
        print("[INFO] Upload canceled.")
        return

    # Check whether the `upload_port` exists
    available_ports = [port.device for port in serial.tools.list_ports.comports()]
    
    if upload_port not in available_ports:
        print(f"[WARNING] The port {upload_port} was not found!")
        upload_port = select_serial_port()
        
        if not upload_port:
            print("[ERROR] No valid port selected. Upload canceled.")
            return
        
    # Exit serial monitor if active   
    kill_serial_monitor(upload_port)
    time.sleep(1)
    
    # Optional: Erase flash before upload
    if erase_flash:
        cmd = f"esptool.py --chip {chip} --port {upload_port} erase_flash"
        print("[INFO] Erasing flash before upload...")
        env.Execute(cmd)
    
    # Upload the firmware with `esptool.py`.
    cmd = f"esptool.py --chip {chip} --port {upload_port} --baud {upload_speed} write_flash -z "
    cmd += f"{partOffsets['bootloader']} {build_dir}/bootloader.bin "
    cmd += f"{partOffsets['partition']} {build_dir}/partitions.bin "
    cmd += f"{partOffsets['firmware']} {firmware_path}"

    littlefs_path = os.path.join(build_dir, "littlefs.bin")
    if os.path.exists(littlefs_path):
        cmd += f" {partOffsets['littlefs']} {littlefs_path}"
    else:
        print("[INFO] No LittleFS image found. Skipping in upload_firmware().")
    
    print(f"[INFO] Starte Upload auf {upload_port}...")
    env.Execute(cmd)
    
    # Start Serial Monitor automatically after flashing
    print(f"[INFO] Öffne Serial Monitor auf {upload_port}...")
    subprocess.run(["pio", "device", "monitor", "--port", upload_port, "--baud", str(monitor_speed)])

def generate_littlefs(source, target, env):
    """ Creates the LittleFS image automatically after the build """
    littlefs_path = os.path.join(build_dir, "littlefs.bin")
    data_path = os.path.join(project_dir, "data")

    # Ensure that the data/ directory exists
    if not os.path.exists(data_path):
        print(f"[INFO] LittleFS data folder {data_path} does not exist, will be create!")
        os.mkdir(data_path)
    
    # Check whether at least one file is included
    data_files = [f for f in os.listdir(data_path) if os.path.isfile(os.path.join(data_path, f))]
    if not data_files:
        print(f"[INFO] No files found in {data_path}. empty LittleFS image will be create.")
            
        # get LittleFS offset address
        littlefs_size = partSizes["littlefs"] if partSizes["littlefs"] else "0x1E0000"

        # Select the correct mklittlefs version
        mklittlefs_dirs = glob(os.path.join(os.getenv("USERPROFILE"), ".platformio", "packages", "tool-mklittlefs*"))
        mklittlefs_exe = None

        for d in mklittlefs_dirs:
            candidate = os.path.join(d, "mklittlefs.exe")
            if os.path.exists(candidate):
                mklittlefs_exe = candidate
                break

        if not mklittlefs_exe:
            print("[ERROR] mklittlefs was not found!")
            return
        
        # Generate LittleFS-Image
        cmd = f'"{mklittlefs_exe}" -c "{data_path}" -b 4096 -p 256 -s {littlefs_size} "{littlefs_path}"'
        print(f"[INFO] Create LittleFS image: {cmd}")
        env.Execute(cmd)
        
    else:
        print(f"[INFO] Building LittleFS image with {len(data_files)} file(s) from /data...")
        result = subprocess.run(["pio", "run", "--target", "buildfs"], cwd=project_dir)   
        
        if result.returncode != 0:
            print("[ERROR] Failed to build LittleFS image using PlatformIO.")
            return
    
    print(f"[INFO] LittleFS image successfully built: {littlefs_path}")    
# Post-Build Actions
env.AddPostAction("$BUILD_DIR/firmware.bin", copy_files) # type: ignore
env.AddPostAction("$BUILD_DIR/firmware.bin", package_last_files) # type: ignore
env.AddPostAction("$BUILD_DIR/firmware.bin", generate_littlefs) # type: ignore
env.AddPostAction("$BUILD_DIR/partitions.bin", copy_files) # type: ignore
env.AddPostAction("$BUILD_DIR/bootloader.bin", copy_files) # type: ignore
env.AddPostAction("$BUILD_DIR/firmware.elf", copy_files) # type: ignore

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin) # type: ignore

# Automatic upload after the build process
env.AddPostAction("$BUILD_DIR/firmware.bin", upload_firmware) # type: ignore
