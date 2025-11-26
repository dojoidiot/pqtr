#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip> // For std::hex, std::setw
#include <byteswap.h> // For __bswap_16, __bswap_32 (GCC/Clang specific)

// TIFF Header structure
struct TiffHeader {
    uint16_t byteOrder;    // 0x4949 for Intel (little-endian), 0x4D4D for Motorola (big-endian)
    uint16_t version;      // 0x002A for TIFF, 0x002B for BigTIFF
    uint32_t ifdOffset;    // Offset of the first Image File Directory
};

// TIFF Tag data types
enum TiffDataType : uint16_t {
    BYTE = 1,
    ASCII = 2,
    SHORT = 3,
    LONG = 4,
    RATIONAL = 5,
    SBYTE = 6,
    UNDEFINED = 7,
    SSHORT = 8,
    SLONG = 9,
    SRATIONAL = 10,
    FLOAT = 11,
    DOUBLE = 12
};

// TIFF IFD Entry structure
struct IfdEntry {
    uint16_t tagId;
    uint16_t dataType;
    uint32_t count;
    uint32_t offsetOrValue; // Value if fits, otherwise offset to value
};

// Function to swap bytes for 16-bit value
uint16_t swapBytes(uint16_t val) {
    return (val << 8) | (val >> 8);
}

// Function to swap bytes for 32-bit value
uint32_t swapBytes(uint32_t val) {
    return (val << 24) | ((val & 0x00FF0000) >> 8) | ((val & 0x0000FF00) << 8) | (val >> 24);
}

// Function to read data from a file with endianness handling
template <typename T>
T readValue(std::ifstream& file, bool isLittleEndian) {
    T value;
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!isLittleEndian) {
        // If file is Big-endian and host is Little-endian, swap bytes
        if (sizeof(T) == 2) {
            value = swapBytes(static_cast<uint16_t>(value));
        } else if (sizeof(T) == 4) {
            value = swapBytes(static_cast<uint32_t>(value));
        }
    }
    return value;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file_path>" << std::endl;
        return 1;
    }

    std::string rawFilePath = argv[1];
    std::ifstream file(rawFilePath, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << rawFilePath << std::endl;
        return 1;
    }

    TiffHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(TiffHeader));

    bool isLittleEndian = false;
    if (header.byteOrder == 0x4949) { // 'II'
        isLittleEndian = true;
    } else if (header.byteOrder == 0x4D4D) { // 'MM'
        isLittleEndian = false; // Big-endian
    } else {
        std::cerr << "Error: Invalid TIFF byte order: 0x" << std::hex << header.byteOrder << std::endl;
        return 1;
    }

    // Adjust for endianness
    if (!isLittleEndian) {
        // Assume host is little-endian, so swap if file is big-endian
        header.version = swapBytes(header.version);
        header.ifdOffset = swapBytes(header.ifdOffset);
    }

    if (header.version != 0x002A) { // Standard TIFF version
        std::cerr << "Error: Not a standard TIFF file (version: 0x" << std::hex << header.version << ")" << std::endl;
        return 1;
    }

    std::cout << "TIFF Header Parsed:" << std::endl;
    std::cout << "  Byte Order: " << (isLittleEndian ? "Little-endian (0x4949)" : "Big-endian (0x4D4D)") << std::endl;
    std::cout << "  Version: 0x" << std::hex << std::setw(4) << std::setfill('0') << header.version << std::endl;
    std::cout << "  First IFD Offset: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.ifdOffset << std::endl;

    // Now, parse the first IFD
    file.seekg(header.ifdOffset);
    uint16_t numEntries = readValue<uint16_t>(file, isLittleEndian);

    std::cout << "\nFirst IFD (" << numEntries << " entries):" << std::endl;

    for (int i = 0; i < numEntries; ++i) {
        IfdEntry entry;
        entry.tagId = readValue<uint16_t>(file, isLittleEndian);
        entry.dataType = readValue<uint16_t>(file, isLittleEndian);
        entry.count = readValue<uint32_t>(file, isLittleEndian);
        entry.offsetOrValue = readValue<uint32_t>(file, isLittleEndian);

        // For debugging, print basic info about each tag
        std::cout << "  Tag ID: 0x" << std::hex << std::setw(4) << std::setfill('0') << entry.tagId
                  << ", Data Type: " << std::dec << entry.dataType
                  << ", Count: " << entry.count
                  << ", Offset/Value: 0x" << std::hex << std::setw(8) << std::setfill('0') << entry.offsetOrValue << std::endl;
    }

    file.close();
    return 0;
}