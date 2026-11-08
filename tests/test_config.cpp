#include "vagedc15/Core/Config.h"
#include "vagedc15/Logging/Logger.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
    void writeText(const fs::path& path, std::string_view content)
    {
        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        file << content;
    }

    // Text mode on purpose: Config writes in text mode, so on Windows
    // "\r\n" is read back as "\n" and the expected strings stay portable
    std::string readText(const fs::path& path)
    {
        std::ifstream file{ path };
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    // Checks that `fn` throws a std::runtime_error whose message contains `part`
    template<typename Fn>
    ::testing::AssertionResult throwsWith(Fn&& fn, std::string_view part)
    {
        try {
            fn();
        }
        catch (const std::runtime_error& e) {
            if (std::string_view{ e.what() }.find(part) == std::string_view::npos)
                return ::testing::AssertionFailure()
                << "error '" << e.what() << "' does not contain '" << part << "'";
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "expected a std::runtime_error, nothing was thrown";
    }

    // Each test gets its own temp directory, removed afterwards
    class ConfigTest : public ::testing::Test {
    protected:
        fs::path tmpDir;
        fs::path cfgPath;

        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            std::string name{ info->name() };
            std::replace(name.begin(), name.end(), '/', '_');   // parameterized names

            tmpDir = fs::temp_directory_path() / std::format(
                "vagedc15_{}_{}", info->test_suite_name(), name
            );
            fs::remove_all(tmpDir);
            fs::create_directories(tmpDir);
            cfgPath = tmpDir / "config.cfg";
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(tmpDir, ec);
        }

        Config loadFrom(std::string_view content)
        {
            writeText(cfgPath, content);
            return Config{ cfgPath };
        }
    };

    using ConfigReadKeyTest = ConfigTest;
    using ConfigDefaultTest = ConfigTest;
    using ConfigSaveTest = ConfigTest;
}

// ============================================================================
// Construction
// ============================================================================

TEST_F(ConfigTest, MissingFileIsNotCreatedByDefault)
{
    const Config cfg{ cfgPath };
    EXPECT_FALSE(fs::exists(cfgPath));
}

TEST_F(ConfigTest, MissingFileIsCreatedWithSaveIfNoFile)
{
    const Config cfg{ cfgPath, true };

    ASSERT_TRUE(fs::is_regular_file(cfgPath));
    EXPECT_TRUE(readText(cfgPath).empty());
}

TEST_F(ConfigTest, EmptyFileHasNoKey)
{
    const auto cfg = loadFrom("");
    EXPECT_TRUE(throwsWith([&] { cfg.readKey<int>("anything"); }, "anything"));
}

TEST_F(ConfigTest, DefaultConstructedConfigWorksInMemory)
{
    Config cfg;

    EXPECT_THROW(cfg.readKey<int>("key"), std::runtime_error);
    EXPECT_EQ(cfg.readKey<int>("key", 7), 7);
    EXPECT_EQ(cfg.readKey<int>("key"), 7);
}

// ============================================================================
// Parsing
// ============================================================================

TEST_F(ConfigTest, ReadsSimpleKeyValues)
{
    const auto cfg = loadFrom("speed=42\nname=edc15\n");

    EXPECT_EQ(cfg.readKey<int>("speed"), 42);
    EXPECT_EQ(cfg.readKey<std::string>("name"), "edc15");
}

TEST_F(ConfigTest, LastLineWithoutNewline)
{
    const auto cfg = loadFrom("a=1\nb=2");
    EXPECT_EQ(cfg.readKey<int>("b"), 2);
}

TEST_F(ConfigTest, IgnoresCommentsAndBlankLines)
{
    const auto cfg = loadFrom(
        "# a comment\n"
        "\n"
        "    \n"
        "   # an indented comment\n"
        "key=1\n"
        "#other=2\n"
    );

    EXPECT_EQ(cfg.readKey<int>("key"), 1);
    EXPECT_THROW(cfg.readKey<int>("other"), std::runtime_error);
    EXPECT_THROW(cfg.readKey<int>("#other"), std::runtime_error);
}

