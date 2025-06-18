#ifndef SCHEME1
#define SCHEME1

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace Scheme1 {
    // Encrypt and decrypt a single image.
    cv::Mat encrypt_image(const cv::Mat &image, const std::string &key);
    cv::Mat decrypt_image(const cv::Mat &image, const std::string &key);

    // Public functions to process I-frames from a video.
    int encrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key);
    int decrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key);
}

#endif // SCHEME1
