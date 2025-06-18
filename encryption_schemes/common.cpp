#include "common.h"
#include "scheme1/scheme_1.h"
#include "scheme2/scheme_2.h"

namespace Encryption {

int encrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme) {
    switch (scheme) {
        case Scheme::Scheme1:
            return Scheme1::encrypt(videoPath,outputPath,key);
            break;
        case Scheme::Scheme2:
            return Scheme2::encrypt(videoPath,outputPath,key);
            break;
        // Add more cases here for new schemes.
    }
    return 0;
}

int decrypt(const std::string &videoPath, const std::string &outputPath, const std::string &key, Scheme scheme) {
    switch (scheme) {
        case Scheme::Scheme1:
            return Scheme1::decrypt(videoPath,outputPath,key);
            break;
        case Scheme::Scheme2:
            return Scheme2::decrypt(videoPath,outputPath,key);
            break;
        // Add more cases here for new schemes.
    }
    return 0;
}

} // namespace Encryption
