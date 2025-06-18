#ifndef ENCRYPTION_COMMON_H
#define ENCRYPTION_COMMON_H

#include <string>
#include <opencv2/opencv.hpp>

// Enumerate available schemes.
namespace Encryption {
    enum class Scheme {
        Scheme1,
        Scheme2,
        // Add more schemes here as needed.
    };

    // Encryption and decryption functions
    int encrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme);
    int decrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme);
}

#endif // ENCRYPTION_COMMON_H