import tkinter as tk
from tkinter import ttk, messagebox
import subprocess
import ipaddress
from concurrent.futures import ThreadPoolExecutor

# IP lists
online_ips = []
offline_ips = []

# IP check (Ping)
def ping(ip):
    param = "-n" if subprocess.os.name == "nt" else "-c"
    result = subprocess.run(
        ["ping", param, "1", str(ip)],
        capture_output=True,
        text=True,
        encoding="latin1", 
        errors="replace"   # Replace invalid characters
    )

    output = result.stdout.lower()

    # Check for unreachable host indicators
    if subprocess.os.name == "nt":
        if "zielhost nicht erreichbar" in output or "allgemeiner fehler" in output:
            return str(ip), False
        elif "destination host unreachable" in output or "general failure" in output:
            return str(ip), False
    else:
        if "destination host unreachable" in output:
            return str(ip), False

    return str(ip), result.returncode == 0



# start Scan (UI + Scan-Thread)
def start_scan():
    ip_range = entry.get().strip()
    if not ip_range:
        messagebox.showerror("Error", "Please enter a valid IP range (e.g., 192.168.0.0/24)")
        return

    try:
        network = ipaddress.IPv4Network(ip_range, strict=False)
    except ValueError:
        messagebox.showerror("Error", "Invalid IP range!")
        return

    # Reset UI and lists
    scan_button.config(state=tk.DISABLED)
    result_label.config(text="Scanning...")
    progress_bar.config(mode="indeterminate")
    progress_bar.start()
    list_online.delete(0, tk.END)
    list_offline.delete(0, tk.END)
    online_ips.clear()
    offline_ips.clear()

    ip_list = list(network.hosts())

    def run():
        with ThreadPoolExecutor(max_workers=100) as executor:
            results = list(executor.map(ping, ip_list))

        for ip, status in results:
            if status:
                online_ips.append(ip)
            else:
                offline_ips.append(ip)

        update_ui()

    root.after(100, lambda: executor.submit(run))

# Update the UI with scan results
def update_ui():
    online_ips.sort()
    offline_ips.sort()

    for ip in online_ips:
        list_online.insert(tk.END, ip)
    for ip in offline_ips:
        list_offline.insert(tk.END, ip)

    progress_bar.stop()
    progress_bar.config(mode="determinate")
    result_label.config(text=f"Scan complete: {len(online_ips)} active, {len(offline_ips)} free")
    scan_button.config(state=tk.NORMAL)

# Build the GUI
root = tk.Tk()
root.title("IP Scanner")
root.geometry("700x450")
root.resizable(False, False)

frame = tk.Frame(root)
frame.pack(padx=10, pady=10, fill="both", expand=True)

# Input field for IP range
tk.Label(frame, text="IP range (CIDR e.g. 192.168.0.0/24):").pack()
entry = tk.Entry(frame, width=30)
entry.insert(0, "192.6.1.0/24")
entry.pack(pady=5)

# Start scan button
scan_button = tk.Button(frame, text="Start Scan", command=start_scan)
scan_button.pack(pady=10)

# Result label
result_label = tk.Label(frame, text="Ready", font=("Arial", 12))
result_label.pack()

# Progress bar
progress_bar = ttk.Progressbar(frame, length=600)
progress_bar.pack(pady=10)

# IP result lists
lists_frame = tk.Frame(frame)
lists_frame.pack(pady=10, fill="both", expand=True)

# List labels
tk.Label(lists_frame, text="🟢 Active IPs").grid(row=0, column=0)
tk.Label(lists_frame, text="⚪ Free IPs").grid(row=0, column=1)

# List boxes
list_online = tk.Listbox(lists_frame, width=30, height=15)
list_online.grid(row=1, column=0, padx=10)

list_offline = tk.Listbox(lists_frame, width=30, height=15)
list_offline.grid(row=1, column=1, padx=10)

# Background executor
executor = ThreadPoolExecutor(max_workers=1)

# Start the GUI loop
root.mainloop()
