#include "Core/CommandInterpreter.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    // The internal string_views point to the string literals (static lifetime),
    // so the pointer vector can be destroyed right after construction.
    CommandInterpreter makeInterpreter(std::initializer_list<const char*> args)
    {
        std::vector<const char*> argv{ args };
        return CommandInterpreter{ static_cast<int>(argv.size()), argv.data() };
    }

    // Test command table, modeled on the logger's one
    const std::vector<CommandInterpreter::CommandSpec> testSpecs{
        {
            .name = "list",
            .freeArgsError = "list takes no argument",
            .additionalParams = {},
        },
        {
            .name = "log",
            .valued = { "interface", "file" },
            .minFreeArgs = 1,
            .maxFreeArgs = std::nullopt,
            .freeArgsError = "You need to provide at least one variable to log!",
            .additionalParams = {},
        },
        {
            .name = "diag",
            .flags = { "save" },
            .valued = { "interface" },
            .freeArgsError = "diag takes no free argument",
            .additionalParams = {},
        },
    };

    const std::vector<CommandInterpreter::CommandSpec> otherSpecs{
        {
            .name = "other",
            .freeArgsError = "other takes no argument",
            .additionalParams = {},
        },
    };

    // Checks that validateCommandLine returned an error containing `part`
    ::testing::AssertionResult errorContains(const std::optional<std::string>& error, std::string_view part)
    {
        if (!error)
            return ::testing::AssertionFailure() << "expected an error containing '" << part << "', got no error";
        if (error->find(part) == std::string::npos)
            return ::testing::AssertionFailure() << "error '" << *error << "' does not contain '" << part << "'";
        return ::testing::AssertionSuccess();
    }
}

// ============================================================================
// Command
// ============================================================================

TEST(CommandInterpreterTest, NoCommand)
{
    auto ci = makeInterpreter({ "exe.exe" });

    EXPECT_EQ(ci.getCommand(), std::nullopt);
    EXPECT_FALSE(ci.isCommand("list"));
    EXPECT_FALSE(ci.hasArgs());
    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_EQ(ci.countOptArgs(), 0u);
    EXPECT_TRUE(ci.validate());
}

TEST(CommandInterpreterTest, CommandOnly)
{
    auto ci = makeInterpreter({ "exe.exe", "list" });

    EXPECT_EQ(ci.getCommand(), "list");
    EXPECT_TRUE(ci.isCommand("list"));
    EXPECT_FALSE(ci.isCommand("list-pid"));
    EXPECT_FALSE(ci.hasArgs());
    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_EQ(ci.countOptArgs(), 0u);
}

// Current behavior: an option in first position is taken as the command.
TEST(CommandInterpreterTest, OptionBeforeCommand)
{
    auto ci = makeInterpreter({ "exe.exe", "--interface", "A6000001", "log", "rpm" });
    ci.addNeedsValue("interface");

    EXPECT_EQ(ci.getCommand(), "--interface");
    EXPECT_EQ(ci.countFreeArgs(), 2u);
}

// ============================================================================
// Splitting options / free args
// ============================================================================

TEST(CommandInterpreterTest, FlagOptionIsNotFreeArg)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1" });

    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_EQ(ci.countOptArgs(), 1u);
    EXPECT_TRUE(ci.hasArgs());
}

TEST(CommandInterpreterTest, SingleDashIsFreeArgNotOption)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "-arg1" });

    EXPECT_EQ(ci.countOptArgs(), 0u);
    EXPECT_EQ(ci.countFreeArgs(), 1u);
}

TEST(CommandInterpreterTest, ContiguousFreeArgs)
{
    auto ci = makeInterpreter({ "exe.exe", "--test", "opt1", "opt2" });

    EXPECT_EQ(ci.countFreeArgs(), 2u);
    EXPECT_TRUE(ci.hasArgs());
}

