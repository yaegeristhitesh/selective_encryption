#include "scheme_1.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <random>       // For std::mt19937
#include <vector>

namespace fs = std::filesystem;

namespace Scheme1 {

// Helper: Compute a simple hash from the key (for RNG seeding).
unsigned int simpleHash(const std::string &key) {
    unsigned int hash = 0;
    for (char c : key)
        hash = hash * 101 + static_cast<unsigned int>(c);
    return hash;
}

// Helper: Generate a permutation (swap key) for the given width.
std::vector<int> generateSwapKey(int width, const std::string &key) {
    std::vector<int> perm(width);
    for (int i = 0; i < width; i++) {
        perm[i] = i;
    }
    unsigned int seed = simpleHash(key);
    std::mt19937 rng(seed); // now defined because <random> is included
    std::shuffle(perm.begin(), perm.end(), rng);
    return perm;
}

// Helper: Invert a permutation.
std::vector<int> invertPermutation(const std::vector<int> &perm) {
    std::vector<int> inv(perm.size());
    for (size_t i = 0; i < perm.size(); i++) {
        inv[perm[i]] = i;
    }
    return inv;
}

// Encrypt a single image.
cv::Mat encrypt_image(const cv::Mat &image, const std::string &key) {
    cv::Mat encrypted = image.clone();
    int rows = encrypted.rows;
    int cols = encrypted.cols;
    // Generate swap key (deterministic: same key and same width gives same permutation).
    std::vector<int> swapKey = generateSwapKey(cols, key);
    for (int i = 0; i < rows; i++) {
        // Process each row.
        std::vector<uchar> rowB(cols), rowG(cols), rowR(cols);
        for (int j = 0; j < cols; j++) {
            cv::Vec3b pixel = encrypted.at<cv::Vec3b>(i, j);
            // OpenCV uses BGR ordering.
            rowB[j] = pixel[0] ^ key[j % key.size()];
            rowG[j] = pixel[1] ^ key[j % key.size()];
            rowR[j] = pixel[2] ^ key[j % key.size()];
        }
        // Perform column swapping.
        std::vector<uchar> newRowB(cols), newRowG(cols), newRowR(cols);
        for (int j = 0; j < cols; j++) {
            newRowB[j] = rowB[swapKey[j]];
            newRowG[j] = rowG[swapKey[j]];
            newRowR[j] = rowR[swapKey[j]];
        }
        // Write back the swapped values.
        for (int j = 0; j < cols; j++) {
            encrypted.at<cv::Vec3b>(i, j)[0] = newRowB[j];
            encrypted.at<cv::Vec3b>(i, j)[1] = newRowG[j];
            encrypted.at<cv::Vec3b>(i, j)[2] = newRowR[j];
        }
    }
    return encrypted;
}

// Decrypt a single image.
cv::Mat decrypt_image(const cv::Mat &image, const std::string &key) {
    cv::Mat decrypted = image.clone();
    int rows = decrypted.rows;
    int cols = decrypted.cols;
    // Generate the same swap key and then compute its inverse.
    std::vector<int> swapKey = generateSwapKey(cols, key);
    std::vector<int> invSwap = invertPermutation(swapKey);
    for (int i = 0; i < rows; i++) {
        std::vector<uchar> rowB(cols), rowG(cols), rowR(cols);
        // Undo column swapping.
        for (int j = 0; j < cols; j++) {
            rowB[j] = decrypted.at<cv::Vec3b>(i, invSwap[j])[0];
            rowG[j] = decrypted.at<cv::Vec3b>(i, invSwap[j])[1];
            rowR[j] = decrypted.at<cv::Vec3b>(i, invSwap[j])[2];
        }
        // Undo XOR operation.
        for (int j = 0; j < cols; j++) {
            rowB[j] = rowB[j] ^ key[j % key.size()];
            rowG[j] = rowG[j] ^ key[j % key.size()];
            rowR[j] = rowR[j] ^ key[j % key.size()];
        }
        // Write back.
        for (int j = 0; j < cols; j++) {
            decrypted.at<cv::Vec3b>(i, j)[0] = rowB[j];
            decrypted.at<cv::Vec3b>(i, j)[1] = rowG[j];
            decrypted.at<cv::Vec3b>(i, j)[2] = rowR[j];
        }
    }
    return decrypted;
}

#include <iostream>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// Assume encrypt_image and decrypt_image are defined elsewhere
cv::Mat encrypt_image(const cv::Mat& frame, const std::string& key);
cv::Mat decrypt_image(const cv::Mat& frame, const std::string& key);

// Encrypt ALL frames from the video and rebuild a new, fully encrypted video.
int encrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key) {
    // Create temporary directories.
    fs::create_directory("temp_all_frames");
    fs::create_directory("temp_encrypted_frames");

    // --- Step 1: Extract ALL frames from the video ---
    // Note: It's good practice to get the original video's framerate for reassembly.
    // For simplicity here, we'll assume 25, but a robust solution would use ffprobe to get the actual rate.
    // A placeholder framerate of 25 is used in the final command.
    {
        std::string cmd = "ffmpeg -y -i " + videoPath + " temp_all_frames/frame_%04d.png";
        std::cout << "Executing: " << cmd << std::endl;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error extracting all frames." << std::endl;
            return -1;
        }
    }

    // --- Step 2: Encrypt each frame ---
    for (const auto &entry : fs::directory_iterator("temp_all_frames")) {
        if (entry.path().extension() == ".png") {
            cv::Mat frame = cv::imread(entry.path().string(), cv::IMREAD_COLOR);
            if (frame.empty()) continue;

            cv::Mat encFrame = encrypt_image(frame, key);
            std::string outPath = "temp_encrypted_frames/" + entry.path().filename().string();
            cv::imwrite(outPath, encFrame);
        }
    }

    // --- Step 3: Rebuild the video from encrypted frames, copying the original audio ---
    {
        // This command creates a new video from the encrypted image sequence
        // and copies the audio stream from the original video.
        std::string cmd = "ffmpeg -y -framerate 25 -i temp_encrypted_frames/frame_%04d.png "
                          "-i " + videoPath + " -map 0:v:0 -map 1:a:0? -c:a copy "
                          "-c:v libx264 -pix_fmt yuv420p " + outputPath;
        std::cout << "Executing: " << cmd << std::endl;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error rebuilding video from encrypted frames." << std::endl;
            return -1;
        }
    }

    // --- Step 4: Cleanup temporary directories ---
    fs::remove_all("temp_all_frames");
    fs::remove_all("temp_encrypted_frames");

    std::cout << "Encrypted video saved to " << outputPath << std::endl;
    return 0;
}

