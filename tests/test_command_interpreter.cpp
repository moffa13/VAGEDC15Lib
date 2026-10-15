#include "Core/CommandInterpreter.h"

#include <gtest/gtest.h>
#include <array>
#include <optional>

TEST(CommandInterpreterTest, NoArgs)
{


	constexpr int argc = 1;
	std::array < const char*, argc > argv = { "exe.exe" };


	CommandInterpreter ci{ argc, argv.data() };


	EXPECT_EQ(ci.countFreeArgs(), 0);
	EXPECT_EQ(ci.hasArgs(), false);
}

TEST(CommandInterpreterTest, NoFreeArgs)
{


	constexpr int argc = 3;
	std::array < const char*, argc > argv = { "exe.exe", "command", "--arg1"};


	CommandInterpreter ci{ argc, argv.data() };


	EXPECT_EQ(ci.countFreeArgs(), 0);
	EXPECT_EQ(ci.hasArgs(), true);
}

TEST(CommandInterpreterTest, OptArgMissingValue)
{


	constexpr int argc = 5;
	std::array < const char*, argc > argv = { "exe.exe", "command", "--file", "--save", "opt1"};


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");


	EXPECT_EQ(ci.countFreeArgs(), 1);
	EXPECT_EQ(ci.hasArgs(), true);
	EXPECT_EQ(ci.countOptArgs(), 2);
}

TEST(CommandInterpreterTest, OptArgMissingValueLastElem)
{


	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "command", "--save", "--file"};


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");


	EXPECT_EQ(ci.countFreeArgs(), 0);
	EXPECT_EQ(ci.hasArgs(), true);
	EXPECT_EQ(ci.countOptArgs(), 2);
	EXPECT_EQ(ci.hasOption("file"), true);
	EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, ContiguousFreeArgs)
{


	constexpr int argc = 4;
	std::array < const char*, argc > argv = {"exe.exe", "--test", "opt1", "opt2"};


	CommandInterpreter ci{ argc, argv.data() };


	EXPECT_EQ(ci.countFreeArgs(), 2);
	EXPECT_EQ(ci.hasArgs(), true);
}

TEST(CommandInterpreterTest, NotContiguousFreeArgs)
{


	constexpr int argc = 6;
	std::array < const char*, argc > argv = { "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3"};


	CommandInterpreter ci{ argc, argv.data() };


	EXPECT_EQ(ci.countFreeArgs(), 2);
	EXPECT_EQ(ci.hasArgs(), true);
}

TEST(CommandInterpreterTest, OptionWithValue)
{


	constexpr int argc = 6;
	std::array < const char*, argc > argv = { "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3"};


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("test2");

	EXPECT_EQ(ci.countFreeArgs(), 1);
	EXPECT_EQ(ci.countOptArgs(), 3);
	EXPECT_EQ(ci.hasArgs(), true);
}

TEST(CommandInterpreterTest, freeArgsArray)
{


	constexpr int argc = 6;
	std::array < const char*, argc > argv = { "exe.exe", "--test", "opt1", "--test2", "opt2", "--test3" };


	CommandInterpreter ci{ argc, argv.data() };

	const auto& freeargs = ci.freeArgs();

	EXPECT_EQ(freeargs.size(), 2);
	EXPECT_EQ(freeargs[0], "opt1");
	EXPECT_EQ(freeargs[1], "opt2");
}

TEST(CommandInterpreterTest, OptionNeedingValueAtEnd)
{


	constexpr int argc = 3;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");

	EXPECT_EQ(ci.countOptArgs(), 1);
	EXPECT_EQ(ci.countFreeArgs(), 0);
	EXPECT_EQ(ci.hasOption("file"), true);
	EXPECT_EQ(ci.optionHasValue("file"), false);
	EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, OptionValueIsAnotherOption)
{


	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file", "--save" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");

	const auto& optargs = ci.optArgs();

	EXPECT_EQ(optargs.size(), 2);
	EXPECT_EQ(optargs[0].first, "--file");
	EXPECT_EQ(optargs[0].second, std::nullopt);
	EXPECT_EQ(optargs[1].first, "--save");
	EXPECT_EQ(ci.hasOption("save"), true);
	EXPECT_EQ(ci.countFreeArgs(), 0);
}

TEST(CommandInterpreterTest, OptArgsContent)
{


	constexpr int argc = 7;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--interface", "A6000001", "--save", "rpm", "ect" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("interface");

	const auto& optargs = ci.optArgs();

	EXPECT_EQ(optargs.size(), 2);
	EXPECT_EQ(optargs[0].first, "--interface");
	EXPECT_EQ(optargs[0].second, "A6000001");
	EXPECT_EQ(optargs[1].second, std::nullopt);
	EXPECT_EQ(ci.countFreeArgs(), 2);
	EXPECT_EQ(ci.freeArg(0), "rpm");
	EXPECT_EQ(ci.freeArg(1), "ect");
	EXPECT_EQ(ci.isCommand("log"), true);
}

TEST(CommandInterpreterTest, NeedsValueDeclaredAfterFirstParse)
{


	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file", "out.csv" };


	CommandInterpreter ci{ argc, argv.data() };

	EXPECT_EQ(ci.countFreeArgs(), 1);

	ci.addNeedsValue("file");

	EXPECT_EQ(ci.countFreeArgs(), 0);
	EXPECT_EQ(ci.getOption("file"), "out.csv");
}

