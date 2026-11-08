#include "vagedc15/EDC15/EDC15File.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
    constexpr size_t kFlashSize = EDC15File::flashSize;

    // Exposes the protected API for testing
    class TestableEDC15File : public EDC15File {
    public:
        using EDC15File::EDC15File;
        using EDC15File::isEmptyBlock;
    };

    // A blank flash image, 0xFF like an erased flash
    std::vector<uint8_t> blankImage(uint8_t fill = 0xFF)
    {
        return std::vector<uint8_t>(kFlashSize, fill);
    }

    void putBytes(std::vector<uint8_t>& image, size_t pos, std::initializer_list<uint8_t> bytes)
    {
        std::copy(bytes.begin(), bytes.end(), image.begin() + pos);
    }

    void putString(std::vector<uint8_t>& image, size_t pos, std::string_view text)
    {
        std::copy(text.begin(), text.end(), image.begin() + pos);
    }

    // Writes a zero terminated string with a zero right before it, like in a real flash
    void putCString(std::vector<uint8_t>& image, size_t pos, std::string_view text)
    {
        image[pos - 1] = 0x00;
        putString(image, pos, text);
        image[pos + text.size()] = 0x00;
    }

    void writeBytes(const fs::path& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    fs::path binaryPath(std::string_view name)
    {
        return fs::path{ TEST_DATA_DIR } / "binaries" / name;
    }

    // Each test gets its own temp directory, removed afterwards
    class EDC15FileTest : public ::testing::Test {
    protected:
        fs::path tmpDir;

        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            tmpDir = fs::temp_directory_path() / std::format(
                "vagedc15_{}_{}", info->test_suite_name(), info->name()
            );
            fs::remove_all(tmpDir);
            fs::create_directories(tmpDir);
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(tmpDir, ec);
        }

        fs::path writeImage(const std::vector<uint8_t>& image, std::string_view name = "image.bin")
        {
            const auto path = tmpDir / name;
            writeBytes(path, image);
            return path;
        }

        EDC15File load(const std::vector<uint8_t>& image)
        {
            return EDC15File{ writeImage(image) };
        }
    };

    using EDC15FileSearchTest = EDC15FileTest;
    using EDC15FileWordTest = EDC15FileTest;
    using EDC15FileInfosTest = EDC15FileTest;
    using EDC15FileIOTest = EDC15FileTest;
}

// ============================================================================
// Construction
// ============================================================================

TEST_F(EDC15FileTest, LoadsValidImage)
{
    auto image = blankImage();
    putBytes(image, 0, { 0x12, 0x34 });
    putBytes(image, kFlashSize - 1, { 0x56 });

    const auto file = load(image);

    EXPECT_EQ(file.getRealFlashSize(), kFlashSize);
    EXPECT_EQ(file.getRealFlashSizeAsWord(), kFlashSize / 2);
    EXPECT_EQ(file.data(), image);
}

TEST_F(EDC15FileTest, ThrowsOnSmallerImage)
{
    const auto path = writeImage(std::vector<uint8_t>(kFlashSize - 1, 0xFF));
    EXPECT_THROW(EDC15File{ path }, std::runtime_error);
}

TEST_F(EDC15FileTest, ThrowsOnBiggerImage)
{
    const auto path = writeImage(std::vector<uint8_t>(kFlashSize + 1, 0xFF));
    EXPECT_THROW(EDC15File{ path }, std::runtime_error);
}

TEST_F(EDC15FileTest, ThrowsOnEmptyImage)
{
    const auto path = writeImage({});
    EXPECT_THROW(EDC15File{ path }, std::runtime_error);
}

TEST_F(EDC15FileTest, ThrowsOnMissingFile)
{
    EXPECT_THROW(EDC15File{ tmpDir / "does_not_exist.bin" }, std::runtime_error);
}

TEST_F(EDC15FileTest, LoadsRealBinaries)
{
    EXPECT_NO_THROW(EDC15File{ binaryPath("012fk.bin") });
    EXPECT_NO_THROW(EDC15File{ binaryPath("038906019NJ.bin") });
}

TEST_F(EDC15FileTest, DataIsMutable)
{
    auto file = load(blankImage());
    file.data()[0x100] = 0x42;
    EXPECT_EQ(file.data()[0x100], 0x42);
}

// ============================================================================
// parsePattern
// ============================================================================

