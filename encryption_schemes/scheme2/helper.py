import os
import subprocess
import sys
from hashlib import sha256

from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes


def generate_key_nonce(seed: str, index: int) -> tuple[bytes, bytes]:
    """
    Generate a deterministic AES key and nonce from a seed and index.

    Args:
        seed (str): Seed string for key derivation.
        index (int): Index to differentiate key/nonce generation.

    Returns:
        tuple[bytes, bytes]: A tuple containing 32-byte key and 8-byte nonce.
    """
    data = f"{seed}_{index}".encode()
    key = sha256(data).digest()
    nonce = sha256(data[::-1]).digest()[:8]
    return key, nonce


def has_audio(input_mp4: str) -> bool:
    """
    Check whether the input file has an audio stream (AAC or others).

    Args:
        input_mp4 (str): Path to the media file.

    Returns:
        bool: True if at least one audio stream exists, False otherwise.
    """
    try:
        result = subprocess.run(
            [
                "ffprobe", "-v", "error",
                "-select_streams", "a",
                "-show_entries", "stream=index",
                "-of", "csv=p=0",
                input_mp4
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=True
        )
        return bool(result.stdout.strip())
    except subprocess.CalledProcessError:
        return False


def extract_audio(input_mp4: str, output_aac: str) -> bool:
    """
    Extract AAC audio from an MP4 file without re-encoding, if present.

    Args:
        input_mp4 (str): Path to source MP4 file.
        output_aac (str): Path to save extracted AAC stream.

    Returns:
        bool: True if extraction succeeded, False otherwise or if no audio stream.
    """
    if not has_audio(input_mp4):
        print(f"No audio stream detected in {input_mp4}, skipping audio extraction.")
        return False
    try:
        subprocess.run(
            ["ffmpeg", "-y", "-i", input_mp4, "-vn", "-acodec", "copy", output_aac],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True
        )
        return True
    except subprocess.CalledProcessError as e:
        print(f"FFmpeg audio extraction failed: {e}. Skipping audio encryption.")
        return False


def mux_audio_video(input_h264: str, input_aac: str, output_mp4: str) -> None:
    """
    Mux H.264 video stream and AAC audio into a single MP4 container.

    Args:
        input_h264 (str): Path to raw H.264 annex-B stream.
        input_aac (str): Path to AAC audio file.
        output_mp4 (str): Path for the output MP4 file.
    """
    subprocess.run(
        [
            "ffmpeg", "-y", "-i", input_h264, "-i", input_aac,
            "-c:v", "copy", "-c:a", "aac", "-strict", "experimental", output_mp4,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    print(f"[+] Muxed {input_h264} and {input_aac} into {output_mp4}")


def encrypt_audio(input_aac: str, output_aac_enc: str, seed: str) -> None:
    """
    Encrypt an AAC audio file using AES-CTR mode with a deterministic key.

    Args:
        input_aac (str): Path to plaintext AAC audio.
        output_aac_enc (str): Path to save encrypted audio.
        seed (str): Seed for key derivation.
    """
    audio_data = open(input_aac, "rb").read()
    key, nonce = generate_key_nonce(seed, -1)
    cipher = AES.new(key, AES.MODE_CTR, nonce=nonce)
    encrypted = cipher.encrypt(audio_data)

    with open(output_aac_enc, "wb") as f:
        f.write(encrypted)


def decrypt_audio(input_aac_enc: str, output_aac: str, seed: str) -> None:
    """
    Decrypt an encrypted AAC file produced by encrypt_audio.

    Args:
        input_aac_enc (str): Path to encrypted AAC audio.
        output_aac (str): Path to save decrypted audio.
        seed (str): Seed used for encryption key derivation.
    """
    encrypted = open(input_aac_enc, "rb").read()
    key, nonce = generate_key_nonce(seed, -1)
    cipher = AES.new(key, AES.MODE_CTR, nonce=nonce)
    decrypted = cipher.decrypt(encrypted)

    with open(output_aac, "wb") as f:
        f.write(decrypted)


def get_annexb_stream(input_mp4: str, output_h264: str) -> None:
    """
    Extract raw H.264 bitstream in annex-B format from MP4 container.

    Args:
        input_mp4 (str): Path to source MP4 file.
        output_h264 (str): Path to save annex-B H.264 stream.
    """
    subprocess.run(
        [
            "ffmpeg", "-y", "-i", input_mp4,
            "-c:v", "copy", "-bsf:v", "h264_mp4toannexb", "-an", output_h264,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def remux_to_mp4(input_h264: str, output_mp4: str) -> None:
    """
    Remux a raw H.264 annex-B stream into an MP4 container without re-encoding.

    Args:
        input_h264 (str): Path to annex-B H.264 stream.
        output_mp4 (str): Path for the output MP4.
    """
    subprocess.run(
        ["ffmpeg", "-y", "-i", input_h264, "-c:v", "copy", output_mp4],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def parse_nal_units(bytestream: bytes) -> list[bytes]:
    """
    Split an H.264 byte stream into individual NAL units.

    Args:
        bytestream (bytes): Raw bytes of an annex-B formatted H.264 stream.

    Returns:
        list[bytes]: List of NAL unit byte sequences.
    """
    nals = []
    i = 0
    length = len(bytestream)

    while i < length:
        if bytestream[i:i + 4] == b"\x00\x00\x00\x01":
            start = i
            i += 4
        elif bytestream[i:i + 3] == b"\x00\x00\x01":
            start = i
            i += 3
        else:
            i += 1
            continue

        next3 = bytestream.find(b"\x00\x00\x01", i)
        next4 = bytestream.find(b"\x00\x00\x00\x01", i)
        candidates = [p for p in (next3, next4) if p != -1]
        end = min(candidates) if candidates else length

        nals.append(bytestream[start:end])
        i = end

    return nals


def get_nal_type_name(nal_type: int) -> str:
    """
    Map a numeric NAL unit type to a human-readable name.

    Args:
        nal_type (int): NAL unit type identifier.

    Returns:
        str: Human-friendly name or default format.
    """
    mapping = {
        1: "Non-IDR Slice",
        5: "IDR Slice (I-frame)",
        6: "SEI",
        7: "SPS (Seq Param Set)",
        8: "PPS (Pic Param Set)",
        9: "Access Unit Delimiter",
    }
    return mapping.get(nal_type, f"Unknown (type {nal_type})")


def extract_rbsp(payload: bytes) -> bytes:
    """
    Remove H.264 emulation prevention bytes from RBSP payload.

    Args:
        payload (bytes): Raw NAL payload including emulation prevention bytes.

    Returns:
        bytes: Cleaned RBSP data.
    """
    rbsp = bytearray()
    i = 0
    length = len(payload)

    while i < length:
        if i + 2 < length and payload[i:i + 2] == b"\x00\x00" and payload[i + 2] == 0x03:
            rbsp.extend(payload[i : i + 2])
            i += 3
        else:
            rbsp.append(payload[i])
            i += 1

    return bytes(rbsp)


def insert_emulation_prevention(rbsp: bytes) -> bytes:
    """
    Insert emulation prevention bytes into RBSP data.

    Args:
        rbsp (bytes): Raw RBSP data.

    Returns:
        bytes: RBSP with emulation prevention bytes added.
    """
    out = bytearray()
    zero_count = 0

    for byte in rbsp:
        if zero_count == 2 and byte <= 3:
            out.append(3)
            zero_count = 0
        out.append(byte)
        zero_count = zero_count + 1 if byte == 0 else 0

    return bytes(out)


def cleanup(temp_files: list[str], folder: str = '.') -> None:
    """
    Remove temporary files generated during processing.

    Args:
        temp_files (list[str]): List of filenames to delete.
        folder (str): Directory where the temp files reside.
    """
    temp_files = temp_files + ['temp_input_stream.h264']
    for fname in temp_files:
        path = os.path.join(folder, fname)
        try:
            os.remove(path)
            print(f"Removed temporary file: {fname}")
        except FileNotFoundError:
            pass
        except PermissionError:
            print(f"Failed to remove temporary file: {fname}")