// Decrypt ALL frames from the video and rebuild a new, decrypted video.
int decrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key) {
    fs::create_directory("temp_enc_frames");
    fs::create_directory("temp_dec_frames");

    // --- Step 1: Extract all frames from the encrypted video ---
    {
        std::string cmd = "ffmpeg -y -i " + videoPath + " temp_enc_frames/frame_%04d.png";
        std::cout << "Executing: " << cmd << std::endl;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error extracting encrypted frames." << std::endl;
            return -1;
        }
    }

    // --- Step 2: Decrypt each frame ---
    for (const auto &entry : fs::directory_iterator("temp_enc_frames")) {
        if (entry.path().extension() == ".png") {
            cv::Mat encFrame = cv::imread(entry.path().string(), cv::IMREAD_COLOR);
            if (encFrame.empty()) continue;

            cv::Mat decFrame = decrypt_image(encFrame, key);
            std::string outPath = "temp_dec_frames/" + entry.path().filename().string();
            cv::imwrite(outPath, decFrame);
        }
    }

    // --- Step 3: Rebuild the video from decrypted frames, copying the audio ---
    {
        // We copy the audio from the encrypted video file.
        std::string cmd = "ffmpeg -y -framerate 25 -i temp_dec_frames/frame_%04d.png "
                          "-i " + videoPath + " -map 0:v:0 -map 1:a:0? -c:a copy "
                          "-c:v libx264 -pix_fmt yuv420p " + outputPath;
        std::cout << "Executing: " << cmd << std::endl;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "Error rebuilding video from decrypted frames." << std::endl;
            return -1;
        }
    }

    // --- Step 4: Cleanup ---
    fs::remove_all("temp_enc_frames");
    fs::remove_all("temp_dec_frames");
    std::cout << "Decrypted video saved to " << outputPath << std::endl;
    return 0;
}
}