TEST(EDC15FileParsePatternTest, ParsesSpaceSeparatedBytes)
{
    const auto pat = EDC15File::parsePattern("45 44 43 00 FF");

    ASSERT_EQ(pat.size(), 5u);
    const uint8_t expected[]{ 0x45, 0x44, 0x43, 0x00, 0xFF };
    for (size_t i = 0; i < pat.size(); ++i) {
        EXPECT_EQ(pat[i].value, expected[i]) << "at index " << i;
        EXPECT_FALSE(pat[i].wildcard) << "at index " << i;
    }
}

TEST(EDC15FileParsePatternTest, ParsesWithoutSpaces)
{
    const auto pat = EDC15File::parsePattern("4544FF");

    ASSERT_EQ(pat.size(), 3u);
    EXPECT_EQ(pat[0].value, 0x45);
    EXPECT_EQ(pat[1].value, 0x44);
    EXPECT_EQ(pat[2].value, 0xFF);
}

TEST(EDC15FileParsePatternTest, AcceptsLowercaseAndMixedCaseHex)
{
    const auto pat = EDC15File::parsePattern("ab Cd eF");

    ASSERT_EQ(pat.size(), 3u);
    EXPECT_EQ(pat[0].value, 0xAB);
    EXPECT_EQ(pat[1].value, 0xCD);
    EXPECT_EQ(pat[2].value, 0xEF);
}

TEST(EDC15FileParsePatternTest, IgnoresExtraSpaces)
{
    const auto pat = EDC15File::parsePattern("   12    34  ");

    ASSERT_EQ(pat.size(), 2u);
    EXPECT_EQ(pat[0].value, 0x12);
    EXPECT_EQ(pat[1].value, 0x34);
}

TEST(EDC15FileParsePatternTest, ParsesAllWildcardSpellings)
{
    const auto pat = EDC15File::parsePattern("XX xx ?? 12");

    ASSERT_EQ(pat.size(), 4u);
    EXPECT_TRUE(pat[0].wildcard);
    EXPECT_TRUE(pat[1].wildcard);
    EXPECT_TRUE(pat[2].wildcard);
    EXPECT_FALSE(pat[3].wildcard);
    EXPECT_EQ(pat[3].value, 0x12);
}

TEST(EDC15FileParsePatternTest, EmptyPatternGivesEmptyVector)
{
    EXPECT_TRUE(EDC15File::parsePattern("").empty());
    EXPECT_TRUE(EDC15File::parsePattern("    ").empty());
}

TEST(EDC15FileParsePatternTest, ThrowsOnOddDigitCount)
{
    EXPECT_THROW(EDC15File::parsePattern("4"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("45 4"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("454"), std::invalid_argument);
}

TEST(EDC15FileParsePatternTest, ThrowsOnSplitPair)
{
    // "4 5" is not a byte, the pair "4 " is invalid
    EXPECT_THROW(EDC15File::parsePattern("4 5"), std::invalid_argument);
}

TEST(EDC15FileParsePatternTest, ThrowsOnInvalidHex)
{
    EXPECT_THROW(EDC15File::parsePattern("4G"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("ZZ"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("-1"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("+1"), std::invalid_argument);
}

TEST(EDC15FileParsePatternTest, ThrowsOnMixedCaseWildcard)
{
    EXPECT_THROW(EDC15File::parsePattern("Xx"), std::invalid_argument);
    EXPECT_THROW(EDC15File::parsePattern("?X"), std::invalid_argument);
}

// ============================================================================
// containerToPatternByte
// ============================================================================

TEST(EDC15FileContainerToPatternTest, ConvertsEveryByteWithoutWildcard)
{
    std::vector<uint8_t> bytes{ 0x00, 0x7F, 0xFF };
    const auto pat = EDC15File::containerToPatternByte(bytes);

    ASSERT_EQ(pat.size(), bytes.size());
    for (size_t i = 0; i < pat.size(); ++i) {
        EXPECT_EQ(pat[i].value, bytes[i]);
        EXPECT_FALSE(pat[i].wildcard);
    }
}

TEST(EDC15FileContainerToPatternTest, EmptyContainerGivesEmptyPattern)
{
    std::vector<uint8_t> bytes;
    EXPECT_TRUE(EDC15File::containerToPatternByte(bytes).empty());
}

// ============================================================================
// findPattern - forward
// ============================================================================

TEST_F(EDC15FileSearchTest, ForwardFindsPattern)
{
    auto image = blankImage();
    putBytes(image, 0x1234, { 0xAA, 0xBB, 0xCC });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("AA BB CC", 0), 0x1234u);
}

TEST_F(EDC15FileSearchTest, ForwardReturnsFirstMatch)
{
    auto image = blankImage();
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("AA BB", 0), 0x1000u);
}

TEST_F(EDC15FileSearchTest, ForwardFromIsInclusive)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("AA BB", 0x1000), 0x1000u);
    EXPECT_EQ(file.findPattern("AA BB", 0x1001), 0x2000u);
}

TEST_F(EDC15FileSearchTest, ForwardToIsInclusiveOnStartPosition)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("AA BB", 0, 0x1000), 0x1000u);
    EXPECT_FALSE(file.findPattern("AA BB", 0, 0x0FFF));
}

