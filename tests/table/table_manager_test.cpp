#include "table/table_manager.h"
#include "catalog/catalog.h"
#include "common/definitions.h"
#include "core/row_reader.h"
#include "gtest/gtest.h"
#include <atomic>
#include <string>
#include <vector>

class TableManagerTest : public ::testing::TestWithParam<TableType> {
protected:
    std::unique_ptr<TableManager> table_manager;
    std::atomic<uint32_t> counter_m { 0 };
    std::vector<std::string> created_tables_m;

    void SetUp() override
    {
        table_manager = std::make_unique<TableManager>(GetParam());
    }

    void TearDown() override
    {
        for (const auto& table_name : created_tables_m) {
            if (table_manager->TableExists(table_name))
                table_manager->DeleteTable(table_name);
        }
        created_tables_m.clear();
        table_manager.reset();
        counter_m = 0;
    }

    std::string GenerateUniqueTableName()
    {
        return "test_table_" + std::to_string(counter_m.fetch_add(1));
    }

    Schema CreateSimpleSchema()
    {
        return { { "id", type_id<I32> } };
    }

    Schema CreateComplexSchema()
    {
        return {
            { "id", type_id<I32> },
            { "name", type_id<String> },
            { "value", type_id<I32> },
            { "flag", type_id<I32> }
        };
    }

    bool CreateAndTrackTable(std::string_view name, TableType type, const Schema& schema)
    {
        if (table_manager->CreateTable(name, type, schema)) {
            created_tables_m.push_back(std::string(name));
            return true;
        }
        return false;
    }
};

// --- Core CRUD ---

TEST_P(TableManagerTest, CreateAndVerifyTableExists)
{
    auto name = GenerateUniqueTableName();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), CreateSimpleSchema()));
    ASSERT_TRUE(table_manager->TableExists(name));
}

TEST_P(TableManagerTest, GetTableAfterCreation)
{
    auto name = GenerateUniqueTableName();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), CreateSimpleSchema()));
    ASSERT_TRUE(table_manager->GetTable(name));
}

TEST_P(TableManagerTest, TableDoesNotExistInitially)
{
    ASSERT_FALSE(table_manager->TableExists(GenerateUniqueTableName()));
}

TEST_P(TableManagerTest, DuplicateTableCreation)
{
    auto name = GenerateUniqueTableName();
    auto schema = CreateSimpleSchema();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), schema));
    ASSERT_FALSE(table_manager->CreateTable(name, GetParam(), schema));
}

TEST_P(TableManagerTest, DeleteTable)
{
    auto name = GenerateUniqueTableName();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), CreateSimpleSchema()));
    ASSERT_TRUE(table_manager->DeleteTable(name));
    ASSERT_FALSE(table_manager->TableExists(name));
}

TEST_P(TableManagerTest, DeleteNonExistentTable)
{
    ASSERT_FALSE(table_manager->DeleteTable(GenerateUniqueTableName()));
}

// --- Schema ---

TEST_P(TableManagerTest, SchemaVerification)
{
    auto name = GenerateUniqueTableName();
    auto schema = CreateComplexSchema();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), schema));

    auto table = table_manager->GetTable(name);
    ASSERT_TRUE(table);

    const auto& retrieved = table->GetSchema();
    ASSERT_EQ(retrieved.size(), schema.size());
    for (std::size_t i = 0; i < schema.size(); ++i) {
        EXPECT_EQ(retrieved[i].name, schema[i].name);
        EXPECT_EQ(retrieved[i].type, schema[i].type);
    }
}

// --- Lifecycle ---

TEST_P(TableManagerTest, CompleteTableLifecycle)
{
    auto name = GenerateUniqueTableName();
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), CreateComplexSchema()));
    ASSERT_TRUE(table_manager->TableExists(name));
    ASSERT_TRUE(table_manager->GetTable(name));

    ASSERT_TRUE(table_manager->DeleteTable(name));
    ASSERT_FALSE(table_manager->TableExists(name));
}

TEST_P(TableManagerTest, DeleteAndRecreate)
{
    auto name = GenerateUniqueTableName();
    auto schema = CreateSimpleSchema();

    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), schema));
    ASSERT_TRUE(table_manager->DeleteTable(name));
    ASSERT_FALSE(table_manager->TableExists(name));

    // Recreate with the same name — should succeed and produce a clean table
    ASSERT_TRUE(table_manager->CreateTable(name, GetParam(), schema));
    ASSERT_TRUE(table_manager->TableExists(name));

    auto table = table_manager->GetTable(name);
    ASSERT_TRUE(table);

    auto iter = table->begin();
    EXPECT_TRUE(iter == std::default_sentinel_t {}); // empty after recreation
}

