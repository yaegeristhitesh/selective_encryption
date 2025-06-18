#include "compress.h"
#include "decompress.h"
#include "encryption_schemes/common.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: ./run <input.mp4> <encrypted_output.mp4> <decrypted_output.mp4>" << std::endl;
        return EXIT_FAILURE;
    }

    const char* encodeInput = argv[1];
    const char* encryptedOutput = argv[2];
    const char* decryptedOutput = argv[3];

    const char* encodeOutput = "./video/output/temp_encoded.mp4";

    Encryption::Scheme current = Encryption::Scheme::Scheme2;

    if (encode_video(encodeInput, encodeOutput) < 0) {
        std::cerr << "Encoding failed" << std::endl;
        return EXIT_FAILURE;
    }

    std::string key;
    std::cout << "Enter encryption key (max 16 characters): ";
    std::getline(std::cin, key);
    if (key.size() > 16) key = key.substr(0, 16);

    if (Encryption::encrypt(encodeOutput, encryptedOutput, key, current) != 0) {
        std::cerr << "Encryption failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (Encryption::decrypt(encryptedOutput, decryptedOutput, key, current) != 0) {
        std::cerr << "Decryption failed" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}