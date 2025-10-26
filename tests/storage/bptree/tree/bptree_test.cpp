#include "config/config.h"
#include "storage/bptree/buffer/page_buffer_manager.h"
#include "storage/bptree/tree/bptree.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

/**
 * \brief Test fixture for BPTree tests
 */
class BPTreeTest : public ::testing::Test {
protected:
    static constexpr int number_frames = 100;
    PageBufferManager page_buffer_man { number_frames };

    // Helper function to convert string to span
    std::span<const char> to_span(const std::string& str)
    {
        return std::span<const char>(str.data(), str.size());
    }

    // Helper function to compare spans with strings
    bool span_equals(std::span<const char> span, const std::string& str)
    {
        if (span.size() != str.size()) {
            return false;
        }
        return std::equal(span.begin(), span.end(), str.begin());
    }
};

/**
 * \brief Test creating a new B+ tree and basic insertion/search
 */
TEST_F(BPTreeTest, TestCreateAndInsertSingle)
{
    BPTree tree(&page_buffer_man);

    std::string key = "key1";
    std::string value = "value1";

    tree.Insert(to_span(key), to_span(value));

    auto result = tree.Search(to_span(key));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(span_equals(*result, value));
}

/**
 * \brief Test searching for a non-existent key
 */
TEST_F(BPTreeTest, TestSearchNonExistent)
{
    BPTree tree(&page_buffer_man);

    std::string key = "nonexistent";
    auto result = tree.Search(to_span(key));
    EXPECT_FALSE(result.has_value());
}

/**
 * \brief Test multiple insertions and searches
 */
TEST_F(BPTreeTest, TestMultipleInsertions)
{
    BPTree tree(&page_buffer_man);

    const int num_entries = 10;
    for (int i = 0; i < num_entries; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        tree.Insert(to_span(key), to_span(value));
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_TRUE(span_equals(*result, expected_value))
            << "Value mismatch for key: " << key;
    }
}

/**
 * \brief Test sequential insertions (sorted order)
 */
TEST_F(BPTreeTest, TestSequentialInsertions)
{
    BPTree tree(&page_buffer_man);

    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++) {
        // Use zero-padded keys to ensure lexicographic order matches numeric order
        char key_buf[32];
        char value_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);
        std::snprintf(value_buf, sizeof(value_buf), "value%05d", i);

        std::string key(key_buf);
        std::string value(value_buf);

        tree.Insert(to_span(key), to_span(value));
    }

    // Verify all entries
    for (int i = 0; i < num_entries; i++) {
        char key_buf[32];
        char value_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);
        std::snprintf(value_buf, sizeof(value_buf), "value%05d", i);

        std::string key(key_buf);
        std::string expected_value(value_buf);

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_TRUE(span_equals(*result, expected_value));
    }
}

/**
 * \brief Test random insertions
 */
TEST_F(BPTreeTest, TestRandomInsertions)
{
    BPTree tree(&page_buffer_man);

    const int num_entries = 100;
    std::vector<int> indices(num_entries);
    std::iota(indices.begin(), indices.end(), 0);

    // Shuffle indices to create random insertion order
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    // Insert in random order
    for (int idx : indices) {
        char key_buf[32];
        char value_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", idx);
        std::snprintf(value_buf, sizeof(value_buf), "value%05d", idx);

        std::string key(key_buf);
        std::string value(value_buf);

        tree.Insert(to_span(key), to_span(value));
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++) {
        char key_buf[32];
        char value_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);
        std::snprintf(value_buf, sizeof(value_buf), "value%05d", i);

        std::string key(key_buf);
        std::string expected_value(value_buf);

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_TRUE(span_equals(*result, expected_value));
    }
}

/**
 * \brief Test updating an existing key with a new value
 */
TEST_F(BPTreeTest, TestUpdateValue)
{
    BPTree tree(&page_buffer_man);

    std::string key = "mykey";
    std::string value1 = "value1";
    std::string value2 = "value2_updated";

    // Insert initial value
    tree.Insert(to_span(key), to_span(value1));

    auto result1 = tree.Search(to_span(key));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(span_equals(*result1, value1));

    // Update with new value
    tree.Insert(to_span(key), to_span(value2));

    auto result2 = tree.Search(to_span(key));
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(span_equals(*result2, value2));
}

/**
 * \brief Test deleting a single entry
 */
TEST_F(BPTreeTest, TestDeleteSingle)
{
    BPTree tree(&page_buffer_man);

    std::string key = "key1";
    std::string value = "value1";

    tree.Insert(to_span(key), to_span(value));

    // Verify it exists
    auto result1 = tree.Search(to_span(key));
    ASSERT_TRUE(result1.has_value());

    // Delete it
    tree.Delete(to_span(key));

    // Verify it's gone
    auto result2 = tree.Search(to_span(key));
    EXPECT_FALSE(result2.has_value());
}

/**
 * \brief Test deleting non-existent key (should not crash)
 */
TEST_F(BPTreeTest, TestDeleteNonExistent)
{
    BPTree tree(&page_buffer_man);

    std::string key = "nonexistent";

    // This should not crash or throw
    EXPECT_NO_THROW(tree.Delete(to_span(key)));
}

/**
 * \brief Test multiple deletions
 */