TEST(CommandInterpreterTest, NotContiguousFreeArgs)
{
    auto ci = makeInterpreter({ "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3" });

    EXPECT_EQ(ci.countFreeArgs(), 2u);
    EXPECT_TRUE(ci.hasArgs());
}

TEST(CommandInterpreterTest, FreeArgsContent)
{
    auto ci = makeInterpreter({ "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3" });

    const auto& freeargs = ci.freeArgs();

    ASSERT_EQ(freeargs.size(), 2u);
    EXPECT_EQ(freeargs[0], "opt1");
    EXPECT_EQ(freeargs[1], "opt2");
}

TEST(CommandInterpreterTest, FreeArgsStableAcrossCalls)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file", "out.csv", "rpm" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.freeArgs().size(), ci.freeArgs().size());
    EXPECT_EQ(ci.countFreeArgs(), ci.freeArgs().size());
    EXPECT_EQ(ci.countOptArgs(), ci.optArgs().size());
}

// ============================================================================
// Options with a value
// ============================================================================

TEST(CommandInterpreterTest, OptionWithValueConsumesNextArg)
{
    auto ci = makeInterpreter({ "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3" });
    ci.addNeedsValue("test2");

    EXPECT_EQ(ci.countFreeArgs(), 1u);
    EXPECT_EQ(ci.countOptArgs(), 3u);
    EXPECT_TRUE(ci.hasArgs());
}

TEST(CommandInterpreterTest, OptArgsContent)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "--save", "rpm", "ect" });
    ci.addNeedsValue("interface");

    const auto& optargs = ci.optArgs();

    ASSERT_EQ(optargs.size(), 2u);
    EXPECT_EQ(optargs[0].first, "--interface");
    EXPECT_EQ(optargs[0].second, "A6000001");
    EXPECT_EQ(optargs[1].second, std::nullopt);
    ASSERT_EQ(ci.countFreeArgs(), 2u);
    EXPECT_EQ(ci.freeArg(0), "rpm");
    EXPECT_EQ(ci.freeArg(1), "ect");
    EXPECT_TRUE(ci.isCommand("log"));
}

TEST(CommandInterpreterTest, ValueOptionFollowedByOptionHasNoValue)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "--save", "opt1" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.countFreeArgs(), 1u);
    EXPECT_EQ(ci.countOptArgs(), 2u);
    EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, ValueOptionDoesNotSwallowFollowingOption)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file", "--save" });
    ci.addNeedsValue("file");

    const auto& optargs = ci.optArgs();

    ASSERT_EQ(optargs.size(), 2u);
    EXPECT_EQ(optargs[0].first, "--file");
    EXPECT_EQ(optargs[0].second, std::nullopt);
    EXPECT_EQ(optargs[1].first, "--save");
    EXPECT_TRUE(ci.hasOption("save"));
    EXPECT_EQ(ci.countFreeArgs(), 0u);
}

TEST(CommandInterpreterTest, ValueOptionAtEndHasNoValue)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.countOptArgs(), 1u);
    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_TRUE(ci.hasOption("file"));
    EXPECT_FALSE(ci.optionHasValue("file"));
    EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, ValueOptionAtEndAfterFlag)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save", "--file" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_EQ(ci.countOptArgs(), 2u);
    EXPECT_TRUE(ci.hasOption("file"));
    EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, NeedsValueDeclaredAfterFirstParse)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file", "out.csv" });

    EXPECT_EQ(ci.countFreeArgs(), 1u);   // "out.csv" seen as a free arg

    ci.addNeedsValue("file");

    EXPECT_EQ(ci.countFreeArgs(), 0u);   // reparsed: "out.csv" is the value of --file
    EXPECT_EQ(ci.getOption("file"), "out.csv");
}

TEST(CommandInterpreterTest, GetOptionOnOptionThatTakesNoValue)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--save" });

    EXPECT_THROW(ci.getOption("save"), std::logic_error);
}

