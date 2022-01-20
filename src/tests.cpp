#include <gtest/gtest.h>

#include <stdio.h>

#include <invertible_bloom_filter.hpp>

using namespace ibf;

struct Murmur3Finalizer {
  template <class T> constexpr T operator()(T key) const {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdLLU;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53LLU;
    key ^= key >> 33;
    return key;
  }
};

TEST(InvertibleBloomDictionary, TestConstruct) {
  using Key = std::uint64_t;
  using Value = std::uint32_t;
  using HashFn = Murmur3Finalizer;

  InvertibleBloomDictionary<Key, Value, HashFn> ibf(0);
  EXPECT_EQ(ibf.size(), 0);
  EXPECT_EQ(ibf.directory_size(), 0);
  auto seeds = ibf.listSeeds();
  for (size_t i = 0; i < seeds.size(); i++)
    for (size_t j = i + 1; j < seeds.size(); j++)
      EXPECT_FALSE(seeds[i] == seeds[j]);

  InvertibleBloomDictionary<Key, Value, HashFn> ibf2(10);
  EXPECT_EQ(ibf2.size(), 0);
  EXPECT_EQ(ibf2.directory_size(), 10);
  seeds = ibf2.listSeeds();
  for (size_t i = 0; i < seeds.size(); i++)
    for (size_t j = i + 1; j < seeds.size(); j++)
      EXPECT_FALSE(seeds[i] == seeds[j]);
}

TEST(InvertibleBloomDictionary, TestInsertAndRetrieve) {
  using Key = std::uint64_t;
  using Value = std::uint32_t;
  using HashFn = Murmur3Finalizer;

  InvertibleBloomDictionary<Key, Value, HashFn> ibf(10, 0);

  EXPECT_TRUE(ibf.contains(1337) == ContainsResult::not_found);
  ibf.insert(1337, 42);
  EXPECT_TRUE(ibf.contains(1337) == ContainsResult::exists);
  EXPECT_EQ(ibf.get(1337), 42);
  EXPECT_EQ(ibf.size(), 1);

  EXPECT_TRUE(ibf.contains(84) == ContainsResult::not_found);
  ibf.insert(84, 85);
  EXPECT_TRUE(ibf.contains(84) == ContainsResult::exists);
  EXPECT_EQ(ibf.get(84), 85);
  EXPECT_EQ(ibf.size(), 2);
}

TEST(InvertibleBloomDictionary, TestRemove) {
  using Key = std::uint64_t;
  using Value = std::uint32_t;
  using HashFn = Murmur3Finalizer;

  InvertibleBloomDictionary<Key, Value, HashFn> ibf(10, 0);

  ibf.insert(1337, 42);
  ibf.insert(84, 85);
  EXPECT_TRUE(ibf.contains(1337) == ContainsResult::exists);
  EXPECT_TRUE(ibf.contains(84) == ContainsResult::exists);
  EXPECT_EQ(ibf.get(1337), 42);
  EXPECT_EQ(ibf.get(84), 85);

  EXPECT_TRUE(ibf.remove(1337));
  EXPECT_TRUE(ibf.contains(1337) == ContainsResult::not_found);
  EXPECT_EQ(ibf.size(), 1);
  EXPECT_TRUE(ibf.remove(84));
  EXPECT_TRUE(ibf.contains(84) == ContainsResult::not_found);
  EXPECT_EQ(ibf.size(), 0);
}

TEST(InvertibleBloomDictionary, TestListAll) {
  using Key = std::uint64_t;
  using Value = std::uint32_t;
  using HashFn = Murmur3Finalizer;

  InvertibleBloomDictionary<Key, Value, HashFn> ibf(10, 0);

  const std::vector<std::pair<Key, Value>> data{{1, 0}, {1337, 42}, {86, 89}};

  for (const auto &d : data)
    ibf.insert(d.first, d.second);

  EXPECT_EQ(ibf.size(), data.size());

  auto l = ibf.listAll();
  EXPECT_TRUE(l);
  if (l) {
    auto listed = *l;
    EXPECT_EQ(listed.size(), ibf.size());
    for (const auto &d : data) {
      bool found = false;
      for (size_t i = 0; i < listed.size(); i++) {
        if (listed[i].first == d.first && listed[i].second == d.second) {
          found = true;
          listed.erase(std::next(listed.begin(), i));
          break;
        }
      }
      EXPECT_TRUE(found);
    }
  }
}
