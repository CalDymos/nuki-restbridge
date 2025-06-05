# coredumpToBIN.py
# Converts a NukiBridge coredump.txt to a binary .bin file for further analysis

def convert_coredump(txt_path, bin_path):
    with open(txt_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    # Skip first 2 lines (NUKI_REST_BRIDGE_HW and BUILD strings)
    hex_lines = [
        line.strip() for line in lines[2:]
        if line.strip() and all(c in "0123456789abcdefABCDEF" for c in line.strip())
    ]

    binary_data = bytearray()
    for line in hex_lines:
        try:
            binary_data.extend(bytearray.fromhex(line))
        except ValueError:
            # Ignore invalid hex lines
            continue

    with open(bin_path, "wb") as f:
        f.write(binary_data)

    print(f"[INFO] Core dump written to: {bin_path}")


if __name__ == "__main__":
    convert_coredump("tmp/coredump.txt", "tmp/coredump.bin")