// --- Data round-trips ---

TEST_P(TableManagerTest, DataRoundTrip)
{
    auto name = GenerateUniqueTableName();
    Schema schema = { { "id", type_id<I32> }, { "label", type_id<String> } };
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), schema));

    auto table = table_manager->GetTable(name);
    ASSERT_TRUE(table);

    table->insert_row({ Value(I32 { 1 }), Value(String { "alpha" }) });
    table->insert_row({ Value(I32 { 2 }), Value(String { "beta" }) });

    // Read back through a fresh handle to verify persistence
    auto handle = table_manager->GetTable(name);
    ASSERT_TRUE(handle);
    auto iter = handle->begin();
    ASSERT_FALSE(iter == std::default_sentinel_t {});
    RowReader rr1((*iter).second, schema);
    EXPECT_EQ(rr1.Get<I32>(0).value, 1);
    EXPECT_EQ(rr1.Get<String>(1).value, "alpha");

    ++iter;
    ASSERT_FALSE(iter == std::default_sentinel_t {});
    RowReader rr2((*iter).second, schema);
    EXPECT_EQ(rr2.Get<I32>(0).value, 2);
    EXPECT_EQ(rr2.Get<String>(1).value, "beta");

    ++iter;
    EXPECT_TRUE(iter == std::default_sentinel_t {});
}

TEST_P(TableManagerTest, MultipleRowsRoundTrip)
{
    auto name = GenerateUniqueTableName();
    auto schema = CreateComplexSchema(); // id, name, value, flag
    ASSERT_TRUE(CreateAndTrackTable(name, GetParam(), schema));

    auto table = table_manager->GetTable(name);
    ASSERT_TRUE(table);

    constexpr int ROW_COUNT = 10;
    for (int i = 0; i < ROW_COUNT; ++i) {
        table->insert_row({
            Value(I32 { std::int32_t(i) }),
            Value(String { std::string("name_") + std::to_string(i) }),
            Value(I32 { i * 10 }),
            Value(I32 { i % 2 }),
        });
    }

    int count = 0;
    for (auto [row_id, row_data] : *table) {
        RowReader rr(row_data, schema);
        EXPECT_EQ(rr.Get<I32>(0).value, count);
        EXPECT_EQ(rr.Get<String>(1).value, std::string("name_") + std::to_string(count));
        EXPECT_EQ(rr.Get<I32>(2).value, count * 10);
        EXPECT_EQ(rr.Get<I32>(3).value, count % 2);
        ++count;
    }
    EXPECT_EQ(count, ROW_COUNT);
}

// --- Multi-table ---

TEST_P(TableManagerTest, MultipleTablesCoexist)
{
    auto name1 = GenerateUniqueTableName();
    auto name2 = GenerateUniqueTableName();
    auto schema = CreateSimpleSchema();

    ASSERT_TRUE(CreateAndTrackTable(name1, GetParam(), schema));
    ASSERT_TRUE(CreateAndTrackTable(name2, GetParam(), schema));

    ASSERT_TRUE(table_manager->TableExists(name1));
    ASSERT_TRUE(table_manager->TableExists(name2));

    // Deleting one must not affect the other
    ASSERT_TRUE(table_manager->DeleteTable(name1));
    ASSERT_FALSE(table_manager->TableExists(name1));
    ASSERT_TRUE(table_manager->TableExists(name2));
    ASSERT_TRUE(table_manager->GetTable(name2));
}

// --- Cross-backend ---

TEST_P(TableManagerTest, CrossBackendTableCreation)
{
    auto name = GenerateUniqueTableName();
    auto schema = CreateSimpleSchema();
    TableType other = (GetParam() == TableType::Disk) ? TableType::InMemory : TableType::Disk;

    ASSERT_TRUE(CreateAndTrackTable(name, other, schema));
    ASSERT_TRUE(table_manager->TableExists(name));
    ASSERT_TRUE(table_manager->GetTable(name));
}

INSTANTIATE_TEST_SUITE_P(
    TableManagerInstantiation,
    TableManagerTest,
    ::testing::Values(TableType::InMemory, TableType::Disk));