TEST_F(ConfigTest, TrimsKeysAndValues)
{
    const auto cfg = loadFrom("  key  =  value with spaces  \n\tother\t=\t3\t\n");

    EXPECT_EQ(cfg.readKey<std::string>("key"), "value with spaces");
    EXPECT_EQ(cfg.readKey<int>("other"), 3);
}

TEST_F(ConfigTest, HandlesWindowsLineEndings)
{
    const auto cfg = loadFrom("a=1\r\nname=edc15\r\n");

    EXPECT_EQ(cfg.readKey<int>("a"), 1);
    EXPECT_EQ(cfg.readKey<std::string>("name"), "edc15");
}

TEST_F(ConfigTest, KeysFromFileAreLowercased)
{
    const auto cfg = loadFrom("LogLevel=debug\n");
    EXPECT_EQ(cfg.readKey<std::string>("loglevel"), "debug");
}

TEST_F(ConfigTest, ValuesKeepTheirCase)
{
    const auto cfg = loadFrom("key=MiXeD\n");
    EXPECT_EQ(cfg.readKey<std::string>("key"), "MiXeD");
}

TEST_F(ConfigTest, DuplicateKeyThrowsWithLineNumber)
{
    writeText(cfgPath, "a=1\nb=2\na=3\n");
    EXPECT_TRUE(throwsWith([&] { Config{ cfgPath }; }, "line 3"));
}

TEST_F(ConfigTest, DuplicateKeyIsCaseInsensitive)
{
    writeText(cfgPath, "Key=1\nkey=2\n");
    EXPECT_THROW(Config{ cfgPath }, std::runtime_error);
}

TEST_F(ConfigTest, BadLineReportsItsNumber)
{
    writeText(cfgPath, "# comment\na=1\n\nbroken\n");
    EXPECT_TRUE(throwsWith([&] { Config{ cfgPath }; }, "Line 4"));
}

// ============================================================================
// Malformed lines
// ============================================================================

class ConfigMalformedLineTest
    : public ConfigTest
    , public ::testing::WithParamInterface<std::string> {};