TEST(CommandInterpreterTest, GetOptionOnRegisteredFlagOptionThrows)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--save" });
    ci.addOption("save");

    EXPECT_THROW(ci.getOption("save"), std::logic_error);
}

TEST(CommandInterpreterTest, GetOptionNotPresent)
{
    auto ci = makeInterpreter({ "exe.exe", "log" });
    ci.addNeedsValue("file");

    EXPECT_FALSE(ci.hasOption("file"));
    EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, OptionNameWithAndWithoutDashes)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file", "out.csv" });
    ci.addNeedsValue("--file");

    EXPECT_EQ(ci.hasOption("file"), ci.hasOption("--file"));
    EXPECT_EQ(ci.getOption("file"), ci.getOption("--file"));
    EXPECT_TRUE(ci.optionNeedsValue("file"));
}

TEST(CommandInterpreterTest, DuplicatedOptionReturnsFirstValue)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--file", "a.csv", "--file", "b.csv" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.countOptArgs(), 2u);
    EXPECT_EQ(ci.getOption("file"), "a.csv");
    EXPECT_EQ(ci.countFreeArgs(), 0u);
}

// ============================================================================
// validateArgs: registered / unknown options
// ============================================================================

TEST(CommandInterpreterTest, NoOptionsPassesOptionValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "free" });

    EXPECT_TRUE(ci.validateArgs());
}

TEST(CommandInterpreterTest, UnknownOptionFailsValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1" });

    EXPECT_FALSE(ci.validateArgs());
    EXPECT_FALSE(ci.validate());
}

TEST(CommandInterpreterTest, RegisteredOptionPassesValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1" });
    ci.addOption("arg1");

    EXPECT_TRUE(ci.validateArgs());
    EXPECT_TRUE(ci.validate());
}

TEST(CommandInterpreterTest, OptionRegisteredWithDashesPassesValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1" });
    ci.addOption("--arg1");

    EXPECT_TRUE(ci.validateArgs());
}

TEST(CommandInterpreterTest, RegisteredButAbsentOptionPassesValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command" });
    ci.addOption("arg1");
    ci.addNeedsValue("file");

    EXPECT_TRUE(ci.validateArgs());
}

TEST(CommandInterpreterTest, OneUnknownOptionAmongRegisteredFailsValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1", "--typo", "--arg2" });
    ci.addOption("arg1");
    ci.addOption("arg2");

    EXPECT_FALSE(ci.validateArgs());
}

TEST(CommandInterpreterTest, OptionValidationIsCaseSensitive)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--ARG1" });
    ci.addOption("arg1");

    EXPECT_FALSE(ci.validateArgs());
}

TEST(CommandInterpreterTest, DoubleDashAloneIsUnknownOption)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--" });

    EXPECT_EQ(ci.countOptArgs(), 1u);
    EXPECT_EQ(ci.countFreeArgs(), 0u);
    EXPECT_FALSE(ci.validateArgs());
}

TEST(CommandInterpreterTest, ValueOptionPassesValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "x.bin" });
    ci.addNeedsValue("file");

    EXPECT_TRUE(ci.validateArgs());
    EXPECT_EQ(ci.countFreeArgs(), 0u);
}

TEST(CommandInterpreterTest, MixedFlagAndValueOptionsPassValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "x.bin", "--save" });
    ci.addNeedsValue("file");
    ci.addOption("save");

    EXPECT_TRUE(ci.validateArgs());
}

// Current behavior: an option that needs a value is accepted without one.
// Change this test if validateArgs should reject that case some day.
TEST(CommandInterpreterTest, ValueOptionWithoutValuePassesValidation)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file" });
    ci.addNeedsValue("file");

    EXPECT_TRUE(ci.validateArgs());
}

TEST(CommandInterpreterTest, AddOptionAfterParsingIsTakenIntoAccount)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--arg1" });

    EXPECT_FALSE(ci.validateArgs());

    ci.addOption("arg1");

    EXPECT_TRUE(ci.validateArgs());
}

