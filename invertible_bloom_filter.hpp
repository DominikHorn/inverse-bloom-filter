#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <unordered_set>
#include <vector>

namespace ibf {

/**
 * InvertibleBloomDictionarys are probabilistic structures
 * that can, in some cases, only answer probabilistically
 */
enum ContainsResult { not_found, might_exist, exists };

/**
 * InvertibleBloomFilter is a probabilistic set data structure.
 *
 * It can do everything a normal bloom filter is capable of probabilistically
 * recovering the original keyset
 */
template <class Key, class HashFn, size_t K = 3,
          class BucketCounter = std::uint16_t>
class InvertibleBloomFilter {
  struct Bucket {
    Key cumulative_key = 0;
    BucketCounter count = 0;
  } /*__attribute((packed))*/;

  const HashFn hasher;

  using Seed = std::uint64_t;
  std::array<Seed, K> seeds;

  std::vector<Bucket> buckets;
  size_t count;

  size_t hash_index(const Key &key, const Seed &seed) const {
    const auto hash = (hasher(key) ^ seed);
    return hash % buckets.size(); // TODO: use fast modulo
  }

public:
  /**
   * Constructs and InvertibleBloomFilter given a target directory size and
   * seed (defaults to std::random_device()()). Note that the directory never
   * resizes during InvertibleBloomFilter's lifetime, hence you must pick a
   * value that fits your keys upfront
   */
  InvertibleBloomFilter(size_t size, unsigned int seed = std::random_device()())
      : buckets(size), count(0) {
    std::default_random_engine rng(seed);
    std::uniform_int_distribution<Seed> dist(std::numeric_limits<Seed>::min(),
                                             std::numeric_limits<Seed>::max());

    for (size_t i = 0; i < K; i++) {
      // generate until we find a new random seed
      Seed rand_seed = 0;
      bool already_exists = false;
      do {
        rand_seed = dist(rng);
        for (size_t j = 0; j < i; j++)
          already_exists |= seeds[j] == rand_seed;
      } while (already_exists);

      seeds[i] = rand_seed;
    }
  }

  /**
   * Count of keys in this InvertibleBloomFilter
   */
  size_t size() const { return count; }

  /**
   * Size of internal bucket directory
   */
  size_t directory_size() const { return buckets.size(); }

  /**
   * Exposes internally used seeds, useful for testing or external serialization
   */
  std::array<Seed, K> listSeeds() const { return seeds; }

  /**
   * Inserts a single key with its corresponding value
   */
  void insert(const Key &key) {
    // keep track of indices we already set to avoid issues with two hashes
    // going to same bucket
    std::unordered_set<size_t> seen_indices;

    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      // fix issue with hashfn hashing to same slot
      if (seen_indices.find(index) != seen_indices.end())
        continue;
      seen_indices.insert(index);

      auto &bucket = buckets[index];
      bucket.cumulative_key ^= key;
      bucket.count++;
    }

    count += 1;
  }

  /**
   * Checks whether a key is contained in this IBF. May return false positives,
   * but never false negatives
   */
  ContainsResult contains(const Key &key) const {
    bool might_exist = false;
    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      const auto &bucket = buckets[index];
      if (bucket.count == 1)
        return key == bucket.cumulative_key ? ContainsResult::exists
                                            : ContainsResult::not_found;

      might_exist |= bucket.count > 1;
    }

    return might_exist ? ContainsResult::might_exist
                       : ContainsResult::not_found;
  }

  /**
   * Removes a single key and its corresponding value from
   * the struct. Note that it is possible for this operation
   * to fail (return false) not because the key doesn't exist,
   * but because it is not uniquely identifyable
   */
  bool remove(Key key) {
    if (contains(key) != ContainsResult::exists)
      return false;

    // keep track of indices we already set to avoid issues with two hashes
    // going to same bucket
    std::unordered_set<size_t> seen_indices;

    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      // fix issue with hashfn hashing to same slot
      if (seen_indices.find(index) != seen_indices.end())
        continue;
      seen_indices.insert(index);

      auto &bucket = buckets[index];
      assert(bucket.count > 0);

      bucket.cumulative_key ^= key;
      bucket.count--;
    }

    count -= 1;
    return true;
  }

  /**
   * Attempts to retrieve all key, value pairs. This operation might fail
   * due to the probabilistic nature of this struct
   */
  std::optional<std::unordered_set<Key>> listAll() const {
    std::unordered_set<Key> res;
    res.reserve(count);

    auto copy = *this;

    // TODO: come up with faster algorithm
    bool finished = false;
    bool has_changed = true;
    while (!finished && has_changed) {
      finished = true;
      has_changed = false;
      for (size_t i = 0; i < copy.buckets.size(); i++) {
        const auto &bucket = copy.buckets[i];

        // skip already empty buckets
        if (bucket.count == 0)
          continue;

        // skip ambiguous buckets
        if (bucket.count > 1) {
          finished = false;
          continue;
        }

        res.insert(bucket.cumulative_key);
        has_changed = copy.remove(bucket.cumulative_key);

        assert(bucket.count == 0);
      }
    }

    if (!finished || res.size() != count)
      return std::nullopt;

    return std::make_optional(res);
  }
};

/**
 * InvertibleBloomDictionary is a probabilistic dictionary data structure.
 *
 * It can do everything a normal bloom filter is capable of and, with a certain
 * probability < 1, recover associated values and also the original keyset
 */
