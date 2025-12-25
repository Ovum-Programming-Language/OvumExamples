
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#define OVUM_EXPORT extern "C" __declspec(dllexport)
#else
#define OVUM_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define OVUM_FFI_FUNCTION(Name) \
  OVUM_EXPORT long long Name(   \
      void* input_ptr, unsigned long long input_len, void* output_ptr, unsigned long long output_len)

static inline bool IsPrimeUpTo48(unsigned v) {
  switch (v) {
    case 2:
    case 3:
    case 5:  // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 7:  // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 11: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 13: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 17: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 19: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 23: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 29: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 31: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 37: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 41: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 43: // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 47: // NOLINT cppcoreguidelines-avoid-magic-numbers
      return true;
    default:
      return false;
  }
}

static inline int HexVal(unsigned char c) {
  if (c >= '0' && c <= '9')
    return int(c - '0');
  if (c >= 'A' && c <= 'F')
    return int(c - 'A' + 10); // NOLINT cppcoreguidelines-avoid-magic-numbers
  return -1;
}

// Crockford Base32 alphabet: 0-9, A-Z without I L O U
static inline int Base32CrockfordVal(unsigned char c) {
  if (c >= '0' && c <= '9')
    return int(c - '0');

  // letters (uppercase only)
  switch (c) {
    case 'A':
      return 10; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'B':
      return 11; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'C':
      return 12; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'D':
      return 13; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'E':
      return 14; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'F':
      return 15; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'G':
      return 16; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'H':
      return 17; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'J':
      return 18; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'K':
      return 19; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'M':
      return 20; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'N':
      return 21; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'P':
      return 22; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'Q':
      return 23; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'R':
      return 24; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'S':
      return 25; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'T':
      return 26; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'V':
      return 27; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'W':
      return 28; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'X':
      return 29; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'Y':
      return 30; // NOLINT cppcoreguidelines-avoid-magic-numbers
    case 'Z':
      return 31; // NOLINT cppcoreguidelines-avoid-magic-numbers
    default:
      return -1;
  }
}

static inline void WriteInt64LE(void* output_ptr, std::int64_t value) {
  auto* out = static_cast<std::uint8_t*>(output_ptr);
  auto u = static_cast<std::uint64_t>(value);
  for (int i = 0; i < sizeof(std::int64_t); ++i) {
    out[i] = static_cast<std::uint8_t>((u >> (sizeof(std::int64_t) * i)) &
                                       0xFFu); // NOLINT cppcoreguidelines-avoid-magic-numbers
  }
}

static inline std::uint64_t Popcount64(std::uint64_t x) {
#if defined(__GNUG__) || defined(__clang__)
  return static_cast<std::uint64_t>(__builtin_popcountll(x));
#else
  // portable fallback
  std::uint64_t cnt = 0;
  while (x) {
    x &= (x - 1);
    ++cnt;
  }
  return cnt;
#endif
}

static bool CheckUid(const std::uint8_t* s, std::size_t n) {
  // Allow one trailing '\0' from some callers, ignore it.
  if (n > 0 && s[n - 1] == 0)
    n--;

  // Exact length: 29
  if (n != 29) // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;

  // "OVUM-"
  if (!(s[0] == 'O' && s[1] == 'V' && s[2] == 'U' && s[3] == 'M' && s[4] == '-'))
    return false;

  // HEX[12] at [5..16]
  std::uint64_t hex = 0;
  for (int i = 0; i < 12; ++i) { // NOLINT cppcoreguidelines-avoid-magic-numbers
    int hv = HexVal(s[5 + i]);   // NOLINT cppcoreguidelines-avoid-magic-numbers
    if (hv < 0)
      return false;
    hex = (hex << 4) | static_cast<std::uint64_t>(hv);
  }

  // '-' at 17
  if (s[17] != '-') // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;

  // BASE32[8] at [18..25]
  std::uint64_t base = 0; // NOLINT cppcoreguidelines-avoid-magic-numbers
  for (int i = 0; i < sizeof(std::uint64_t); ++i) {
    int bv = Base32CrockfordVal(s[18 + i]); // NOLINT cppcoreguidelines-avoid-magic-numbers
    if (bv < 0)
      return false;
    base = (base << 5) | static_cast<std::uint64_t>(bv); // NOLINT cppcoreguidelines-avoid-magic-numbers
  }
  // B is 40-bit (fits)

  // '-' at 26
  if (s[26] != '-') // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;

  // checksum digits at [27..28]
  if (!(s[27] >= '0' && s[27] <= '9' && s[28] >= '0' && s[28] <= '9')) // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;
  int CHK = int(s[27] - '0') * 10 + int(s[28] - '0'); // NOLINT cppcoreguidelines-avoid-magic-numbers

  // Complex rule:
  // 1) popcount(H) is prime
  auto pc = static_cast<unsigned>(Popcount64(hex));
  if (!IsPrimeUpTo48(pc))
    return false;

  // 2) mod 997 match
  if ((base % 997u) != (hex % 997u)) // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;

  // 3) last nibble of (H + B) is A
  if (((hex + base) & 0xFu) != 0xAu) // NOLINT cppcoreguidelines-avoid-magic-numbers
    return false;

  // 4) checksum = (H xor B) % 97, 0..96 (two digits allowed: 00..96)
  int expected = int((hex ^ base) % 97u); // NOLINT cppcoreguidelines-avoid-magic-numbers
  if (CHK != expected)
    return false;

  return true;
}

OVUM_FFI_FUNCTION(IsCorrectId) {
  output_ptr = reinterpret_cast<char*>(output_ptr) + sizeof(std::int64_t); // consume descriptor for Int
  output_len -= sizeof(std::int64_t);
  if (!input_ptr || input_len == 0)
    return 1;
  if (!output_ptr || output_len < sizeof(std::int64_t))
    return 1;

  const auto* input_bytes = static_cast<const std::uint8_t*>(input_ptr);
  if (!CheckUid(input_bytes, static_cast<std::size_t>(input_len)))
    return 1;

  // unix time seconds
  using namespace std::chrono;
  std::int64_t current_nanotime = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
  std::int64_t answer =
      (current_nanotime ^ 0xA12BC3456DE89F70ll) / 997ll + 239ll; // NOLINT cppcoreguidelines-avoid-magic-numbers

  WriteInt64LE(output_ptr, answer);
  return 0;
}

// Optional alias to satisfy "DoSomething" fixed entrypoint signature.
OVUM_FFI_FUNCTION(DoSomething) {
  return IsCorrectId(input_ptr, input_len, output_ptr, output_len);
}