TEST(CommandInterpreterTest, AddNeedsValueAfterParsingIsTakenIntoAccount)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "x.bin" });

    EXPECT_FALSE(ci.validateArgs());

    ci.addNeedsValue("file");

    EXPECT_TRUE(ci.validateArgs());
    EXPECT_EQ(ci.countFreeArgs(), 0u);
}

// ============================================================================
// validateFreeArgs / setFreeArgSize
// ============================================================================

TEST(CommandInterpreterTest, NoFreeArgLimitByDefault)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a", "b", "c", "d" });

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, FreeArgsWithinBoundsAreValid)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a", "b" });
    ci.setFreeArgSize(1, 3);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, FreeArgsEqualToMinAndMaxAreValid)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a", "b" });
    ci.setFreeArgSize(2, 2);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, ZeroFreeArgsAllowedRejectsOne)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a" });
    ci.setFreeArgSize(0, 0);

    EXPECT_FALSE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, ZeroFreeArgsAllowedAcceptsNone)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save" });
    ci.addOption("save");
    ci.setFreeArgSize(0, 0);

    EXPECT_TRUE(ci.validateFreeArgs());
    EXPECT_TRUE(ci.validate());
}

TEST(CommandInterpreterTest, FreeArgsAboveMaxAreInvalid)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a", "b" });
    ci.setFreeArgSize(std::nullopt, 1);

    EXPECT_FALSE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, FreeArgsBelowMinAreInvalid)
{
    auto ci = makeInterpreter({ "exe.exe", "command" });
    ci.setFreeArgSize(1, std::nullopt);

    EXPECT_FALSE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, OnlyMinSetHasNoUpperBound)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a", "b", "c", "d" });
    ci.setFreeArgSize(1, std::nullopt);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, OnlyMaxSetHasNoLowerBound)
{
    auto ci = makeInterpreter({ "exe.exe", "command" });
    ci.setFreeArgSize(std::nullopt, 2);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, OptionValueNotCountedAsFreeArgForLimits)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "x.bin" });
    ci.addNeedsValue("file");
    ci.setFreeArgSize(0, 0);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, CommandNotCountedAsFreeArgForLimits)
{
    auto ci = makeInterpreter({ "exe.exe", "command" });
    ci.setFreeArgSize(0, 0);

    EXPECT_TRUE(ci.validateFreeArgs());
}

TEST(CommandInterpreterTest, SetFreeArgSizeMinGreaterThanMaxThrows)
{
    auto ci = makeInterpreter({ "exe.exe", "command" });

    EXPECT_THROW(ci.setFreeArgSize(3, 1), std::invalid_argument);
}

TEST(CommandInterpreterTest, SetFreeArgSizeThrowKeepsPreviousLimits)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a" });
    ci.setFreeArgSize(0, 0);

    EXPECT_THROW(ci.setFreeArgSize(3, 1), std::invalid_argument);
    EXPECT_FALSE(ci.validateFreeArgs());   // still limited to 0
}

TEST(CommandInterpreterTest, SetFreeArgSizeCanBeReset)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "a" });
    ci.setFreeArgSize(0, 0);
    ci.setFreeArgSize(std::nullopt, std::nullopt);

    EXPECT_TRUE(ci.validateFreeArgs());
}

// ============================================================================
// validate: both checks combined
// ============================================================================

TEST(CommandInterpreterTest, ValidateFailsOnTooManyFreeArgsEvenIfOptionsOk)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save", "extra" });
    ci.addOption("save");
    ci.setFreeArgSize(0, 0);

    EXPECT_TRUE(ci.validateArgs());
    EXPECT_FALSE(ci.validateFreeArgs());
    EXPECT_FALSE(ci.validate());
}

