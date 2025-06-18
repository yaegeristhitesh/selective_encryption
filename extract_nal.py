#!/usr/bin/env python3
import sys

def find_nal_units(file_path):
    with open(file_path, 'rb') as f:
        data = f.read()
    nal_units = []
    start = 0
    # Look for NAL start codes (0x00000001)
    while True:
        start_code_index = data.find(b'\x00\x00\x00\x01', start)
        if start_code_index == -1:
            break
        # Find the next start code
        next_start_code = data.find(b'\x00\x00\x00\x01', start_code_index + 4)
        if next_start_code == -1:
            nal_unit = data[start_code_index+4:]
            nal_units.append(nal_unit)
            break
        else:
            nal_unit = data[start_code_index+4:next_start_code]
            nal_units.append(nal_unit)
            start = next_start_code
    return nal_units

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 extract_nal.py <output_track1.h264>")
        sys.exit(1)
    file_path = sys.argv[1]
    nal_units = find_nal_units(file_path)
    for i, nal in enumerate(nal_units):
        nal_type = nal[0] & 0x1F  # lower 5 bits
        print(f"NAL unit {i}: type {nal_type}, length {len(nal)} bytes")
        if nal_type == 7:
            print("-> Found SPS:")
            print(nal.hex())
        elif nal_type == 8:
            print("-> Found PPS:")
            print(nal.hex())

if __name__ == "__main__":
    main()
