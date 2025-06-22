import os
import json
import struct
import base64
import subprocess
import re
import sys
import argparse
from Crypto.Random import get_random_bytes
from hashlib import sha256
from Crypto.Cipher import AES

from helper import (
    generate_key_nonce,
    extract_audio,
    get_annexb_stream,
    parse_nal_units,
    extract_rbsp,
    insert_emulation_prevention,
    encrypt_audio,
    cleanup
)

def trace_h264_headers_line_by_line(input_file: str) -> tuple[list[int], int, list[int]]:
    """
    Run ffmpeg with the "trace_headers" bitstream filter to extract
    per-slice QP and NAL type information.

    Args:
        input_file (str): Path to the input MP4 file.

    Returns:
        tuple:
            - qps (list[int]): List of slice QP values in decoding order.
            - qp_init (int): The initial picture QP (pic_init_qp_minus26 + 26).
            - nal_types (list[int]): Corresponding NAL unit types.
    """
    cmd = [
        "ffmpeg", "-i", input_file,
        "-c", "copy",
        "-bsf:v", "trace_headers",
        "-f", "null", "-"
    ]
    qps: list[int] = []
    nal_types: list[int] = []
    qp_init = 0
    current_nal = None

    proc = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.DEVNULL, text=True)
    assert proc.stderr is not None

    for raw_line in proc.stderr:
        line = raw_line.strip()
        if not line:
            continue
        entry = re.sub(r"^\[.*?\]\s*", "", line)

        if "nal_unit_type" in entry:
            current_nal = int(entry.split("=")[1])
        elif "pic_init_qp_minus26" in entry:
            qp_init = int(entry.split("=")[1]) + 26
        elif "slice_qp_delta" in entry and current_nal in (1, 5):
            qp = int(entry.split("=")[1]) + qp_init
            qps.append(qp)
            nal_types.append(current_nal)

    proc.wait()
    return qps, qp_init, nal_types


def generate_and_save_seeds(num_seeds: int, out_file: str) -> list[str]:
    """
    Create a JSON file containing cryptographically secure random seeds.

    Args:
        num_seeds (int): Number of seeds to generate.
        out_file (str): Path to write the JSON {"seeds": [...]}.

    Returns:
        list[str]: The list of base64-encoded seeds.
    """
    seeds: list[str] = []
    for _ in range(num_seeds):
        raw = get_random_bytes(32)
        b64 = base64.urlsafe_b64encode(raw).decode('utf-8').rstrip('=')
        seeds.append(b64)

    with open(out_file, 'w') as f:
        json.dump({'seeds': seeds}, f, indent=2)

    print(f"[+] Generated {num_seeds} seeds and saved to {out_file}")
    return seeds


def selective_encrypt(
    input_mp4: str,
    out_h264: str,
    out_audio: str,
    seed: str,
    qps: list[int],
    qp_threshold: int
) -> None:
    """
    Selectively encrypt H.264 slices whose QP <= qp_threshold.
    Attempts to encrypt audio; skips if extraction fails.

    Args:
        input_mp4 (str): Original MP4 file path.
        out_h264 (str): Path to write encrypted raw H.264 Annex-B stream.
        out_audio (str): Path to write encrypted AAC audio stream.
        seed (str): Base64 seed for key/nonce derivation.
        qps (list[int]): QP list per slice.
        qp_threshold (int): Threshold below which slices get encrypted.
    """
    temp_h264 = 'temp_stream.h264'
    audio_ok = extract_audio(input_mp4, out_audio)
    get_annexb_stream(input_mp4, temp_h264)

    raw = open(temp_h264, 'rb').read()
    os.remove(temp_h264)

    nals = parse_nal_units(raw)
    encrypted_nals: list[bytes] = []
    count = 0

    for nal in nals:
        offset = 4 if nal.startswith(b'\x00\x00\x00\x01') else 3
        if len(nal) <= offset:
            encrypted_nals.append(nal)
            continue

        nal_type = nal[offset] & 0x1F
        if nal_type in (1, 5) and count < len(qps) and qps[count] <= qp_threshold:
            key, nonce = generate_key_nonce(seed, count)
            cipher = AES.new(key, AES.MODE_CTR, nonce=nonce)
            rbsp = extract_rbsp(nal[offset+1:])
            enc_rbsp = cipher.encrypt(rbsp)
            new_payload = insert_emulation_prevention(enc_rbsp)
            encrypted_nals.append(nal[:offset+1] + new_payload)
            count += 1
        else:
            encrypted_nals.append(nal)
            if nal_type in (1, 5):
                count += 1

    with open(out_h264, 'wb') as f:
        for unit in encrypted_nals:
            f.write(unit)

    if audio_ok:
        encrypt_audio(out_audio, out_audio + '.enc', seed)
        os.replace(out_audio + '.enc', out_audio)
    else:
        print("Audio encryption skipped; only video encrypted.")

    print(f"[+] Encrypted {count} video slices.")


