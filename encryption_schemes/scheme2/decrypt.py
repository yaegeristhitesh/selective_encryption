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
    parse_nal_units,
    extract_rbsp,
    insert_emulation_prevention,
    decrypt_audio,
    mux_audio_video,
    remux_to_mp4,
    cleanup
)

def decrypt_bytes(data: bytes, seed: str, index: int) -> bytes:
    """
    AES-CTR decrypt arbitrary bytes using a deterministic key and nonce.

    Args:
        data (bytes): The ciphertext to decrypt.
        seed (str): Base64 seed for key derivation.
        index (int): Index used in key/nonce derivation.

    Returns:
        bytes: The decrypted plaintext.
    """
    key, nonce = generate_key_nonce(seed, index)
    cipher = AES.new(key, AES.MODE_CTR, nonce=nonce)
    return cipher.decrypt(data)


def load_seeds(in_file: str) -> list[str]:
    """
    Load URL-safe base64 seeds from a JSON file.

    Args:
        in_file (str): Path to the JSON file containing {'seeds': [...]}.

    Returns:
        list[str]: The list of seed strings.
    """
    with open(in_file, 'r') as f:
        data = json.load(f)
    seeds = data.get('seeds', [])
    print(f"[+] Loaded {len(seeds)} seeds from {in_file}")
    return seeds


def unpack_package(package_file: str, seed: str, extract_dir: str) -> dict:
    """
    Unpack a custom .bin package and decrypt metadata.

    Args:
        package_file (str): Path to the package .bin file.
        seed (str): Base64 seed for metadata decryption.
        extract_dir (str): Directory to write extracted blobs.

    Returns:
        dict: Contains paths and metadata:
            video_enc (str), audio_enc (str), meta (dict)
    """
    os.makedirs(extract_dir, exist_ok=True)
    with open(package_file, 'rb') as pkg:
        vlen = struct.unpack('>Q', pkg.read(8))[0]
        vid_data = pkg.read(vlen)
        alen = struct.unpack('>Q', pkg.read(8))[0]
        aud_data = pkg.read(alen)
        mlen = struct.unpack('>Q', pkg.read(8))[0]
        meta_enc = pkg.read(mlen)

    video_enc = os.path.join(extract_dir, 'video.h264')
    audio_enc = os.path.join(extract_dir, 'audio.aac')
    with open(video_enc, 'wb') as f:
        f.write(vid_data)
    with open(audio_enc, 'wb') as f:
        f.write(aud_data)

    meta_bytes = decrypt_bytes(meta_enc, seed, index=-2)
    meta = json.loads(meta_bytes.decode('utf-8'))
    print(f"[+] Unpacked package and decrypted metadata from {package_file}")

    return {'video_enc': video_enc, 'audio_enc': audio_enc, 'meta': meta}


def selective_decrypt(
    enc_h264: str,
    out_h264: str,
    seed: str,
    qps: list[int],
    qp_threshold: int
) -> list[list[int]]:
    """
    Selectively decrypt H.264 slices whose QP <= qp_threshold.

    Args:
        enc_h264 (str): Path to encrypted H.264 Annex-B file.
        out_h264 (str): Path for decrypted raw H.264 stream.
        seed (str): Base64 seed for key derivation.
        qps (list[int]): Original slice QP list.
        qp_threshold (int): Threshold used during encryption.

    Returns:
        list[list[int]]: List of [nal_type, qp] for decrypted slices.
    """
    data = open(enc_h264, 'rb').read()
    nals = parse_nal_units(data)
    decrypted: list[bytes] = []
    dec_units: list[list[int]] = []
    count = 0

    for nal in nals:
        offset = 4 if nal.startswith(b'\x00\x00\x00\x01') else 3
        if len(nal) <= offset:
            decrypted.append(nal)
            continue
        nal_type = nal[offset] & 0x1F
        if nal_type in (1,5) and count < len(qps):
            if qps[count] <= qp_threshold:
                key, nonce = generate_key_nonce(seed, count)
                cipher = AES.new(key, AES.MODE_CTR, nonce=nonce)
                rbsp = extract_rbsp(nal[offset+1:])
                dec_rbsp = cipher.decrypt(rbsp)
                payload = insert_emulation_prevention(dec_rbsp)
                decrypted.append(nal[:offset+1] + payload)
                dec_units.append([nal_type, qps[count]])
            else:
                decrypted.append(nal)
            count += 1
        else:
            decrypted.append(nal)

    with open(out_h264, 'wb') as f:
        for unit in decrypted:
            f.write(unit)

    # print(f"[+] Decrypted video slices written to {out_h264}")
    return dec_units

