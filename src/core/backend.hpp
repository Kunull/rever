#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

// File I/O
std::vector<uint8_t> load_file(const char* path);
bool save_file(const char* path, const uint8_t* data, size_t size);

// Disassembly
struct DisasmLine {
  uint64_t address;
  std::string bytes_hex;
  std::string mnemonic;
  std::string operands;
};
// arch: 0=x86_64, 1=x86_32, 2=ARM64, 3=ARM32, 4=MIPS
std::vector<DisasmLine> disassemble(const uint8_t* data, size_t size,
                                     uint64_t base_addr = 0, int arch = 0,
                                     int max_insns = 4000);

// Visualization
std::array<uint32_t, 256> byte_histogram(const uint8_t* data, size_t size);
std::vector<float> entropy_curve(const uint8_t* data, size_t size,
                                  int window = 256);
void bigram_image(const uint8_t* data, size_t size,
                  uint8_t* rgba_out); // 256x256 RGBA

// Strings extraction
struct FoundString {
  size_t offset;
  std::string value;
};
std::vector<FoundString> extract_strings(const uint8_t* data, size_t size,
                                          int min_len = 4);

// Magic / format detection
std::string detect_format(const uint8_t* data, size_t size);

// PE/ELF/Mach-O section parsing
struct Section {
  std::string name;
  uint64_t vaddr;
  uint64_t offset;
  uint64_t size;
  std::string flags; // e.g. "RX", "RW", "R"
};
std::vector<Section> parse_sections(const uint8_t* data, size_t size);

// Search
struct SearchHit { size_t offset; };
std::vector<SearchHit> search_hex(const uint8_t* data, size_t size,
                                   const uint8_t* pattern, size_t pat_len);
std::vector<SearchHit> search_text(const uint8_t* data, size_t size,
                                    const char* text,
                                    bool case_sensitive = true);

// Hashing
std::string hash_md5(const uint8_t* data, size_t size);
std::string hash_sha256(const uint8_t* data, size_t size);

// Data Inspector
struct DataInspectorResult {
  std::string type_name;
  std::string value;
};
std::vector<DataInspectorResult> inspect_data(const uint8_t* data, size_t size,
                                               size_t offset);

// Imports / Exports (PE & ELF)
struct ImportEntry {
  std::string library;
  std::string name;
  uint64_t hint;  // ordinal or hint
};
std::vector<ImportEntry> parse_imports(const uint8_t* data, size_t size);

struct ExportEntry {
  std::string name;
  uint64_t rva;
  int ordinal;
};
std::vector<ExportEntry> parse_exports(const uint8_t* data, size_t size);

// Cross-reference info for a file
struct FileInfo {
  std::string format;
  std::string arch;
  std::string bits;
  std::string endian;
  uint64_t entry_point;
  size_t file_size;
};
FileInfo get_file_info(const uint8_t* data, size_t size);
