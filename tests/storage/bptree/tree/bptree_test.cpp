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
 *
 * Note: B+ Tree now stores keys -> record_id_t (not arbitrary values).
 * Tests use record_id_t values instead of string values.
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
};

/**
 * \brief Test creating a new B+ tree and basic insertion/search
 */
TEST_F(BPTreeTest, TestCreateAndInsertSingle)
{
    BPTree tree(&page_buffer_man);

    std::string key = "key1";
    record_id_t record_id = { 42, 7 };

    tree.Insert(to_span(key), record_id);

    auto result = tree.Search(to_span(key));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, record_id);
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
        record_id_t record_id = {100 + static_cast<uint32_t>(i), 0};
        tree.Insert(to_span(key), record_id);
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++) {
        std::string key = "key" + std::to_string(i);
        record_id_t expected_record_id = {100 + static_cast<uint32_t>(i), 0};

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_EQ(*result, expected_record_id) << "Value mismatch for key: " << key;
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
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);

        std::string key(key_buf);
        record_id_t record_id = {1000 + static_cast<uint32_t>(i), 0};

        tree.Insert(to_span(key), record_id);
    }

    // Verify all entries
    for (int i = 0; i < num_entries; i++) {
        char key_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);

        std::string key(key_buf);
        record_id_t expected_record_id = {1000 + static_cast<uint32_t>(i), 0};

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_EQ(*result, expected_record_id);
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
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", idx);

        std::string key(key_buf);
        record_id_t record_id = {2000 + static_cast<uint32_t>(idx), 0};

        tree.Insert(to_span(key), record_id);
    }

    // Verify all entries can be found
    for (int i = 0; i < num_entries; i++) {
        char key_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "key%05d", i);

        std::string key(key_buf);
        record_id_t expected_record_id = {2000 + static_cast<uint32_t>(i), 0};

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_EQ(*result, expected_record_id);
    }
}

/**
 * \brief Test updating an existing key with a new value
 */
TEST_F(BPTreeTest, TestUpdateValue)
{
    BPTree tree(&page_buffer_man);

    std::string key = "mykey";
    record_id_t record_id1 = {500, 0};
    record_id_t record_id2 = {999, 0};

    // Insert initial value
    tree.Insert(to_span(key), record_id1);

    auto result1 = tree.Search(to_span(key));
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, record_id1);

    // Update with new value
    tree.Insert(to_span(key), record_id2);

    auto result2 = tree.Search(to_span(key));
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, record_id2);
}

/**
 * \brief Test deleting a single entry
 */
TEST_F(BPTreeTest, TestDeleteSingle)
{
    BPTree tree(&page_buffer_man);

    std::string key = "key1";
    record_id_t record_id = {42, 0};

    tree.Insert(to_span(key), record_id);

    // Verify it exists
    auto result1 = tree.Search(to_span(key));
    ASSERT_TRUE(result1.has_value());

    // Delete it
    tree.Delete(to_span(key));

    // Verify it's gone (will fail until Delete is implemented)
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
        record_id_t record_id = {3000 + static_cast<uint32_t>(i), 0};
        tree.Insert(to_span(key), record_id);
    }

    // Delete every other entry
    for (int i = 0; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        tree.Delete(to_span(key));
    }

    // Verify deleted entries are gone (will fail until Delete is implemented)
    for (int i = 0; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        auto result = tree.Search(to_span(key));
        EXPECT_FALSE(result.has_value()) << "Key should be deleted: " << key;
    }

    // Verify remaining entries still exist
    for (int i = 1; i < num_entries; i += 2) {
        std::string key = "key" + std::to_string(i);
        record_id_t expected_record_id = {3000 + static_cast<uint32_t>(i), 0};

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key should still exist: " << key;
        EXPECT_EQ(*result, expected_record_id);
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
            record_id_t record_id = {4000 + static_cast<uint32_t>(i), 0};
            tree.Insert(to_span(key), record_id);
        }
    }

    // Reopen the tree using the root page ID
    {
        BPTree tree(&page_buffer_man, root_page_id);

        // Verify all entries still exist
        for (int i = 0; i < 10; i++) {
            std::string key = "key" + std::to_string(i);
            record_id_t expected_record_id = {4000 + static_cast<uint32_t>(i), 0};

            auto result = tree.Search(to_span(key));
            ASSERT_TRUE(result.has_value()) << "Key not found after reopening: " << key;
            EXPECT_EQ(*result, expected_record_id);
        }
    }
}

/**
 * \brief Test with long keys
 */
TEST_F(BPTreeTest, TestLongKeys)
{
    BPTree tree(&page_buffer_man);

    std::string long_key(200, 'X'); // 200 character key
    record_id_t record_id = {5000, 0};

    tree.Insert(to_span(long_key), record_id);

    auto result = tree.Search(to_span(long_key));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, record_id);
}

/**
 * \brief Test with empty key
 */
TEST_F(BPTreeTest, TestEmptyKey)
{
    BPTree tree(&page_buffer_man);

    std::string empty_key = "";
    record_id_t record_id = {6000, 0};

    tree.Insert(to_span(empty_key), record_id);

    auto result = tree.Search(to_span(empty_key));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, record_id);
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
        std::snprintf(key_buf, sizeof(key_buf), "stress_key_%06d", i);

        std::string key(key_buf);
        record_id_t record_id = {10000 + static_cast<uint32_t>(i), 0};

        tree.Insert(to_span(key), record_id);
    }

    // Sample verify (check every 10th entry to keep test fast)
    for (int i = 0; i < num_entries; i += 10) {
        char key_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "stress_key_%06d", i);

        std::string key(key_buf);
        record_id_t expected_record_id = {10000 + static_cast<uint32_t>(i), 0};

        auto result = tree.Search(to_span(key));
        ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
        EXPECT_EQ(*result, expected_record_id);
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
            record_id_t record_id = {20000 + static_cast<uint32_t>(round * 100 + i), 0};
            tree.Insert(to_span(key), record_id);
        }

        // Delete 10 entries
        for (int i = 0; i < 10; i++) {
            std::string key = "key_" + std::to_string(round) + "_" + std::to_string(i);
            tree.Delete(to_span(key));
        }

        // Verify remaining 10 entries exist
        for (int i = 10; i < 20; i++) {
            std::string key = "key_" + std::to_string(round) + "_" + std::to_string(i);
            record_id_t expected_record_id = {20000 + static_cast<uint32_t>(round * 100 + i), 0};

            auto result = tree.Search(to_span(key));
            ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
            EXPECT_EQ(*result, expected_record_id);
        }
    }
}