TEST_F(EDC15FileSearchTest, ForwardFindsAtStartAndEndOfFlash)
{
    auto image = blankImage();
    putBytes(image, 0, { 0x11, 0x22 });
    putBytes(image, kFlashSize - 2, { 0x33, 0x44 });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("11 22", 0), 0u);
    EXPECT_EQ(file.findPattern("33 44", 0), kFlashSize - 2);
}

TEST_F(EDC15FileSearchTest, ForwardNotFound)
{
    const auto file = load(blankImage());
    EXPECT_FALSE(file.findPattern("12 34", 0));
}

TEST_F(EDC15FileSearchTest, ForwardFromTooCloseToEndReturnsNullopt)
{
    const auto file = load(blankImage());

    // A 2-byte pattern cannot start at the last byte
    EXPECT_FALSE(file.findPattern("FF FF", kFlashSize - 1));
}

TEST_F(EDC15FileSearchTest, ForwardFromAfterToThrows)
{
    const auto file = load(blankImage());
    EXPECT_THROW(file.findPattern("12", 0x2000, 0x1000), std::invalid_argument);
}

TEST_F(EDC15FileSearchTest, WildcardsMatchAnyByte)
{
    auto image = blankImage();
    putBytes(image, 0x3000, { 0xAA, 0x01, 0xCC });
    putBytes(image, 0x4000, { 0xAA, 0x02, 0xCC });
    const auto file = load(image);

    EXPECT_EQ(file.findPattern("AA XX CC", 0), 0x3000u);
    EXPECT_EQ(file.findPattern("AA ?? CC", 0x3001), 0x4000u);
}

TEST_F(EDC15FileSearchTest, StringAndPatternByteOverloadsAgree)
{
    auto image = blankImage();
    putBytes(image, 0x5555, { 0xDE, 0xAD, 0xBE, 0xEF });
    const auto file = load(image);

    std::vector<uint8_t> bytes{ 0xDE, 0xAD, 0xBE, 0xEF };
    const auto pat = EDC15File::containerToPatternByte(bytes);

    EXPECT_EQ(file.findPattern(pat, 0), file.findPattern("DE AD BE EF", 0));
    EXPECT_EQ(file.findPattern(pat, 0), 0x5555u);
}

// ============================================================================
// findPattern - argument validation
// ============================================================================

TEST_F(EDC15FileSearchTest, EmptyPatternThrows)
{
    const auto file = load(blankImage());

    EXPECT_THROW(file.findPattern("", 0), std::invalid_argument);
    EXPECT_THROW(file.findPattern(std::vector<PatternByte>{}, 0), std::invalid_argument);
}

TEST_F(EDC15FileSearchTest, PatternBiggerThanFlashThrows)
{
    const auto file = load(blankImage());
    const std::vector<PatternByte> pat(kFlashSize + 1, PatternByte{ 0xFF, false });

    EXPECT_THROW(file.findPattern(pat, 0), std::invalid_argument);
}

TEST_F(EDC15FileSearchTest, FromOutOfRangeThrows)
{
    const auto file = load(blankImage());

    EXPECT_THROW(file.findPattern("FF", kFlashSize), std::invalid_argument);
    EXPECT_THROW(
        file.findPattern("FF", kFlashSize, std::nullopt, SearchDirection::Backward),
        std::invalid_argument
    );
}