TEST(CommandInterpreterTest, ValidateFailsOnUnknownOptionEvenIfFreeArgsOk)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--typo" });
    ci.setFreeArgSize(0, 0);

    EXPECT_FALSE(ci.validateArgs());
    EXPECT_TRUE(ci.validateFreeArgs());
    EXPECT_FALSE(ci.validate());
}

// Real case: list-pid --alias-only --compatible-only, no free arg
TEST(CommandInterpreterTest, ListPidTypicalUsage)
{
    auto ci = makeInterpreter({ "exe.exe", "list-pid", "--alias-only", "--compatible-only" });
    ci.addOption("alias-only");
    ci.addOption("compatible-only");
    ci.setFreeArgSize(0, 0);

    EXPECT_TRUE(ci.isCommand("list-pid"));
    EXPECT_TRUE(ci.hasOption("alias-only"));
    EXPECT_TRUE(ci.hasOption("compatible-only"));
    EXPECT_TRUE(ci.validate());
}

TEST(CommandInterpreterTest, ListPidRejectsFreeArg)
{
    auto ci = makeInterpreter({ "exe.exe", "list-pid", "--alias-only", "foo" });
    ci.addOption("alias-only");
    ci.addOption("compatible-only");
    ci.setFreeArgSize(0, 0);

    EXPECT_FALSE(ci.validate());
}

// ============================================================================
// getOptionHex
// ============================================================================

TEST(CommandInterpreterTest, HexTestNormal)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0x1234F" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), 0x1234Fu);
}

TEST(CommandInterpreterTest, HexTestNo0X)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "ABCD" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), 0xABCDu);
}

TEST(CommandInterpreterTest, HexTestEndsWithH)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "DEADh" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), 0xDEADu);
}

TEST(CommandInterpreterTest, HexTestPrefixAndSuffixTogether)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0xDEADh" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestNotHex)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "hello" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestPrefixThenNotHex)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0xhello" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestOnlyPrefix)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0x" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestOnlyH)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "h" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestEmptyValue)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestTrailingGarbage)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "8C420XYZ" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestNegativeSign)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "-1234" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestOptionAbsent)
{
    auto ci = makeInterpreter({ "exe.exe", "log" });
    ci.addNeedsValue("addr");

    EXPECT_FALSE(ci.getOptionHex("addr").has_value());
}

TEST(CommandInterpreterTest, HexTestOptionWithoutValue)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr" });
    ci.addNeedsValue("addr");

    EXPECT_FALSE(ci.getOptionHex("addr").has_value());
}

TEST(CommandInterpreterTest, HexTestOptionNotDeclared)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0x1234" });

    EXPECT_THROW(ci.getOptionHex("addr"), std::logic_error);
}

TEST(CommandInterpreterTest, HexTestZero)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), 0u);
}

TEST(CommandInterpreterTest, HexTestMaxValue)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0xFFFFFFFFFFFFFFFF" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), std::numeric_limits<size_t>::max());
}

TEST(CommandInterpreterTest, HexTestOverflow)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0x1FFFFFFFFFFFFFFFF" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestLowercaseDigits)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "abcd" });
    ci.addNeedsValue("addr");

    EXPECT_EQ(*ci.getOptionHex("addr"), 0xABCDu);
}

TEST(CommandInterpreterTest, HexTestUppercasePrefix)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "0X1234F" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestUppercaseHSuffix)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--addr", "DEADH" });
    ci.addNeedsValue("addr");

    EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

// ============================================================================
// firstUnregisteredOption
// ============================================================================

TEST(CommandInterpreterTest, FirstUnregisteredOptionNoneWithoutOptions)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "free" });

    EXPECT_EQ(ci.firstUnregisteredOption(), std::nullopt);
}

TEST(CommandInterpreterTest, FirstUnregisteredOptionNoneWhenAllRegistered)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save", "--file", "x.csv" });
    ci.addOption("save");
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.firstUnregisteredOption(), std::nullopt);
}

