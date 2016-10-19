#pragma once
#include <iostream>
#include <memory>
#include <mutex>
#include <functional>
#include <stdexcept>
#include <condition_variable>
#include <KVPairList.hpp>

namespace TSMap
{

/**
 * a thread safe hashmap
 *
 * takes a key and its corresponding value
 *
 * this map uses std::hash for hashing, but one can use own hash function if
 * desired.
 *
 * NOTE: thread safety is guaranteed at the bucket level. thus no thread guard
 * on the map level
 *
 * resizing is automatic as insertion is delegated to underlying KVPairList,
 * which resizes automatically.
 *
 */
template <typename KeyT, typename ValueT>
class TSMap
{
private:
    //array of buckets (key-value pairs) for same hash
    std::shared_ptr<utility::KVPairList<KeyT, ValueT> > buckets;
    //number of buckets:
    size_t tableSize;
    std::hash<KeyT> hashFunc;

public:
    /**
     * default constructor: set bucket size to 128
     */
    TSMap() : TSMap(128)
    {}

    TSMap(size_t tableSize) :
        buckets(new utility::KVPairList<KeyT, ValueT>[tableSize](),
                std::default_delete<utility::KVPairList<KeyT, ValueT>[] >()),
        tableSize(tableSize)
    {}

    ~TSMap()
    {}

    /**
     * delete element by key
     * delegate to corresponding bucket for deletion.
     *
     * param: key of element to be deleted
     */
    void deleteByKey(const KeyT& key)
    {
        auto hashKey = hashFunc(key) % tableSize;
        buckets.get()[hashKey].erase(key);
    }

    /**
     * count function similar to the std::unordered_map::count
     *
     * returns 1 if key exists in map, otherwise 0
     *
     * delegated to bucket object.
     *
     * param: key of element
     */
    size_t count(const KeyT& key)
    {
        return buckets.get()[hashFunc(key)%tableSize].count(key);
    }

    /**
     * insert an entry by key to map
     *
     * key and value pairs should have a default/overloaded copying operator=
     * and/or a copying constructor
     *
     * params: key and value
     */
    void insert(const KeyT &key, const ValueT &value)
    {
        auto hashKey = hashFunc(key) % tableSize;
        //insert to corresponding bucket
        buckets.get()[hashKey].upsert(make_pair(key, value));
    }

    /**
     * lookup(access) an element in map
     *
     * param: key of element to be looked up
     * returns element if exists, otherwise throw an exception.
     */
    ValueT& lookup(const KeyT &key)
    {
        auto hashKey = hashFunc(key) % tableSize;
        return buckets.get()[hashKey][key];
    }

    /**
     * retrieves an element in map
     *
     * param: key of element to be looked up
     *
     * returns element if exists, otherwise throw an exception.
     */
    ValueT& operator[](const KeyT &key)
    {
        return lookup(key);
    }

    /**
     * thread safety not guaranteed
     * for debugging purpose
     *
     * outputs an ostream of string representation of map
     */
    friend std::ostream& operator<<(std::ostream &stream, const TSMap& rhs)
    {
        stream<<"{";
        for(auto i=0;i<rhs.tableSize;++i){
            stream<<rhs.buckets.get()[i];
        }
        stream<<"}";
        return stream;
    }
};
}//end namespace TSMap
