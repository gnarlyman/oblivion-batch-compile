#pragma once

#include <cstdint>
#include <string>

namespace obc {

// Returns lowercase-hex SHA-256 of `data` (size bytes). Returns empty string
// on failure. Uses Windows BCrypt; caller does not need to initialize anything.
std::string Sha256Hex(const void* data, std::size_t size);

}  // namespace obc