template <class Key, class Value, class HashFn, size_t K = 3,
          class BucketCounter = std::uint16_t>
class InvertibleBloomDictionary {
  struct Bucket {
    Key cumulative_key = 0;
    Value cumulative_value = 0;
    BucketCounter count = 0;
  } /*__attribute((packed))*/;

  const HashFn hasher;

  using Seed = std::uint64_t;
  std::array<Seed, K> seeds;

  std::vector<Bucket> buckets;
  size_t count;

  size_t hash_index(const Key &key, const Seed &seed) const {
    const auto hash = (hasher(key) ^ seed);
    return hash % buckets.size(); // TODO: use fast modulo
  }

public:
  /**
   * Constructs and InvertibleBloomDictionary given a target directory size and
   * seed (defaults to std::random_device()()). Note that the directory never
   * resizes during InvertibleBloomDictionary's lifetime, hence you must pick a
   * value that fits your keys upfront
   */
  InvertibleBloomDictionary(size_t size,
                            unsigned int seed = std::random_device()())
      : buckets(size), count(0) {
    std::default_random_engine rng(seed);
    std::uniform_int_distribution<Seed> dist(std::numeric_limits<Seed>::min(),
                                             std::numeric_limits<Seed>::max());

    for (size_t i = 0; i < K; i++) {
      // generate until we find a new random seed
      Seed rand_seed = 0;
      bool already_exists = false;
      do {
        rand_seed = dist(rng);
        for (size_t j = 0; j < i; j++)
          already_exists |= seeds[j] == rand_seed;
      } while (already_exists);

      seeds[i] = rand_seed;
    }
  }

  /**
   * Count of keys in this InvertibleBloomDictionary
   */
  size_t size() const { return count; }

  /**
   * Size of internal bucket directory
   */
  size_t directory_size() const { return buckets.size(); }

  /**
   * Exposes internally used seeds, useful for testing or external serialization
   */
  std::array<Seed, K> listSeeds() const { return seeds; }

  /**
   * Inserts a single key with its corresponding value
   */
  void insert(const Key &key, const Value &value) {
    // keep track of indices we already set to avoid issues with two hashes
    // going to same bucket
    std::unordered_set<size_t> seen_indices;

    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      // fix issue with hashfn hashing to same slot
      if (seen_indices.find(index) != seen_indices.end())
        continue;
      seen_indices.insert(index);

      auto &bucket = buckets[index];
      bucket.cumulative_key ^= key;
      bucket.cumulative_value ^= value;
      bucket.count++;
    }

    count += 1;
  }

  /**
   * Checks whether a key is contained in this IBF. May return false positives,
   * but never false negatives
   */
  ContainsResult contains(const Key &key) const {
    bool might_exist = false;
    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      const auto &bucket = buckets[index];
      if (bucket.count == 1)
        return key == bucket.cumulative_key ? ContainsResult::exists
                                            : ContainsResult::not_found;

      might_exist |= bucket.count > 1;
    }

    return might_exist ? ContainsResult::might_exist
                       : ContainsResult::not_found;
  }

  /**
   * Returns the value associated with key if retrievable.
   * Can return nullopt, eiher if key does not exist or if it's value is not
   * uniquely identifyable.
   */
  std::optional<Value> get(const Key &key) const {
    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      const auto &bucket = buckets[index];
      if (bucket.count == 1)
        return key == bucket.cumulative_key
                   ? std::make_optional(bucket.cumulative_value)
                   : std::nullopt;
    }

    return std::nullopt;
  }

  /**
   * Removes a single key and its corresponding value from
   * the struct. Note that it is possible for this operation
   * to fail (return false) not because the key doesn't exist,
   * but because it is not uniquely identifyable
   */
  bool remove(Key key) {
    const auto value = get(key);
    if (!value)
      return false;
    assert(value.has_value());

    // keep track of indices we already set to avoid issues with two hashes
    // going to same bucket
    std::unordered_set<size_t> seen_indices;

    for (const auto &seed : seeds) {
      const auto index = hash_index(key, seed);

      // fix issue with hashfn hashing to same slot
      if (seen_indices.find(index) != seen_indices.end())
        continue;
      seen_indices.insert(index);

      auto &bucket = buckets[index];
      assert(bucket.count > 0);

      bucket.cumulative_key ^= key;
      bucket.cumulative_value ^= *value;
      bucket.count--;
    }

    count -= 1;
    return true;
  }

  /**
   * Attempts to retrieve all key, value pairs. This operation might fail
   * due to the probabilistic nature of this struct
   */
  std::optional<std::vector<std::pair<Key, Value>>> listAll() const {
    std::vector<std::pair<Key, Value>> res;
    res.reserve(count);

    auto copy = *this;

    // TODO: come up with faster algorithm
    bool finished = false;
    bool has_changed = true;
    while (!finished && has_changed) {
      finished = true;
      has_changed = false;
      for (size_t i = 0; i < copy.buckets.size(); i++) {
        const auto &bucket = copy.buckets[i];

        // skip already empty buckets
        if (bucket.count == 0)
          continue;

        // skip ambiguous buckets
        if (bucket.count > 1) {
          finished = false;
          continue;
        }

        res.push_back({bucket.cumulative_key, bucket.cumulative_value});
        has_changed = copy.remove(bucket.cumulative_key);

        assert(bucket.count == 0);
      }
    }

    if (!finished || res.size() != count)
      return std::nullopt;

    return std::make_optional(res);
  }
};
} // namespace ibf