def encrypt_bytes(data: bytes, seed: str, index: int) -> bytes:
    """
    Encrypt arbitrary data using AES-CTR with deterministic key/nonce.

    Args:
        data (bytes): Raw bytes to encrypt.
        seed (str): Base64 seed string.
        index (int): Index for key/nonce derivation.

    Returns:
        bytes: Encrypted ciphertext.
    """
    key, nonce = generate_key_nonce(seed, index)
    return AES.new(key, AES.MODE_CTR, nonce=nonce).encrypt(data)


def package_encrypted_files(
    enc_video: str,
    enc_audio: str,
    qps: list[int],
    qp_threshold: int,
    extension: str,
    seed: str,
    output_file: str,
    include_audio: bool
) -> None:
    """
    Bundle encrypted video, optional audio, and metadata into a single .bin package.
    Each section is prefixed by its length (8-byte big-endian unsigned).

    Args:
        enc_video (str): Path to encrypted H.264 Annex-B file.
        enc_audio (str): Path to encrypted AAC file (if available).
        qps (list[int]): Original slice QP list.
        qp_threshold (int): QP threshold used.
        extension (str): Original file extension (e.g. '.mp4').
        seed (str): Base64 seed for metadata encryption.
        output_file (str): Destination package path.
        include_audio (bool): Whether audio_blob is valid.
    """
    video_blob = open(enc_video, 'rb').read()
    audio_blob = b''
    if include_audio and os.path.exists(enc_audio):
        audio_blob = open(enc_audio, 'rb').read()

    metadata = json.dumps({
        'qps': qps,
        'qp_threshold': qp_threshold,
        'extension': extension,
        'audio_included': include_audio
    }).encode('utf-8')
    enc_meta = encrypt_bytes(metadata, seed, index=-2)

    with open(output_file, 'wb') as pkg:
        for blob in (video_blob, audio_blob, enc_meta):
            pkg.write(struct.pack('>Q', len(blob)))
            pkg.write(blob)

    print(f"[+] Package created: {output_file}")


def parse_args():
    parser = argparse.ArgumentParser(description="Selective video/audio encryption CLI tool")
    parser.add_argument("-i", "--input", required=True, help="Path to input video file (MP4)")
    parser.add_argument("-t", "--threshold", type=int, default=30, help="QP threshold for encryption")
    parser.add_argument("-k","--no-cleanup", action='store_true', help="Don't delete intermediate files")
    parser.add_argument("-s","--seeds", help="Path to seeds file (Optional Defaults to output Dir)")
    parser.add_argument("-o", "--output", default=None, help="Directory for output files (defaults to input directory)")
    return parser.parse_args()


def main():
    args = parse_args()
    input_path = args.input
    threshold = args.threshold
    num_seeds = 2
    out_dir = args.output or os.path.dirname(input_path)
    # print(out_dir)
    if(out_dir == ''):
        out_dir = "./"
    delete_intermediate = not args.no_cleanup

    base, ext = os.path.splitext(os.path.basename(input_path))

    if not os.path.exists(input_path):
        print(f"[-] Input file not found: {input_path}")
        sys.exit(1)

    os.makedirs(out_dir, exist_ok=True)

    print(base)
    input_dir = os.path.dirname(input_path)
    print(input_dir)
    seed_path = args.seeds or out_dir
    enc_h264 = os.path.join(out_dir, f"{base}_encrypted.h264")
    enc_aac = os.path.join(out_dir, f"{base}_encrypted.aac")
    package_file = os.path.join(out_dir, f"{base}.bin")
    seed_file = os.path.join(seed_path, f"{base}_seeds.json")

    seeds = generate_and_save_seeds(num_seeds, seed_file)
    qps, _, _ = trace_h264_headers_line_by_line(input_path)

    selective_encrypt(input_path, enc_h264, enc_aac, seeds[0], qps, threshold)

    metadata = {
        'qps': qps,
        'qp_threshold': threshold,
        'extension': ext,
        'audio_included': os.path.exists(enc_aac)
    }
    with open(os.path.join(out_dir, f"{base}_metadata.json"), 'w') as f:
        json.dump(metadata, f, indent=4)

    include_audio = os.path.exists(enc_aac)
    package_encrypted_files(
        enc_h264,
        enc_aac,
        qps,
        threshold,
        ext,
        seeds[1],
        package_file,
        include_audio
    )

    if delete_intermediate:
        cleanup([enc_h264, enc_aac])

if __name__ == '__main__':
    main()