TEST_F(EDC15FileSearchTest, ToOutOfRangeThrows)
{
    const auto file = load(blankImage());
    EXPECT_THROW(file.findPattern("FF", 0, kFlashSize), std::invalid_argument);
}

// ============================================================================
// findPattern - backward
// ============================================================================

TEST_F(EDC15FileSearchTest, BackwardFromIsExclusiveEnd)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    const auto file = load(image);

    // The match must end at or before `from`
    EXPECT_EQ(
        file.findPattern("AA BB", 0x1002, std::nullopt, SearchDirection::Backward),
        0x1000u
    );
    EXPECT_FALSE(
        file.findPattern("AA BB", 0x1001, std::nullopt, SearchDirection::Backward)
    );
}

TEST_F(EDC15FileSearchTest, BackwardReturnsClosestMatch)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    const auto file = load(image);

    EXPECT_EQ(
        file.findPattern("AA BB", 0x3000, std::nullopt, SearchDirection::Backward),
        0x2000u
    );
}

TEST_F(EDC15FileSearchTest, BackwardToIsInclusiveLowerBound)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    const auto file = load(image);

    EXPECT_EQ(
        file.findPattern("AA BB", 0x3000, 0x1000, SearchDirection::Backward),
        0x1000u
    );
    EXPECT_FALSE(
        file.findPattern("AA BB", 0x3000, 0x1001, SearchDirection::Backward)
    );
}

TEST_F(EDC15FileSearchTest, BackwardFindsAtStartOfFlash)
{
    auto image = blankImage();
    putBytes(image, 0, { 0x11, 0x22 });
    const auto file = load(image);

    EXPECT_EQ(
        file.findPattern("11 22", 0x100, std::nullopt, SearchDirection::Backward),
        0u
    );
}

TEST_F(EDC15FileSearchTest, BackwardFromSmallerThanPatternReturnsNullopt)
{
    const auto file = load(blankImage());
    EXPECT_FALSE(file.findPattern("FF FF FF", 2, std::nullopt, SearchDirection::Backward));
}

TEST_F(EDC15FileSearchTest, BackwardBoundAboveStartThrows)
{
    const auto file = load(blankImage());
    EXPECT_THROW(
        file.findPattern("FF FF", 0x1000, 0x2000, SearchDirection::Backward),
        std::invalid_argument
    );
}

// ============================================================================
// findPatterns
// ============================================================================

TEST_F(EDC15FileSearchTest, FindPatternsForwardReturnsAllInOrder)
{
    auto image = blankImage();
    putBytes(image, 0x3000, { 0xAA, 0xBB });
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, kFlashSize - 2, { 0xAA, 0xBB });
    const auto file = load(image);

    const std::vector<size_t> expected{ 0x1000, 0x3000, kFlashSize - 2 };
    EXPECT_EQ(file.findPatterns("AA BB", 0), expected);
}

TEST_F(EDC15FileSearchTest, FindPatternsForwardIncludesOverlappingMatches)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xAA, 0xAA });
    const auto file = load(image);

    const std::vector<size_t> expected{ 0x1000, 0x1001 };
    EXPECT_EQ(file.findPatterns("AA AA", 0), expected);
}

TEST_F(EDC15FileSearchTest, FindPatternsNoMatchGivesEmptyVector)
{
    const auto file = load(blankImage());

    EXPECT_TRUE(file.findPatterns("12 34", 0).empty());
    EXPECT_TRUE(
        file.findPatterns("12 34", kFlashSize - 1, std::nullopt, SearchDirection::Backward).empty()
    );
}

TEST_F(EDC15FileSearchTest, FindPatternsForwardRespectsBound)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    putBytes(image, 0x3000, { 0xAA, 0xBB });
    const auto file = load(image);

    const std::vector<size_t> expected{ 0x1000, 0x2000 };
    EXPECT_EQ(file.findPatterns("AA BB", 0, 0x2800), expected);
}

TEST_F(EDC15FileSearchTest, FindPatternsForwardMatchOnBound)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    const auto file = load(image);

    // A match sitting exactly on `to` must be returned, not make the search throw
    std::vector<size_t> positions;
    EXPECT_NO_THROW(positions = file.findPatterns("AA BB", 0, 0x2000));

    const std::vector<size_t> expected{ 0x1000, 0x2000 };
    EXPECT_EQ(positions, expected);
}