TEST_P(ConfigMalformedLineTest, Throws)
{
    writeText(cfgPath, GetParam());
    EXPECT_THROW(Config{ cfgPath }, std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    Lines,
    ConfigMalformedLineTest,
    ::testing::Values(
        "novalue",
        "=value",
        "  =value",
        "a=b=c",
        "a==b",
        "ok=1\nbroken"
    )
);

// ============================================================================
// readKey<T>(name)
// ============================================================================

TEST_F(ConfigReadKeyTest, MissingKeyThrowsWithItsName)
{
    const auto cfg = loadFrom("a=1\n");
    EXPECT_TRUE(throwsWith([&] { cfg.readKey<int>("missing"); }, "missing"));
}

TEST_F(ConfigReadKeyTest, Integers)
{
    const auto cfg = loadFrom("pos=42\nneg=-12\nzero=0\nbig=4294967295\n");

    EXPECT_EQ(cfg.readKey<int>("pos"), 42);
    EXPECT_EQ(cfg.readKey<int>("neg"), -12);
    EXPECT_EQ(cfg.readKey<int>("zero"), 0);
    EXPECT_EQ(cfg.readKey<uint32_t>("big"), 4294967295u);
}

TEST_F(ConfigReadKeyTest, IntegerRejectsTrailingGarbage)
{
    const auto cfg = loadFrom("a=12abc\nb=abc\nc=1.5\nd=0x10\n");

    EXPECT_THROW(cfg.readKey<int>("a"), std::runtime_error);
    EXPECT_THROW(cfg.readKey<int>("b"), std::runtime_error);
    EXPECT_THROW(cfg.readKey<int>("c"), std::runtime_error);
    EXPECT_THROW(cfg.readKey<int>("d"), std::runtime_error);
}

TEST_F(ConfigReadKeyTest, IntegerRejectsOutOfRange)
{
    const auto cfg = loadFrom("byte=300\nneg=-1\n");

    EXPECT_THROW(cfg.readKey<uint8_t>("byte"), std::runtime_error);
    EXPECT_THROW(cfg.readKey<unsigned>("neg"), std::runtime_error);
}

TEST_F(ConfigReadKeyTest, Double)
{
    const auto cfg = loadFrom("ratio=1.5\nneg=-0.25\nint=3\n");

    EXPECT_DOUBLE_EQ(cfg.readKey<double>("ratio"), 1.5);
    EXPECT_DOUBLE_EQ(cfg.readKey<double>("neg"), -0.25);
    EXPECT_DOUBLE_EQ(cfg.readKey<double>("int"), 3.0);
}

TEST_F(ConfigReadKeyTest, BoolTrueValues)
{
    const auto cfg = loadFrom("a=1\nb=true\nc=yes\n");

    EXPECT_TRUE(cfg.readKey<bool>("a"));
    EXPECT_TRUE(cfg.readKey<bool>("b"));
    EXPECT_TRUE(cfg.readKey<bool>("c"));
}

TEST_F(ConfigReadKeyTest, BoolEverythingElseIsFalse)
{
    const auto cfg = loadFrom("a=0\nb=false\nc=no\nd=garbage\ne=TRUE\n");

    EXPECT_FALSE(cfg.readKey<bool>("a"));
    EXPECT_FALSE(cfg.readKey<bool>("b"));
    EXPECT_FALSE(cfg.readKey<bool>("c"));
    EXPECT_FALSE(cfg.readKey<bool>("d"));
    // Comparison is case sensitive (documents current behavior)
    EXPECT_FALSE(cfg.readKey<bool>("e"));
}

TEST_F(ConfigReadKeyTest, Char)
{
    const auto cfg = loadFrom("sep=;\nlong=ab\n");

    EXPECT_EQ(cfg.readKey<char>("sep"), ';');
    EXPECT_TRUE(throwsWith([&] { cfg.readKey<char>("long"); }, "single character"));
}

TEST_F(ConfigReadKeyTest, String)
{
    const auto cfg = loadFrom("path=C:\\Users\\me\\file.bin\n");
    EXPECT_EQ(cfg.readKey<std::string>("path"), "C:\\Users\\me\\file.bin");
}

TEST_F(ConfigReadKeyTest, LogLevel)
{
    const auto cfg = loadFrom("a=trace\nb=debug\nc=info\nd=warning\ne=error\nf=garbage\n");

    EXPECT_EQ(cfg.readKey<Logger::Level>("a"), Logger::Level::Trace);
    EXPECT_EQ(cfg.readKey<Logger::Level>("b"), Logger::Level::Debug);
    EXPECT_EQ(cfg.readKey<Logger::Level>("c"), Logger::Level::Info);
    EXPECT_EQ(cfg.readKey<Logger::Level>("d"), Logger::Level::Warning);
    EXPECT_EQ(cfg.readKey<Logger::Level>("e"), Logger::Level::Error);
    // Unknown names fall back to None (documents current behavior)
    EXPECT_EQ(cfg.readKey<Logger::Level>("f"), Logger::Level::None);
}

// ============================================================================
// readKey<T>(name, default)
// ============================================================================

TEST_F(ConfigDefaultTest, MissingKeyReturnsDefault)
{
    auto cfg = loadFrom("other=1\n");
    EXPECT_EQ(cfg.readKey<int>("key", 99), 99);
}

TEST_F(ConfigDefaultTest, MissingKeyIsStoredAndPersisted)
{
    auto cfg = loadFrom("other=1\n");
    cfg.readKey<int>("key", 99);

    EXPECT_EQ(cfg.readKey<int>("key"), 99);

    const Config reloaded{ cfgPath };
    EXPECT_EQ(reloaded.readKey<int>("key"), 99);
    EXPECT_EQ(reloaded.readKey<int>("other"), 1);
}

TEST_F(ConfigDefaultTest, MissingKeyCreatesFile)
{
    Config cfg{ cfgPath };
    ASSERT_FALSE(fs::exists(cfgPath));

    cfg.readKey<std::string>("interface", std::string{ "ftdi" });

    EXPECT_EQ(readText(cfgPath), "interface=ftdi\n");
}

TEST_F(ConfigDefaultTest, ExistingKeyIgnoresDefault)
{
    auto cfg = loadFrom("key=5\n");
    EXPECT_EQ(cfg.readKey<int>("key", 99), 5);
}

TEST_F(ConfigDefaultTest, ExistingKeyDoesNotRewriteFile)
{
    const std::string content{ "# keep me\nkey=5\n" };
    auto cfg = loadFrom(content);
    cfg.readKey<int>("key", 99);

    EXPECT_EQ(readText(cfgPath), content);
}

TEST_F(ConfigDefaultTest, ExistingInvalidValueStillThrows)
{
    auto cfg = loadFrom("key=abc\n");
    EXPECT_THROW(cfg.readKey<int>("key", 99), std::runtime_error);
}

TEST_F(ConfigDefaultTest, DefaultsAreSerializedPerType)
{
    Config cfg{ cfgPath };
    cfg.readKey<bool>("flag", true);
    cfg.readKey<char>("sep", ';');
    cfg.readKey<Logger::Level>("level", Logger::Level::Warning);
    cfg.readKey<double>("ratio", 2.5);

    const Config reloaded{ cfgPath };
    EXPECT_TRUE(reloaded.readKey<bool>("flag"));
    EXPECT_EQ(reloaded.readKey<char>("sep"), ';');
    EXPECT_EQ(reloaded.readKey<Logger::Level>("level"), Logger::Level::Warning);
    EXPECT_DOUBLE_EQ(reloaded.readKey<double>("ratio"), 2.5);
}

// ============================================================================
// serialize
// ============================================================================

TEST(ConfigSerializeTest, PerType)
{
    Config cfg;

    EXPECT_EQ(cfg.serialize(std::string{ "text" }), "text");
    EXPECT_EQ(cfg.serialize('c'), "c");
    EXPECT_EQ(cfg.serialize(true), "true");
    EXPECT_EQ(cfg.serialize(false), "false");
    EXPECT_EQ(cfg.serialize(Logger::Level::Info), "info");
    EXPECT_EQ(cfg.serialize(-42), "-42");
    EXPECT_EQ(cfg.serialize(1.5), "1.5");
}

// ============================================================================
// saveKey
// ============================================================================

TEST_F(ConfigSaveTest, RoundTripsAllTypes)
{
    {
        Config cfg{ cfgPath };
        cfg.saveKey("int", -42);
        cfg.saveKey("u8", static_cast<uint8_t>(200));
        cfg.saveKey("double", 0.125);
        cfg.saveKey("bool", false);
        cfg.saveKey("char", 'x');
        cfg.saveKey("string", std::string{ "hello world" });
        cfg.saveKey("level", Logger::Level::Error);
    }

    const Config cfg{ cfgPath };
    EXPECT_EQ(cfg.readKey<int>("int"), -42);
    EXPECT_EQ(cfg.readKey<uint8_t>("u8"), 200);
    EXPECT_DOUBLE_EQ(cfg.readKey<double>("double"), 0.125);
    EXPECT_FALSE(cfg.readKey<bool>("bool"));
    EXPECT_EQ(cfg.readKey<char>("char"), 'x');
    EXPECT_EQ(cfg.readKey<std::string>("string"), "hello world");
    EXPECT_EQ(cfg.readKey<Logger::Level>("level"), Logger::Level::Error);
}

TEST_F(ConfigSaveTest, OverwritesExistingValue)
{
    auto cfg = loadFrom("key=1\n");
    cfg.saveKey("key", 2);

    EXPECT_EQ(cfg.readKey<int>("key"), 2);
    EXPECT_EQ(Config{ cfgPath }.readKey<int>("key"), 2);
}

TEST_F(ConfigSaveTest, KeepsOtherKeys)
{
    auto cfg = loadFrom("a=1\nb=2\n");
    cfg.saveKey("c", 3);

    const Config reloaded{ cfgPath };
    EXPECT_EQ(reloaded.readKey<int>("a"), 1);
    EXPECT_EQ(reloaded.readKey<int>("b"), 2);
    EXPECT_EQ(reloaded.readKey<int>("c"), 3);
}

TEST_F(ConfigSaveTest, MixedCaseKeySurvivesReloads)
{
    // The file loader lowercases keys, saveKey/readKey do not:
    // a mixed-case key must still be readable after a reload
    // and must not end up written twice in the file
    {
        Config cfg{ cfgPath };
        cfg.saveKey("LogLevel", std::string{ "info" });
    }
    {
        Config cfg{ cfgPath };
        EXPECT_EQ(cfg.readKey<std::string>("LogLevel", std::string{ "debug" }), "info");
    }

    std::unique_ptr<Config> cfg;
    ASSERT_NO_THROW(cfg = std::make_unique<Config>(cfgPath));
    EXPECT_EQ(cfg->readKey<std::string>("LogLevel"), "info");
}

TEST_F(ConfigSaveTest, ValueContainingEqualSignSurvivesReload)
{
    {
        Config cfg{ cfgPath };
        cfg.saveKey("args", std::string{ "--speed=10400" });
    }

    std::unique_ptr<Config> cfg;
    ASSERT_NO_THROW(cfg = std::make_unique<Config>(cfgPath));
    EXPECT_EQ(cfg->readKey<std::string>("args"), "--speed=10400");
}

TEST_F(ConfigSaveTest, EmptyStringValueSurvivesReload)
{
    {
        Config cfg{ cfgPath };
        cfg.saveKey("lastfile", std::string{});
    }

    std::unique_ptr<Config> cfg;
    ASSERT_NO_THROW(cfg = std::make_unique<Config>(cfgPath));
    EXPECT_EQ(cfg->readKey<std::string>("lastfile"), "");
}

// ============================================================================
// Escaping
// ============================================================================

TEST_F(ConfigTest, ReadsEscapedEqualSign)
{
    const auto cfg = loadFrom("args=--speed\\=10400\n");
    EXPECT_EQ(cfg.readKey<std::string>("args"), "--speed=10400");
}

TEST_F(ConfigTest, ReadsEscapedBackslash)
{
    const auto cfg = loadFrom("a=x\\\\y\n");
    EXPECT_EQ(cfg.readKey<std::string>("a"), "x\\y");
}

TEST_F(ConfigTest, ReadsEscapedEqualSignInKey)
{
    const auto cfg = loadFrom("a\\=b=1\n");
    EXPECT_EQ(cfg.readKey<int>("a=b"), 1);
}

TEST_F(ConfigTest, KeepsUnknownEscapesAsIs)
{
    // Hand-written Windows paths must not need doubled backslashes
    const auto cfg = loadFrom("path=C:\\Users\\me\\\n");
    EXPECT_EQ(cfg.readKey<std::string>("path"), "C:\\Users\\me\\");
}

INSTANTIATE_TEST_SUITE_P(
    EscapedLines,
    ConfigMalformedLineTest,
    ::testing::Values(
        "a\\=b",        // the only '=' is escaped, no separator
        "a=b\\\\=c",    // escaped backslash then a real second '='
        "a=b\\=c=d"
    )
);

TEST_F(ConfigSaveTest, WritesEscapedValue)
{
    Config cfg{ cfgPath };
    cfg.saveKey("args", std::string{ "a=b\\c" });

    EXPECT_EQ(readText(cfgPath), "args=a\\=b\\\\c\n");
}

TEST_F(ConfigSaveTest, WritesEscapedKey)
{
    Config cfg{ cfgPath };
    cfg.saveKey("a=b", 1);

    EXPECT_EQ(readText(cfgPath), "a\\=b=1\n");
    EXPECT_EQ(Config{ cfgPath }.readKey<int>("a=b"), 1);
}

TEST_F(ConfigSaveTest, TrickyValuesSurviveReload)
{
    const std::vector<std::string> values{
        "--speed=10400",
        "C:\\dir\\",
        "\\\\server\\share",
        "a\\=b",
        "a\\\\=b",
        "==",
        "\\",
        "x=\\=y\\",
    };

    {
        Config cfg{ cfgPath };
        for (size_t i = 0; i < values.size(); ++i) {
            cfg.saveKey(std::format("k{}", i), values[i]);
        }
    }

    std::unique_ptr<Config> cfg;
    ASSERT_NO_THROW(cfg = std::make_unique<Config>(cfgPath));

    for (size_t i = 0; i < values.size(); ++i) {
        SCOPED_TRACE(values[i]);
        EXPECT_EQ(cfg->readKey<std::string>(std::format("k{}", i)), values[i]);
    }
}