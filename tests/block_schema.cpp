#include "test_runner.hpp"
#include "block/definition.hpp"

// Utility Functions
inline Error ParseSchema(std::string fmt, BlockSchema &outSchema)
{
    BlockDefinition def;
    def.Fmt = std::move(fmt);

    Error err = GenerateBlockSchema(def);
    outSchema = std::move(def.Schema);
    return err;
}

// Tests
TEST(SchemaTextOnly)
{
    BlockSchema schema;
    Error err = ParseSchema("Hello World", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 1u);
    EXPECT(schema[0].Type == BlockSchemaType::Text);
    EXPECT_EQ(schema[0].Name, "Hello World");
}

TEST(SchemaAdjacentTextMergeIntoOneItem)
{
    BlockSchema schema;
    Error err = ParseSchema("ab{int:x}cd", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 3u);

    EXPECT(schema[0].Type == BlockSchemaType::Text);
    EXPECT_EQ(schema[0].Name, "ab");

    EXPECT(schema[1].Type == BlockSchemaType::Input);
    EXPECT_EQ(schema[1].Name, "x");
    EXPECT_EQ(schema[1].ValueType, VAL_INT);

    EXPECT(schema[2].Type == BlockSchemaType::Text);
    EXPECT_EQ(schema[2].Name, "cd");
}


TEST(SchemaInputInt)
{
    BlockSchema schema;
    Error err = ParseSchema("{int:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 1u);
    EXPECT(schema[0].Type == BlockSchemaType::Input);
    EXPECT_EQ(schema[0].Name, "x");
    EXPECT_EQ(schema[0].ValueType, VAL_INT);
}

TEST(SchemaInputFloat)
{
    BlockSchema schema;
    Error err = ParseSchema("{float:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema[0].ValueType, VAL_FLOAT);
}
 
TEST(SchemaInputBool)
{
    BlockSchema schema;
    Error err = ParseSchema("{bool:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema[0].ValueType, VAL_BOOL);
}

TEST(SchemaInputString)
{
    BlockSchema schema;
    Error err = ParseSchema("{string:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema[0].ValueType, VAL_STRING);
}

TEST(SchemaInputNumber)
{
    BlockSchema schema;
    Error err = ParseSchema("{number:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema[0].ValueType, VAL_NUMBER);
}

TEST(SchemaInputAny)
{
    BlockSchema schema;
    Error err = ParseSchema("{any:x}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema[0].ValueType, VAL_ANY);
}

TEST(SchemaVariableArg)
{
    BlockSchema schema;
    Error err = ParseSchema("{any:$var}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 1u);
    EXPECT(schema[0].Type == BlockSchemaType::Var);
    EXPECT_EQ(schema[0].Name, "var");
    EXPECT_EQ(schema[0].ValueType, VAL_ANY);
}

TEST(SchemaVariableArgCannotHaveDefaultValue)
{
    BlockSchema schema;
    Error err = ParseSchema("{any:$var=5}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaBodySlot)
{
    BlockSchema schema;
    Error err = ParseSchema("{body:then}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 1u);
    EXPECT(schema[0].Type == BlockSchemaType::Body);
    EXPECT_EQ(schema[0].Name, "then");
}

TEST(SchemaBodySlotCannotBeVariable)
{
    BlockSchema schema;
    Error err = ParseSchema("{body:$then}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}
 
TEST(SchemaBodySlotCannotHaveDefaultValue)
{
    BlockSchema schema;
    Error err = ParseSchema("{body:then=5}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaMultipleBodySlots)
{
    BlockSchema schema;
    Error err = ParseSchema("If {bool:cond=true} {body:then} else {body:else}", schema);

    EXPECT(err == Error::Ok);

    auto slots = BlockSchemaBodySlots(schema);
    EXPECT_EQ(slots.size(), 2u);
    EXPECT_EQ(slots[0], "then");
    EXPECT_EQ(slots[1], "else");
}

TEST(SchemaBodySlotsEmptyWhenNoneDeclared)
{
    BlockSchema schema;
    Error err = ParseSchema("Print {any:out}", schema);

    EXPECT(err == Error::Ok);

    auto slots = BlockSchemaBodySlots(schema);
    EXPECT_EQ(slots.size(), 0u);
}

TEST(SchemaMixedFmt)
{
    BlockSchema schema;
    Error err = ParseSchema("Print {any:out='Hello World'}", schema);

    EXPECT(err == Error::Ok);
    EXPECT_EQ(schema.size(), 2u);

    EXPECT(schema[0].Type == BlockSchemaType::Text);
    EXPECT_EQ(schema[0].Name, "Print ");

    EXPECT(schema[1].Type == BlockSchemaType::Input);
    EXPECT_EQ(schema[1].Name, "out");
    EXPECT_EQ(schema[1].ValueType, VAL_ANY);
}

TEST(SchemaEmptyFmtFails)
{
    BlockSchema schema;
    Error err = ParseSchema("", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaUnknownTypeFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{foo:x}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaMissingTypeFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{:x}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaMissingNameFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{int:}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaUnclosedBraceFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{int:x", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaUnexpectedColonFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{int:a:b}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}

TEST(SchemaUnexpectedEqualsFails)
{
    BlockSchema schema;
    Error err = ParseSchema("{int:a=1=2}", schema);

    EXPECT(err == Error::BlockInvalidFmt);
}
