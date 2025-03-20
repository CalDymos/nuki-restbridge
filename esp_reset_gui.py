import serial
import serial.tools.list_ports
import subprocess
import tkinter as tk
from tkinter import ttk

def get_available_ports():
    """Scans for available serial ports and returns them as a list."""
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def reset_esp(port, baudrate):
    """Performs a reset on the ESP and opens the serial monitor."""
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            ser.dtr = False
            ser.rts = True
    except Exception as e:
        print(f"Error: {e}")
        return

    # Start Serial Monitor (here with PlatformIO, customizable for Putty or other tools)
    subprocess.run(["platformio", "device", "monitor", "--port", port, "--baud", str(baudrate)], shell=True)

def start():
    """Starts the ESP reset process with user-selected settings."""
    selected_port = port_var.get()
    selected_baud = baud_var.get()

    if not selected_port or not selected_baud:
        return  # If no selection has been made

    reset_esp(selected_port, int(selected_baud))

# Create GUI
root = tk.Tk()
root.title("ESP Reset & Serial Monitor")

# Port selection
tk.Label(root, text="Select COM Port:").grid(row=0, column=0, padx=5, pady=5)
port_var = tk.StringVar()
port_combobox = ttk.Combobox(root, textvariable=port_var, values=get_available_ports(), state="readonly")
port_combobox.grid(row=0, column=1, padx=5, pady=5)

# Baud rate selection
tk.Label(root, text="Select Baudrate:").grid(row=1, column=0, padx=5, pady=5)
baud_var = tk.StringVar(value="115200")  # Standard baud rate
baud_combobox = ttk.Combobox(root, textvariable=baud_var, values=["9600", "19200", "38400", "57600", "115200", "230400"], state="readonly")
baud_combobox.grid(row=1, column=1, padx=5, pady=5)

# Buttons
ok_button = tk.Button(root, text="OK", command=start)
ok_button.grid(row=2, column=0, columnspan=2, pady=10)

root.mainloop()