TEST(CommandInterpreterTest, FirstUnregisteredOptionReturnsNameWithDashes)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--typo" });

    EXPECT_EQ(ci.firstUnregisteredOption(), "--typo");
}

TEST(CommandInterpreterTest, FirstUnregisteredOptionReturnsFirstInCommandLineOrder)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save", "--zzz", "--aaa" });
    ci.addOption("save");

    EXPECT_EQ(ci.firstUnregisteredOption(), "--zzz");
}

TEST(CommandInterpreterTest, FirstUnregisteredOptionIgnoresValueOfRegisteredOption)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--file", "typo.csv" });
    ci.addNeedsValue("file");

    EXPECT_EQ(ci.firstUnregisteredOption(), std::nullopt);
}

TEST(CommandInterpreterTest, FirstUnregisteredOptionUpdatedAfterRegistration)
{
    auto ci = makeInterpreter({ "exe.exe", "command", "--save" });

    EXPECT_EQ(ci.firstUnregisteredOption(), "--save");

    ci.addOption("save");

    EXPECT_EQ(ci.firstUnregisteredOption(), std::nullopt);
}

TEST(CommandInterpreterTest, ValidateArgsAgreesWithFirstUnregisteredOption)
{
    auto ok = makeInterpreter({ "exe.exe", "command", "--save" });
    ok.addOption("save");
    auto ko = makeInterpreter({ "exe.exe", "command", "--sav" });
    ko.addOption("save");

    EXPECT_TRUE(ok.validateArgs());
    EXPECT_FALSE(ok.firstUnregisteredOption().has_value());
    EXPECT_FALSE(ko.validateArgs());
    EXPECT_TRUE(ko.firstUnregisteredOption().has_value());
}

// ============================================================================
// setCommandSpecs / getCommandSpec
// ============================================================================

TEST(CommandInterpreterTest, GetCommandSpecReturnsMatchingSpec)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6", "rpm" });
    ci.setCommandSpecs(&testSpecs);

    auto spec = ci.getCommandSpec();

    ASSERT_TRUE(spec);
    EXPECT_EQ(spec->name, "log");
    EXPECT_EQ(spec->valued.size(), 2u);
    EXPECT_EQ(spec->minFreeArgs, 1u);
    EXPECT_EQ(spec->maxFreeArgs, std::nullopt);
}