TEST(CommandInterpreterTest, GetOptionOnOptionThatTakesNoValue)
{


	constexpr int argc = 3;
	std::array < const char*, argc > argv = { "exe.exe", "diag", "--save" };


	CommandInterpreter ci{ argc, argv.data() };

	EXPECT_THROW(ci.getOption("save"), std::runtime_error);
}

TEST(CommandInterpreterTest, GetOptionNotPresent)
{


	constexpr int argc = 2;
	std::array < const char*, argc > argv = { "exe.exe", "log" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");

	EXPECT_EQ(ci.hasOption("file"), false);
	//Doesn't throw anymore, returns nullopt
	// Easier for option checking
	//EXPECT_THROW(ci.getOption("file"), std::runtime_error);
	EXPECT_EQ(ci.getOption("file"), std::nullopt);
}

TEST(CommandInterpreterTest, OptionNameWithAndWithoutDashes)
{


	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file", "out.csv" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("--file");

	EXPECT_EQ(ci.hasOption("file"), ci.hasOption("--file"));
	EXPECT_EQ(ci.getOption("file"), ci.getOption("--file"));
	EXPECT_EQ(ci.optionNeedsValue("file"), true);
}

TEST(CommandInterpreterTest, DuplicatedOption)
{


	constexpr int argc = 6;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file", "a.csv", "--file", "b.csv" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");

	EXPECT_EQ(ci.countOptArgs(), 2);
	EXPECT_EQ(ci.getOption("file"), "a.csv");
	EXPECT_EQ(ci.countFreeArgs(), 0);
}

TEST(CommandInterpreterTest, CommandOnly)
{


	constexpr int argc = 2;
	std::array < const char*, argc > argv = { "exe.exe", "list" };


	CommandInterpreter ci{ argc, argv.data() };

	EXPECT_EQ(ci.getCommand(), "list");
	EXPECT_EQ(ci.isCommand("list"), true);
	EXPECT_EQ(ci.isCommand("list-pid"), false);
	EXPECT_EQ(ci.hasArgs(), false);
	EXPECT_EQ(ci.countOptArgs(), 0);
}

TEST(CommandInterpreterTest, NoCommand)
{


	constexpr int argc = 1;
	std::array < const char*, argc > argv = { "exe.exe" };


	CommandInterpreter ci{ argc, argv.data() };

	EXPECT_EQ(ci.getCommand(), std::nullopt);
	EXPECT_EQ(ci.isCommand("list"), false);
}

TEST(CommandInterpreterTest, OptionBeforeCommand)
{


	constexpr int argc = 5;
	std::array < const char*, argc > argv = { "exe.exe", "--interface", "A6000001", "log", "rpm" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("interface");

	EXPECT_EQ(ci.getCommand(), "--interface");
	EXPECT_EQ(ci.countFreeArgs(), 2);
}

TEST(CommandInterpreterTest, FreeArgsStableAcrossCalls)
{


	constexpr int argc = 5;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--file", "out.csv", "rpm" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("file");

	EXPECT_EQ(ci.freeArgs().size(), ci.freeArgs().size());
	EXPECT_EQ(ci.countFreeArgs(), ci.freeArgs().size());
	EXPECT_EQ(ci.countOptArgs(), ci.optArgs().size());
}

TEST(CommandInterpreterTest, HexTestNormal)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "0x1234F" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), 0x1234F);
}

TEST(CommandInterpreterTest, HexTestNo0X)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "ABCD" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), 0xABCD);
}

TEST(CommandInterpreterTest, HexTestEndsWithH)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "DEADh" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), 0xDEAD);
}

TEST(CommandInterpreterTest, HexTestMalformed)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "0xDEADh" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_THROW(*ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestMalformed2)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "hello" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_THROW(*ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestMalformed3)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "0xhello" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_THROW(*ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestMalformed4)
{

	constexpr int argc = 4;
	std::array < const char*, argc > argv = { "exe.exe", "log", "--addr", "0x" };


	CommandInterpreter ci{ argc, argv.data() };

	ci.addNeedsValue("addr");

	EXPECT_THROW(*ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestOnlyH)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "h" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestEmptyValue)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestTrailingGarbage)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "8C420XYZ" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestNegativeSign)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "-1234" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestOptionAbsent)
{
	constexpr int argc = 2;
	std::array<const char*, argc> argv = { "exe.exe", "log" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_FALSE(ci.getOptionHex("addr").has_value());
}

TEST(CommandInterpreterTest, HexTestOptionWithoutValue)
{
	constexpr int argc = 3;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_FALSE(ci.getOptionHex("addr").has_value());
}

TEST(CommandInterpreterTest, HexTestOptionNotDeclared)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "0x1234" };

	CommandInterpreter ci{ argc, argv.data() };

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestZero)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "0" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), 0u);
}

TEST(CommandInterpreterTest, HexTestMaxValue)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "0xFFFFFFFFFFFFFFFF" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), std::numeric_limits<size_t>::max());
}

TEST(CommandInterpreterTest, HexTestOverflow)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "0x1FFFFFFFFFFFFFFFF" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}


TEST(CommandInterpreterTest, HexTestLowercaseDigits)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "abcd" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_EQ(*ci.getOptionHex("addr"), 0xABCDu);
}

TEST(CommandInterpreterTest, HexTestUppercasePrefix)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "0X1234F" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}

TEST(CommandInterpreterTest, HexTestUppercaseHSuffix)
{
	constexpr int argc = 4;
	std::array<const char*, argc> argv = { "exe.exe", "log", "--addr", "DEADH" };

	CommandInterpreter ci{ argc, argv.data() };
	ci.addNeedsValue("addr");

	EXPECT_THROW(ci.getOptionHex("addr"), std::runtime_error);
}