TEST_F(EDC15FileSearchTest, FindPatternsBackwardReturnsAllInReverseOrder)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    putBytes(image, 0, { 0xAA, 0xBB });
    const auto file = load(image);

    const std::vector<size_t> expected{ 0x2000, 0x1000, 0 };
    EXPECT_EQ(
        file.findPatterns("AA BB", kFlashSize - 1, std::nullopt, SearchDirection::Backward),
        expected
    );
}

TEST_F(EDC15FileSearchTest, FindPatternsBackwardSkipsOverlappingMatches)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xAA, 0xAA });
    const auto file = load(image);

    // Backward restarts at the match start, so unlike forward,
    // overlapping matches are not reported (documents current behavior)
    const std::vector<size_t> expected{ 0x1001 };
    EXPECT_EQ(
        file.findPatterns("AA AA", 0x2000, std::nullopt, SearchDirection::Backward),
        expected
    );
}

TEST_F(EDC15FileSearchTest, FindPatternsBackwardRespectsBound)
{
    auto image = blankImage();
    putBytes(image, 0x1000, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    putBytes(image, 0x3000, { 0xAA, 0xBB });
    const auto file = load(image);

    // Once the last match in range is found, the search must stop cleanly
    std::vector<size_t> positions;
    EXPECT_NO_THROW(
        positions = file.findPatterns("AA BB", 0x4000, 0x1800, SearchDirection::Backward)
    );

    const std::vector<size_t> expected{ 0x3000, 0x2000 };
    EXPECT_EQ(positions, expected);
}

TEST_F(EDC15FileSearchTest, FindPatternsBackwardMatchOnBound)
{
    auto image = blankImage();
    putBytes(image, 0x1800, { 0xAA, 0xBB });
    putBytes(image, 0x2000, { 0xAA, 0xBB });
    const auto file = load(image);

    // A match sitting exactly on `to` must be returned, not make the search throw
    std::vector<size_t> positions;
    EXPECT_NO_THROW(
        positions = file.findPatterns("AA BB", 0x4000, 0x1800, SearchDirection::Backward)
    );

    const std::vector<size_t> expected{ 0x2000, 0x1800 };
    EXPECT_EQ(positions, expected);
}

// ============================================================================
// Word access
// ============================================================================

TEST_F(EDC15FileWordTest, ReadWordIsLittleEndian)
{
    auto image = blankImage();
    putBytes(image, 0x100, { 0x34, 0x12 });
    const auto file = load(image);

    EXPECT_EQ(file.readWord(0x100), 0x1234);
}

TEST_F(EDC15FileWordTest, ReadWordAcceptsOddAddress)
{
    auto image = blankImage();
    putBytes(image, 0x101, { 0xCD, 0xAB });
    const auto file = load(image);

    EXPECT_EQ(file.readWord(0x101), 0xABCD);
}

TEST_F(EDC15FileWordTest, ReadWordBounds)
{
    auto image = blankImage();
    putBytes(image, kFlashSize - 2, { 0x78, 0x56 });
    const auto file = load(image);

    EXPECT_EQ(file.readWord(kFlashSize - 2), 0x5678);
    EXPECT_THROW(file.readWord(kFlashSize - 1), std::invalid_argument);
    EXPECT_THROW(file.readWord(kFlashSize), std::invalid_argument);
}

TEST_F(EDC15FileWordTest, ReadWordAlignedUsesWordIndex)
{
    auto image = blankImage();
    putBytes(image, 0x200, { 0x34, 0x12 });
    const auto file = load(image);

    EXPECT_EQ(file.readWordAligned(0x100), 0x1234);
    EXPECT_EQ(file.readWordAligned(0x100), file.readWord(0x200));
}

TEST_F(EDC15FileWordTest, ReadWordAlignedBounds)
{
    auto image = blankImage();
    putBytes(image, kFlashSize - 2, { 0x78, 0x56 });
    const auto file = load(image);

    EXPECT_EQ(file.readWordAligned(kFlashSize / 2 - 1), 0x5678);
    EXPECT_THROW(file.readWordAligned(kFlashSize / 2), std::invalid_argument);
}

TEST_F(EDC15FileWordTest, WriteLittleEndianWritesLowByteFirst)
{
    auto file = load(blankImage());
    file.writeLittleEndian(0x300, 0xBEEF);

    EXPECT_EQ(file.data()[0x300], 0xEF);
    EXPECT_EQ(file.data()[0x301], 0xBE);
    EXPECT_EQ(file.readWord(0x300), 0xBEEF);
}

TEST_F(EDC15FileWordTest, WriteLittleEndianDoesNotTouchNeighbours)
{
    auto file = load(blankImage());
    file.writeLittleEndian(0x300, 0x0000);

    EXPECT_EQ(file.data()[0x2FF], 0xFF);
    EXPECT_EQ(file.data()[0x302], 0xFF);
}

TEST_F(EDC15FileWordTest, WriteLittleEndianBounds)
{
    auto file = load(blankImage());

    EXPECT_NO_THROW(file.writeLittleEndian(kFlashSize - 2, 0x1234));
    EXPECT_EQ(file.readWord(kFlashSize - 2), 0x1234);
    EXPECT_THROW(file.writeLittleEndian(kFlashSize - 1, 0x1234), std::invalid_argument);
}

// ============================================================================
// Firmware infos / VAG part number
// ============================================================================

TEST_F(EDC15FileInfosTest, FirmwareInfosFromRealBinary012FK)
{
    const EDC15File file{ binaryPath("012fk.bin") };

    EXPECT_EQ(
        file.getFirmwareInfos(),
        "038906012FK 1,9l R4 EDC  SG  3357 28SA5092 0281010683 ED5KD100HEX038906012FK 0101 "
    );
    EXPECT_EQ(file.getVAGPartnumber(), "038906012FK");
}

TEST_F(EDC15FileInfosTest, FirmwareInfosFromRealBinary019NJ)
{
    const EDC15File file{ binaryPath("038906019NJ.bin") };

    EXPECT_EQ(
        file.getFirmwareInfos(),
        "038906019NJ 1,9l R4 EDC  SG  7331 39S10368 0281011823 FGQMCA00   038906019NJ 02/05"
    );
    EXPECT_EQ(file.getVAGPartnumber(), "038906019NJ");
}

TEST_F(EDC15FileInfosTest, FirmwareInfosSkipsGarbageBeforeDigits)
{
    auto image = blankImage();
    constexpr size_t pos = 0x50000;
    putCString(image, pos, "\xC9/garbage038906019LK 1,9l R4 EDC  SG  1234");
    const auto file = load(image);

    EXPECT_EQ(file.getFirmwareInfos(), "038906019LK 1,9l R4 EDC  SG  1234");
    EXPECT_EQ(file.getVAGPartnumber(), "038906019LK");
}

TEST_F(EDC15FileInfosTest, FirmwareInfosEmptyWithoutEDCMarker)
{
    const auto file = load(blankImage());

    EXPECT_TRUE(file.getFirmwareInfos().empty());
    EXPECT_TRUE(file.getVAGPartnumber().empty());
}

TEST_F(EDC15FileInfosTest, FirmwareInfosIgnoresMarkerBelowSecondHalf)
{
    auto image = blankImage();
    putCString(image, 0x1000, "038906019LK 1,9l R4 EDC  SG  1234");
    const auto file = load(image);

    // The marker is only searched from 0x40000
    EXPECT_TRUE(file.getFirmwareInfos().empty());
}

TEST_F(EDC15FileInfosTest, FirmwareInfosEmptyWithoutLongDigitSequence)
{
    auto image = blankImage();
    constexpr size_t pos = 0x50000;
    putCString(image, pos, "12345 R4 EDC  SG");
    const auto file = load(image);

    EXPECT_TRUE(file.getFirmwareInfos().empty());
}

TEST(EDC15FilePartnumberTest, ExtractsPartnumber)
{
    EXPECT_EQ(
        EDC15File::getVAGPartnumber("0281010302 038906019LK 1,9l R4 EDC  SG  2304"),
        "038906019LK"
    );
}

TEST(EDC15FilePartnumberTest, AcceptsOneOrTwoLetters)
{
    EXPECT_EQ(EDC15File::getVAGPartnumber("038906012F rest"), "038906012F");
    EXPECT_EQ(EDC15File::getVAGPartnumber("038906012FK rest"), "038906012FK");
    EXPECT_EQ(EDC15File::getVAGPartnumber("038906012ABC"), "038906012AB");
}

TEST(EDC15FilePartnumberTest, ReturnsFirstMatch)
{
    EXPECT_EQ(
        EDC15File::getVAGPartnumber("038906019NJ 1,9l 038906012FK"),
        "038906019NJ"
    );
}

TEST(EDC15FilePartnumberTest, EmptyWhenNotFound)
{
    EXPECT_TRUE(EDC15File::getVAGPartnumber("").empty());
    EXPECT_TRUE(EDC15File::getVAGPartnumber(std::string_view{}).empty());
    EXPECT_TRUE(EDC15File::getVAGPartnumber("03890601LK").empty());   // 8 digits
    EXPECT_TRUE(EDC15File::getVAGPartnumber("038906019lk").empty());  // lowercase
    EXPECT_TRUE(EDC15File::getVAGPartnumber("038906019 LK").empty());
}

// ============================================================================
// File IO
// ============================================================================

TEST_F(EDC15FileIOTest, ReadFileReturnsExactContent)
{
    const std::vector<uint8_t> content{ 0x00, 0x01, 0x0A, 0x0D, 0x1A, 0xFF };
    const auto path = writeImage(content, "small.bin");

    EXPECT_EQ(EDC15File::readFile(path), content);
}

TEST_F(EDC15FileIOTest, ReadFileOnEmptyFile)
{
    const auto path = writeImage({}, "empty.bin");
    EXPECT_TRUE(EDC15File::readFile(path).empty());
}

TEST_F(EDC15FileIOTest, ReadFileThrowsOnMissingFile)
{
    EXPECT_THROW(EDC15File::readFile(tmpDir / "missing.bin"), std::runtime_error);
}

TEST_F(EDC15FileIOTest, WriteFileRoundTrip)
{
    auto image = blankImage();
    putBytes(image, 0x1234, { 0xDE, 0xAD });
    auto file = load(image);
    file.writeLittleEndian(0x4000, 0xBEEF);

    const auto outPath = tmpDir / "out.bin";
    file.writeFile(outPath);

    const EDC15File reloaded{ outPath };
    EXPECT_EQ(reloaded.data(), file.data());
    EXPECT_EQ(reloaded.readWord(0x4000), 0xBEEF);
}

TEST_F(EDC15FileIOTest, WriteFileWithContentTruncatesExistingFile)
{
    auto file = load(blankImage());
    const auto outPath = writeImage(std::vector<uint8_t>(100, 0x55), "out.bin");

    const std::vector<uint8_t> content{ 0x01, 0x02, 0x03 };
    file.writeFile(outPath, content);

    EXPECT_EQ(EDC15File::readFile(outPath), content);
}

TEST_F(EDC15FileIOTest, WriteFileThrowsOnInvalidPath)
{
    auto file = load(blankImage());
    EXPECT_THROW(file.writeFile(tmpDir / "missing_dir" / "out.bin"), std::runtime_error);
}

// ============================================================================
// isEmptyBlock (protected)
// ============================================================================

TEST_F(EDC15FileTest, IsEmptyBlockOnErasedArea)
{
    const auto path = writeImage(blankImage());
    TestableEDC15File file{ path };

    EXPECT_TRUE(file.isEmptyBlock(0x1000, 0x100, 0xFF));
    EXPECT_FALSE(file.isEmptyBlock(0x1000, 0x100, 0x00));
}

TEST_F(EDC15FileTest, IsEmptyBlockDetectsSingleByte)
{
    auto image = blankImage();
    image[0x10FF] = 0xFE;
    const auto path = writeImage(image);
    TestableEDC15File file{ path };

    EXPECT_FALSE(file.isEmptyBlock(0x1000, 0x100, 0xFF));  // last byte of the block
    EXPECT_TRUE(file.isEmptyBlock(0x1000, 0xFF, 0xFF));    // block stops just before
}

TEST_F(EDC15FileTest, IsEmptyBlockWithZeroLength)
{
    const auto path = writeImage(blankImage(0x00));
    TestableEDC15File file{ path };

    EXPECT_TRUE(file.isEmptyBlock(0x1000, 0, 0xAA));
}

TEST_F(EDC15FileTest, IsEmptyBlockWholeFlash)
{
    const auto path = writeImage(blankImage(0x00));
    TestableEDC15File file{ path };

    EXPECT_TRUE(file.isEmptyBlock(0, kFlashSize, 0x00));
}
