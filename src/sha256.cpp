#include "sha256.h"

#include <Windows.h>
#include <bcrypt.h>

namespace obc {

std::string Sha256Hex(const void* data, std::size_t size) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return {};
    }
    BCRYPT_HASH_HANDLE h = nullptr;
    std::string out;
    if (BCRYPT_SUCCESS(BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0))) {
        if (BCRYPT_SUCCESS(BCryptHashData(h,
                                          const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(data)),
                                          static_cast<ULONG>(size), 0))) {
            UCHAR digest[32] = {};
            if (BCRYPT_SUCCESS(BCryptFinishHash(h, digest, sizeof(digest), 0))) {
                out.resize(64);
                static const char kHex[] = "0123456789abcdef";
                for (int i = 0; i < 32; ++i) {
                    out[2 * i] = kHex[digest[i] >> 4];
                    out[2 * i + 1] = kHex[digest[i] & 0xF];
                }
            }
        }
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

}  // namespace obc