TEST_F(BPTreeTest, TestMultipleDeletions)
{
    BPTree tree(&page_buffer_man);

    const int num_entries = 20;

    // Insert entries
    for (int i = 0; i < num_entries; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        tree.Insert(to_span(key), to_span(value));
    }

    // Delete every other entry
    for (int i = 0; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        tree.Delete(to_span(key));
    }

    // Verify deleted entries are gone
    for (int i = 0; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        auto result = tree.Search(to_span(key));
        EXPECT_FALSE(result.has_value()) << "Key should be deleted: " << key;
    }

    // Verify remaining entries still exist
    for (int i = 1; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key should still exist: " << key;
        EXPECT_TRUE(span_equals(*result, expected_value));
    }
}

/**
 * \brief Test reopening an existing B+ tree
 *
 * NOTE: This test is disabled because BPTree interface doesn't expose GetRootPageId().
 * To enable this test, add a GetRootPageId() method to the BPTree class.
 */
TEST_F(BPTreeTest, DISABLED_TestReopenExistingTree)
{
    page_id_t root_page_id;

    // Create tree and insert data
    {
        BPTree tree(&page_buffer_man);
        // root_page_id = tree.GetRootPageId(); // Method doesn't exist yet

        for (int i = 0; i < 10; i++) {
            std::string key = "key" + std::to_string(i);
            std::string value = "value" + std::to_string(i);
            tree.Insert(to_span(key), to_span(value));
        }
    }

    // Reopen the tree using the root page ID
    {
        BPTree tree(&page_buffer_man, root_page_id);

        // Verify all entries still exist
        for (int i = 0; i < 10; i++) {
            std::string key = "key" + std::to_string(i);
            std::string expected_value = "value" + std::to_string(i);

            auto result = tree.Search(to_span(key));
            ASSERT_TRUE(result.has_value()) << "Key not found after reopening: " << key;
            EXPECT_TRUE(span_equals(*result, expected_value));
        }
    }
}

/**
 * \brief Test with large values
 */
TEST_F(BPTreeTest, TestLargeValues)
{
    BPTree tree(&page_buffer_man);

    std::string key = "large_value_key";
    std::string large_value(1000, 'X'); // 1000 character value

    tree.Insert(to_span(key), to_span(large_value));

    auto result = tree.Search(to_span(key));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(span_equals(*result, large_value));
}

/**
 * \brief Test with empty key
 */
TEST_F(BPTreeTest, TestEmptyKey)
{
    BPTree tree(&page_buffer_man);

    std::string empty_key = "";
    std::string value = "value_for_empty_key";

    tree.Insert(to_span(empty_key), to_span(value));

    auto result = tree.Search(to_span(empty_key));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(span_equals(*result, value));
}

/**
 * \brief Test with empty value
 */
TEST_F(BPTreeTest, TestEmptyValue)
{
    BPTree tree(&page_buffer_man);

    std::string key = "key_with_empty_value";
    std::string empty_value = "";

    tree.Insert(to_span(key), to_span(empty_value));

    auto result = tree.Search(to_span(key));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(span_equals(*result, empty_value));
}

/**
 * \brief Stress test with many insertions
 */
TEST_F(BPTreeTest, TestStressInsertions)
{
    BPTree tree(&page_buffer_man);

    const int num_entries = 1000;
    for (int i = 0; i < num_entries; i++) {
        char key_buf[32];
        char value_buf[64];
        std::snprintf(key_buf, sizeof(key_buf), "stress_key_%06d", i);
        std::snprintf(value_buf, sizeof(value_buf), "stress_value_%06d_data", i);

        std::string key(key_buf);
        std::string value(value_buf);

        tree.Insert(to_span(key), to_span(value));
    }

    // Sample verify (check every 10th entry to keep test fast)
    for (int i = 0; i < num_entries; i += 10) {
        char key_buf[32];
        char value_buf[64];
        std::snprintf(key_buf, sizeof(key_buf), "stress_key_%06d", i);
        std::snprintf(value_buf, sizeof(value_buf), "stress_value_%06d_data", i);

        std::string key(key_buf);
        std::string expected_value(value_buf);

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_TRUE(span_equals(*result, expected_value));
    }
}

/**
 * \brief Test insertion and deletion interleaved
 */
TEST_F(BPTreeTest, TestInsertDeleteInterleaved)
{
    BPTree tree(&page_buffer_man);

    for (int round = 0; round < 5; round++) {
        // Insert 20 entries
        for (int i = 0; i < 20; i++) {
            std::string key = "key_" + std::to_string(round) + "_" + std::to_string(i);
            std::string value = "value_" + std::to_string(round) + "_" + std::to_string(i);
            tree.Insert(to_span(key), to_span(value));
        }

        // Delete 10 entries
        for (int i = 0; i < 10; i++) {
            std::string key = "key_" + std::to_string(round) + "_" + std::to_string(i);
            tree.Delete(to_span(key));
        }

        // Verify remaining 10 entries exist
        for (int i = 10; i < 20; i++) {
            std::string key = "key_" + std::to_string(round) + "_" + std::to_string(i);
            std::string expected_value = "value_" + std::to_string(round) + "_" + std::to_string(i);

            auto result = tree.Search(to_span(key));
            ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
            EXPECT_TRUE(span_equals(*result, expected_value));
        }
    }
}
