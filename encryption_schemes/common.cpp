#include "common.h"
#include "scheme1/scheme_1.h"

namespace Encryption {

int encrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme) {
    return Scheme1::encrypt(videoPath,outputPath,key);
    return 0;
}

int decrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme) {
    return Scheme1::decrypt(videoPath,outputPath,key);
    return 0;
}

} // namespace Encryption
