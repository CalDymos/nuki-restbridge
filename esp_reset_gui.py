import serial
import serial.tools.list_ports
import subprocess
import threading
import tkinter as tk
from tkinter import ttk

def get_available_ports():
    """Returns a list of available serial ports with descriptions."""
    ports = list(serial.tools.list_ports.comports())
    return [f"{port.device} - {port.description}" for port in ports]

def get_available_port_names():
    """Returns only the port names (COMx)."""
    ports = list(serial.tools.list_ports.comports())
    return [port.device for port in ports] 

def reset_esp(port, baudrate, mode):
    """Performs a reset on the ESP."""
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            ser.dtr = False
            ser.rts = True
    except Exception as e:
        print(f"Error: {e}")
        return

    # Serial Monitor in eigenem Thread starten, damit GUI nicht blockiert
    threading.Thread(target=start_serial_monitor, args=(port, baudrate, mode), daemon=True).start()

def start_serial_monitor(port, baudrate, mode):
    """Opens the serial monitor using PlatformIO in a non-blocking way."""
    
    cmd = ["platformio", "device", "monitor", "--port", port, "--baud", str(baudrate)]

    if mode == "HEX":
        cmd += ["--filter", "hexlify"]

    try:
        subprocess.Popen(cmd, shell=True)
    except Exception as e:
        print(f"Error opening Serial Monitor: {e}")

def start():
    """Starts the ESP reset process with user-selected settings."""
    selected_entry = port_var.get()
    selected_baud = baud_var.get()
    selected_mode = mode_var.get()

    if not selected_entry or not selected_baud:
        print("Error: No port or baud rate selected")
        return

    # Extrahiere nur den COM-Port aus "COM5 - USB-Serial"
    selected_port = selected_entry.split(" - ")[0]

    reset_esp(selected_port, int(selected_baud), selected_mode)
    root.destroy()  # GUI schließen nach Start

def center_window(window, width=400, height=150):
    """Centers a Tkinter window on the screen."""
    window.update_idletasks()
    screen_width = window.winfo_screenwidth()
    screen_height = window.winfo_screenheight()
    x = (screen_width // 2) - (width // 2)
    y = (screen_height // 2) - (height // 2)
    window.geometry(f"{width}x{height}+{x}+{y}")

port_list = get_available_ports()
port_devices = get_available_port_names()

# Create GUI
root = tk.Tk()
root.title("ESP Reset & Serial Monitor")
center_window(root)

# Port selection
tk.Label(root, text="Select COM Port:").grid(row=0, column=0, padx=5, pady=5)

port_var = tk.StringVar()
port_combobox = ttk.Combobox(root, textvariable=port_var, values=port_list, state="readonly", width=40)
port_combobox.grid(row=0, column=1, padx=5, pady=5)
if port_list:
    port_combobox.current(0)  # Setzt standardmäßig den ersten verfügbaren Port

# Baud rate selection
tk.Label(root, text="Select Baudrate:").grid(row=1, column=0, padx=5, pady=5)
baud_var = tk.StringVar(value="115200")  # Standard baud rate
baud_combobox = ttk.Combobox(root, textvariable=baud_var, values=["9600", "19200", "38400", "57600", "115200", "230400"], state="readonly")
baud_combobox.grid(row=1, column=1, padx=5, pady=5)

# Mode selection
tk.Label(root, text="Monitor Mode:").grid(row=2, column=0, padx=5, pady=5)
mode_var = tk.StringVar(value="TEXT")
mode_combobox = ttk.Combobox(root, textvariable=mode_var, values=["TEXT", "HEX"], state="readonly")
mode_combobox.grid(row=2, column=1, padx=5, pady=5)

# Buttons
ok_button = tk.Button(root, text="OK", command=start)
ok_button.grid(row=3, column=0, columnspan=2, pady=10)

root.mainloop()
