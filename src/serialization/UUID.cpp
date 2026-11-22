#include "UUID.h"

#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

namespace bird {

namespace {

int hexCharToNibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

std::mt19937_64& threadLocalEngine() {
	thread_local std::mt19937_64 engine = []() {
		std::random_device rd;
		const uint64_t timeSeed = static_cast<uint64_t>(
			std::chrono::high_resolution_clock::now().time_since_epoch().count());
		const uint64_t threadSeed =
			static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
		std::seed_seq seq{rd(), rd(), static_cast<uint32_t>(timeSeed),
						  static_cast<uint32_t>(timeSeed >> 32),
						  static_cast<uint32_t>(threadSeed),
						  static_cast<uint32_t>(threadSeed >> 32)};
		return std::mt19937_64(seq);
	}();
	return engine;
}

}  // namespace

UUID UUID::generate() {
	auto& rng = threadLocalEngine();
	uint64_t high = rng();
	uint64_t low = rng();

	// Set RFC 4122 version 4 (bits 12..15 of time_hi_and_version = 0100).
	high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
	// Set RFC 4122 variant 1 (top 2 bits of clock_seq_hi_and_reserved = 10).
	low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

	return UUID(high, low);
}

std::optional<UUID> UUID::tryParse(const std::string& str) {
	if (str.size() != 36) return std::nullopt;
	if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
		return std::nullopt;
	}

	uint64_t high = 0;
	uint64_t low = 0;
	int nibbleIndex = 0;

	for (size_t i = 0; i < str.size(); ++i) {
		if (i == 8 || i == 13 || i == 18 || i == 23) continue;
		const int nibble = hexCharToNibble(str[i]);
		if (nibble < 0) return std::nullopt;

		if (nibbleIndex < 16) {
			high = (high << 4) | static_cast<uint64_t>(nibble);
		} else {
			low = (low << 4) | static_cast<uint64_t>(nibble);
		}
		++nibbleIndex;
	}

	return UUID(high, low);
}

UUID UUID::fromString(const std::string& str) {
	auto parsed = tryParse(str);
	if (!parsed) {
		throw std::invalid_argument("UUID::fromString: invalid UUID '" + str + "'");
	}
	return *parsed;
}

std::string UUID::toString() const {
	static constexpr char kHex[] = "0123456789abcdef";
	std::string out(36, '-');

	auto writeHex = [&](size_t offset, uint64_t value, int digits) {
		for (int i = digits - 1; i >= 0; --i) {
			out[offset + static_cast<size_t>(i)] = kHex[value & 0xF];
			value >>= 4;
		}
	};

	writeHex(0, m_high >> 32, 8);
	writeHex(9, (m_high >> 16) & 0xFFFF, 4);
	writeHex(14, m_high & 0xFFFF, 4);
	writeHex(19, m_low >> 48, 4);
	writeHex(24, m_low & 0x0000FFFFFFFFFFFFULL, 12);

	return out;
}

}  // namespace bird