TEST(CommandInterpreterTest, GetCommandSpecUnknownCommand)
{
    auto ci = makeInterpreter({ "exe.exe", "lgo" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_FALSE(ci.getCommandSpec());
}

TEST(CommandInterpreterTest, GetCommandSpecNoCommand)
{
    auto ci = makeInterpreter({ "exe.exe" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_FALSE(ci.getCommandSpec());
}

TEST(CommandInterpreterTest, GetCommandSpecIsCaseSensitive)
{
    auto ci = makeInterpreter({ "exe.exe", "LOG" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_FALSE(ci.getCommandSpec());
}

TEST(CommandInterpreterTest, SetCommandSpecsReplacesPreviousSpecs)
{
    auto ci = makeInterpreter({ "exe.exe", "other" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_FALSE(ci.getCommandSpec());

    ci.setCommandSpecs(&otherSpecs);

    auto spec = ci.getCommandSpec();
    ASSERT_TRUE(spec);
    EXPECT_EQ(spec->name, "other");
}

// ============================================================================
// validateCommandLine
// ============================================================================

TEST(CommandInterpreterTest, ValidateCommandLineWithoutSpecsThrows)
{
    auto ci = makeInterpreter({ "exe.exe", "log" });

    EXPECT_THROW(ci.validateCommandLine(), std::logic_error);
}

TEST(CommandInterpreterTest, ValidateCommandLineNoCommand)
{
    auto ci = makeInterpreter({ "exe.exe" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "No command"));
}

TEST(CommandInterpreterTest, ValidateCommandLineUnknownCommandNamesIt)
{
    auto ci = makeInterpreter({ "exe.exe", "lgo" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'lgo'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineEmptySpecsRejectsEverything)
{
    const std::vector<CommandInterpreter::CommandSpec> empty;
    auto ci = makeInterpreter({ "exe.exe", "list" });
    ci.setCommandSpecs(&empty);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'list'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineSimpleCommandOk)
{
    auto ci = makeInterpreter({ "exe.exe", "list" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), std::nullopt);
}

TEST(CommandInterpreterTest, ValidateCommandLineFullLogCommandOk)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "--file", "t.csv", "rpm", "ect" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), std::nullopt);
}

TEST(CommandInterpreterTest, ValidateCommandLineFlagAndValueOk)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--interface", "A6000001", "--save" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), std::nullopt);
}

TEST(CommandInterpreterTest, ValidateCommandLineRegistersSpecOptions)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "--file", "t.csv", "rpm" });
    ci.setCommandSpecs(&testSpecs);

    ASSERT_EQ(ci.validateCommandLine(), std::nullopt);

    // After validation the valued options are registered: their values are no longer free args
    EXPECT_EQ(ci.getOption("interface"), "A6000001");
    EXPECT_EQ(ci.getOption("file"), "t.csv");
    ASSERT_EQ(ci.countFreeArgs(), 1u);
    EXPECT_EQ(ci.freeArg(0), "rpm");
}

TEST(CommandInterpreterTest, ValidateCommandLineTypoInValuedOption)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "--fiel", "t.csv", "rpm" });
    ci.setCommandSpecs(&testSpecs);

    auto error = ci.validateCommandLine();

    EXPECT_TRUE(errorContains(error, "'--fiel'"));
    EXPECT_TRUE(errorContains(error, "'log'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineTypoInFlag)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--interface", "A6000001", "--sav" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'--sav'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineRejectsOptionOfAnotherCommand)
{
    auto ci = makeInterpreter({ "exe.exe", "list", "--interface", "A6000001" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'--interface'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineFlagOfAnotherCommandRejected)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "--save", "rpm" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'--save'"));
}

TEST(CommandInterpreterTest, ValidateCommandLineTooFewFreeArgsUsesSpecMessage)
{
    auto ci = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), "You need to provide at least one variable to log!");
}

TEST(CommandInterpreterTest, ValidateCommandLineTooManyFreeArgsUsesSpecMessage)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--interface", "A6000001", "extra" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), "diag takes no free argument");
}

TEST(CommandInterpreterTest, ValidateCommandLineUnknownOptionReportedBeforeFreeArgs)
{
    // Two errors: unknown option AND extra free arg -> the option is reported first
    auto ci = makeInterpreter({ "exe.exe", "diag", "--sav", "extra" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_TRUE(errorContains(ci.validateCommandLine(), "'--sav'"));
}

// Current behavior: an option that needs a value passes validation without one,
// the caller has to check optionHasValue() (the logger does it for --interface and --file).
TEST(CommandInterpreterTest, ValidateCommandLineAcceptsValueOptionWithoutValue)
{
    auto ci = makeInterpreter({ "exe.exe", "diag", "--interface" });
    ci.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ci.validateCommandLine(), std::nullopt);
    EXPECT_TRUE(ci.hasOption("interface"));
    EXPECT_FALSE(ci.optionHasValue("interface"));
}

TEST(CommandInterpreterTest, ValidateCommandLineIsIdempotent)
{
    auto ok = makeInterpreter({ "exe.exe", "log", "--interface", "A6000001", "rpm" });
    ok.setCommandSpecs(&testSpecs);
    auto ko = makeInterpreter({ "exe.exe", "log", "--interfce", "A6000001", "rpm" });
    ko.setCommandSpecs(&testSpecs);

    EXPECT_EQ(ok.validateCommandLine(), ok.validateCommandLine());
    EXPECT_EQ(ko.validateCommandLine(), ko.validateCommandLine());
}