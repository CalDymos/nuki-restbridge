import json
import sys
import tkinter as tk
from tkinter import ttk, messagebox


def main() -> int:
    payload_raw = sys.stdin.read().strip()
    if not payload_raw:
        print(json.dumps({"confirmed": False, "erase_flash": False, "port": ""}))
        return 1

    payload = json.loads(payload_raw)
    board = payload.get("board", "")
    ports = payload.get("ports", [])
    default_port = payload.get("default_port", "")
    default_erase = bool(payload.get("default_erase", False))

    if not ports:
        messagebox.showerror("Error", "No serial ports found.")
        print(json.dumps({"confirmed": False, "erase_flash": False, "port": ""}))
        return 2

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)

    dialog = tk.Toplevel(root)
    dialog.title("Firmware Upload")
    dialog.resizable(False, False)
    dialog.attributes("-topmost", True)

    width, height = 520, 190
    dialog.update_idletasks()
    x = (dialog.winfo_screenwidth() // 2) - (width // 2)
    y = (dialog.winfo_screenheight() // 2) - (height // 2)
    dialog.geometry(f"{width}x{height}+{x}+{y}")

    tk.Label(dialog, text=f"Upload firmware to {board}?", anchor="w").pack(padx=12, pady=(12, 8), fill="x")

    port_display = [f"{p['device']} - {p.get('description', '')}".strip() for p in ports]
    port_map = {s.split(" - ", 1)[0].strip(): s for s in port_display}

    port_var = tk.StringVar()
    if default_port and default_port in port_map:
        port_var.set(port_map[default_port])
    else:
        port_var.set(port_display[0])

    port_row = tk.Frame(dialog)
    port_row.pack(padx=12, pady=(0, 10), fill="x")

    tk.Label(port_row, text="Port:", width=8, anchor="w").pack(side="left")
    port_combo = ttk.Combobox(port_row, textvariable=port_var, values=port_display, state="readonly", width=55)
    port_combo.pack(side="left", fill="x", expand=True)

    erase_var = tk.BooleanVar(value=default_erase)
    tk.Checkbutton(dialog, text="Erase flash before upload", variable=erase_var).pack(padx=12, pady=(0, 10), anchor="w")

    result = {"confirmed": False, "erase_flash": False, "port": ""}

    def on_upload() -> None:
        selected = port_var.get().split(" - ", 1)[0].strip()
        result["confirmed"] = True
        result["erase_flash"] = bool(erase_var.get())
        result["port"] = selected
        dialog.destroy()
        root.destroy()

    def on_cancel() -> None:
        dialog.destroy()
        root.destroy()

    btn_row = tk.Frame(dialog)
    btn_row.pack(padx=12, pady=(0, 12), fill="x")

    tk.Button(btn_row, text="Upload", command=on_upload, width=12).pack(side="left")
    tk.Button(btn_row, text="Cancel", command=on_cancel, width=12).pack(side="right")

    root.wait_window(dialog)

    print(json.dumps(result))
    return 0 if result["confirmed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
