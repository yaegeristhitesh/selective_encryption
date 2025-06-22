# 🛡️ SelectiveCrypt: Selective Encryption Tool for H.264 Video Streams

> **Project Code**: IITJAMMU\_24VI59IITJ
> **SRIB-PRISM Sponsored Research**

SelectiveCrypt provides a modular framework for **selectively encrypting H.264 video slices and AAC audio streams**, with support for both:

* A **C++-based encryption tool (Scheme1)** with Docker/VS Code container support
* A **Python-based selective slice/audio encryption tool (Scheme2)** with full Docker support

---

## 📆 Features

### ✅ Scheme 1 (C++ Based)

* 🔐 Encrypts frames using XOR + column permutation
* 🎥 Uses **OpenCV** and **FFmpeg**
* 🐳 Fully supports **Docker + VS Code Dev Containers**
* 💧 Built with `make` (cross-platform)

### ✅ Scheme 2 (Python Based)

* 🌟 **Selective slice encryption** based on QP threshold
* 🔊 **AAC audio encryption** using AES-CTR
* 📁 **.bin packaging** with metadata & base64-derived seed files
* 🔑 **Deterministic key/nonce** generation
* ↺ Fully reversible with CLI-based decryption
* 🛃 Full Docker compatibility (no Python or FFmpeg installs required locally)

---

## 💪 Quickstart (Docker + VS Code)

### ✅ 1. Prerequisites

* [Docker](https://www.docker.com/) installed and running
* [VS Code](https://code.visualstudio.com/) with:

  * `Remote - Containers` extension

---

### 🔧 2. Open the Project in a VS Code Container

1. Open your project folder in VS Code
2. Press `F1` → choose:

   ```
   Remote-Containers: Reopen in Container
   ```
3. VS Code will build the container and open your development environment

---

### 📂 3. Add Video Files

Place any test video inside the `video/` directory:

```
video/myclip.mp4
```

---

### ▶️ 4. Run Scheme1 (C++)

```bash
./run video/test2.mp4 video/scheme1_results/encrypted.mp4 video/scheme1_results/decrypted.mp4
```

It will prompt:

```
Enter encryption key (max 16 characters):
```
Might take some time to run (5 mins)
---

### ⚒️ 5. Run Scheme2 (Python) Inside Container

#### Encrypt

```bash
python3 encryption_schemes/scheme2/encrypt.py -i video/test1.mp4 -t 28 -o video/scheme2_results
```

#### Decrypt `.bin` Package

```bash
python3 encryption_schemes/scheme2/decrypt.py -p video/scheme2_results/test1.bin -s video/scheme2_results/test1_seeds.json
```

#### View encrypted video

```bash
 python3 encryption_schemes/scheme2/decrypt.py -p video/scheme2_results/test1.bin -s video/scheme2_results/test1_seeds.json -o output/ -k
```

---

## 🔧 Manual Python (Non-Docker)

### 📝 Requirements

```bash
pip install -r requirements.txt
```

* Python 3.8+
* FFmpeg + FFprobe in PATH
* `pycryptodome`

---

## 📁 Recommended Folder Structure

```
project-root/
├── codec/
├── encryption_schemes/
│   ├── scheme1/
│   └── scheme2/
├── video/
│   ├── test1.mp4
│   └── scheme1_results/
│   └── scheme2_results/
├── build/
├── run
├── Makefile
├── Dockerfile / devcontainer.json
└── README.md
```

---
