#pragma once
#include <iostream>
#include <memory>
#include <algorithm>
#include <mutex>
#include <functional>
#include <stdexcept>


namespace TSMap
{
/**
 * since stl containers are disabled
 * implement our own pair
 */
template <typename FirstT, typename SecondT>
struct pair
{
    FirstT first;
    SecondT second;

    pair() {}

    pair(const FirstT &first, const SecondT &second)
        : first(first),
          second(second)
    {}

    /**
     * copy constructor
     */
    pair(const pair<FirstT, SecondT> & rhs)
    {
        this->first = rhs.first;
        this->second = rhs.second;
    }

    pair<FirstT, SecondT> & operator=(const pair<FirstT, SecondT>& rhs)
    {
        this->first = rhs.first;
        this->second = rhs.second;
        return *this;
    }
};

/**
 * similar to std::make_pair
 */
template <typename FirstT, typename SecondT>
const pair<FirstT, SecondT> make_pair(
        const FirstT &first,
        const SecondT &second)
{
    return pair<FirstT, SecondT>(first, second);
}

namespace utility
{

/**
 * a "bucket" in map for hash function level collision
 * needs to be thread-safe at this level.
 *
 * underlying datastructure: two dynamically allocated arrays, one for key-value
 * pair and the other for key-value pair validity (for quick deletion)
 *
 * the invalid elements are removed at resizing
 */
template <typename KeyT, typename ValueT>
class KVPairList
{
    //bucket-specific lock
    std::mutex mutex;
    //key-value pair array
    std::shared_ptr<pair<KeyT, ValueT> > list;
    //mark stored value as valid/invalid. for fast erase.
    std::shared_ptr<bool> validity;
    //allocated bucket size
    size_t capacity;
    //pointer at last element written to array
    size_t lastElementPtr;
    //number of ``actual'' or valid entries
    size_t validSize;
    //defaults capacity to 32
public:
    KVPairList(size_t capacity) :
        list(new pair<KeyT, ValueT>[capacity], std::default_delete<pair<KeyT, ValueT>[]>()),
        validity(new bool[capacity], std::default_delete<bool[]>()),
        capacity(capacity),
        lastElementPtr(0),
        validSize(0)
    {}

    /**
     * default constructor: bucket size = 32
     */
    KVPairList() :
        KVPairList(32)
    {}

    /**
     * operator= for assignment from right hand side (rhs)
     *
     */
    KVPairList & operator=(const KVPairList<KeyT, ValueT> rhs)
    {
        //need to guard both lists for mutex access
        std::lock_guard<std::mutex> rlock(rhs.mutex);
        std::lock_guard<std::mutex> lock(mutex);

        list = rhs.list;
        validity = rhs.validity;
        capacity = rhs.capacity;
        lastElementPtr = rhs.lastElementPtr;
        validSize = rhs.validSize;

        return *this;
    }

    /**
     * size()
     * returns ``valid size'', or number of elements in list that's valid
     */
    size_t size()
    {
        //acquire lock:
        std::lock_guard<std::mutex> lock(mutex);
        auto retVal = validSize;
        return retVal;
    }

    /**
     * insert key-value pair if key didnt exist in list
     * otherwise update preexisting key's value
     *
     * param: const pair of key-value
     */
    void upsert(const pair<KeyT, ValueT> &kv)
    {
        //acquire lock:
        std::lock_guard<std::mutex> lock(mutex);

        //if array full, increase array size by 50%
        if(lastElementPtr >= capacity)
        {
            resize((size_t)(capacity*1.5));
        }
        //if key exists in list, update
        auto i = indexOf(kv.first);
        if(i != -1)
        {
            list.get()[i].second = kv.second;
            validity.get()[i] = true;
        }
        else
        {
            //insert new
            this->list.get()[lastElementPtr] = kv;
            this->validity.get()[lastElementPtr] = true;
            lastElementPtr++;
            validSize++;
        }
    }

    /**
     *
     * outputs a string that represents the array of key-value pairs
     *
     * thread safety is not guaranteed
     *
     * for debugging purposes mostly.
     *
     */
    friend std::ostream& operator<<(std::ostream &stream, const KVPairList& rhs)
    {
        //stream<<"{";
        for(auto i=0;i<rhs.lastElementPtr;++i){
            if(rhs.validity.get()[i])
            stream<<rhs.list.get()[i].first<<":"<<rhs.list.get()[i].second<<", ";
        }
        //stream<<"}";
        return stream;
    }

    /**
     * removes an element by key
     *
     * only marking the element to be removed as invalid for fast operation
     *
     * if element not found in list, it is considered to be ``removed''
     * param: key of object to be removed
     */
    void erase(const KeyT &key)
    {
        //erase object with given key by marking validity as invalid
        std::lock_guard<std::mutex> lock(mutex);
        //linear search
        auto i = indexOf(key);
        if(i != -1)
        {
            validity.get()[i] = false;

            --validSize;
        }
        //if element not found in list, it is considered to be ``removed''
    }

    /**
     * return 1 if key exists in list
     * 0 otherwise
     *
     * param: key of element
     */
    size_t count(const KeyT &key)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto count = indexOf(key) == -1 ? 0 : 1;
        return count;
    }


    /**
     * get object with given key
     *
     * param: key of element
     */
    ValueT & operator[](const KeyT & key)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto i = indexOf(key);
        if(i != -1)
        {
            return list.get()[i].second;
        }
        //else:
        //throw error:
        //this is why one should check for validity first using .count()
        throw new std::invalid_argument("invalid key given in KVList");
    }

    //allows access from TSMap class
    friend class TSMap;

private:
    /**
     * return index of key-value pair if key exists in list
     * -1 otherwise
     *
     * not thread safe, should always be called by own class methods
     *
     * param: key of element
     */

    size_t indexOf(const KeyT &key)
    {
        auto index = -1;
        for(auto i=0;i<lastElementPtr;++i)
        {
            if(list.get()[i].first == key && validity.get()[i])
            {
                index = i;
                break;
            }
        }
        return index;
    }

    /**
     * resizes both validity list and key-value pair list to new size according
     * to parameter, copies element over.
     *
     * param: new capacity to be resized
     */
    void resize(size_t newCapacity)
    {
        //do nothing
        if(newCapacity == capacity) return;

        if(newCapacity >= lastElementPtr)
        {
            std::shared_ptr<pair<KeyT, ValueT> >
                newList(new pair<KeyT, ValueT>[(size_t)(newCapacity)],
                        std::default_delete<pair<KeyT, ValueT>[]>());
            std::shared_ptr<bool>
                newValidity(new bool[(size_t)newCapacity](),
                            std::default_delete<bool[]>());
            //copy to newlist only the valid entries:
            auto newListPtr = 0;
            for(auto i=0;i<this->lastElementPtr;++i){
                if (validity.get()[i]){
                    newList.get()[newListPtr] = list.get()[i];
                    newValidity.get()[newListPtr] = true;
                    newListPtr++;
                }
            }
            list = newList;
            validity = newValidity;
            lastElementPtr = newListPtr;
            this->capacity = newCapacity;
        }
        else
        {
            //throw error
            throw new std::invalid_argument(
                    "new capacity in KVPairList::resize is smaller than current length");
        }
    }
};

}//end utility namespace

}//end tsmap ns