def parse_args():
    parser = argparse.ArgumentParser(description="Selective H.264 video decryption tool")
    parser.add_argument('-p', '--package', default = None, help="Path to encrypted .bin package")
    parser.add_argument('-s', '--seed-file', help="Path to JSON file with base64 seeds (optional if name_seeds.json exists)")
    parser.add_argument('-o', '--output', default=None, help="Directory for output files (defaults to input directory)")
    parser.add_argument('-v', '--enc-h264', default = None, help="Path to encrypted H.264 Annex-B stream")
    parser.add_argument('-k', '--keep-temp', action='store_true', help="Keep intermediate files (default: deleted)")
    parser.add_argument('-m', '--metadata', default = None, help="Path to JSON file with metadata")
    args = parser.parse_args()
    return args




def main(): 
    args = parse_args()
    pkg_file = args.package
    enc_h264 = args.enc_h264
    seed_path = args.seed_file


    if not seed_path:
        print("[-] Must specify --seed-file for decryption")
        sys.exit(1)

    seeds = load_seeds(seed_path)

    if not pkg_file and not enc_h264:
        print("[-] Must specify either package file or encrypted H.264 stream")
        sys.exit(1)

    if pkg_file:
        pkg_dir = os.path.dirname(pkg_file)
        out_dir = args.output or pkg_dir
        if(out_dir == ""):
            out_dir = "./"
        name = os.path.splitext(os.path.basename(pkg_file))[0]
        os.makedirs(out_dir, exist_ok=True)
        out_h264 = os.path.join(out_dir, f"{name}_dec.h264")
        info = unpack_package(pkg_file, seeds[1], extract_dir=out_dir)
        video_enc = info['video_enc']
        audio_enc = info['audio_enc']
        meta = info['meta']
        qps = meta['qps']
        threshold = meta['qp_threshold']
        ext = meta.get('extension', '.mp4')
        audio_included = meta.get('audio_included', False)

        dec_units = selective_decrypt(video_enc, out_h264, seeds[0], qps, threshold)
        out_mp4 = os.path.join(out_dir, f"{name}_decrypted{ext}")

        if audio_included:
            out_aac = os.path.join(out_dir, f"{name}_decrypted.aac")
            decrypt_audio(audio_enc, out_aac, seeds[0])
            mux_audio_video(out_h264, out_aac, out_mp4)
            temp = [video_enc, audio_enc, out_h264, out_aac]
        else:
            remux_to_mp4(out_h264, out_mp4)
            temp = [video_enc, audio_enc, out_h264]

        print(f"[+] Decrypted video written to {out_mp4}")

        if not args.keep_temp:
            cleanup(temp)

    if enc_h264:
        out_dir = args.output or os.path.dirname(enc_h264)
        if(out_dir == ""):
            out_dir = "./"
        name = os.path.splitext(os.path.basename(enc_h264))[0]
        os.makedirs(out_dir, exist_ok=True)
        meta_path = args.metadata
        if not os.path.exists(meta_path):
            print("[-] Metadata file not found")
            sys.exit(1)

        with open(meta_path, 'r') as f:
            meta = json.load(f)
            
        qps = meta['qps']
        threshold = meta['qp_threshold']
        out_h264 = os.path.join(out_dir, f"{name}_decrypted.h264")
        selective_decrypt(enc_h264, out_h264, seeds[0], qps, threshold)
        print(f"[+] Decrypted video written to {out_h264}")


if __name__ == "__main__":
    main()
