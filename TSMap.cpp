#include <iostream>
#include <string>
#include <thread>
#include <TSMap.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>

#define BOOST_TEST_MODULE threadsafe_map_test

BOOST_AUTO_TEST_SUITE(threadsafe_map_test)

BOOST_AUTO_TEST_CASE(test_single_thread_kvlist_no_resize)
{
    using namespace TSMap;
    using string = std::string;
    utility::KVPairList<string, int> pl;
    for(auto i='A';i<='Z';++i)
    {
        pl.upsert(::TSMap::make_pair(std::to_string(i), (int)i));
    }
    BOOST_TEST(pl.size() == 'Z'-'A'+1);
}

BOOST_AUTO_TEST_CASE(test_single_thread_kvlist_with_resize)
{
    using namespace TSMap;
    using string = std::string;
    utility::KVPairList<string, int> pl;
    for(auto i='A';i<='z';++i)
    {
        pl.upsert(::TSMap::make_pair(std::to_string(i), (int)i));
    }

    BOOST_TEST(pl.size() == 'z'-'A'+1);
}

BOOST_AUTO_TEST_CASE(test_multiple_threads_kvlist_with_resize)
{
    using namespace TSMap;
    using string = std::string;
    utility::KVPairList<string, int> pl;

    //start 3 threads
    std::thread tr[3];
    for(int j=0;j<3;++j)
    {
        tr[j] = std::thread([&](const int val)
        {
            auto id = std::this_thread::get_id();
            for(auto i='A';i<='z';++i)
            {
                pl.upsert(::TSMap::make_pair(std::to_string((char)i)+std::to_string(val), (int)i));
            }
        }, j);
    }
    for(int j=0;j<3;++j)
    {
        tr[j].join();
    }

    BOOST_TEST(pl.size() == ('z'-'A'+1)*3);
}

BOOST_AUTO_TEST_CASE(test_multiple_threads_kvlist_erase)
{
    using namespace TSMap;
    using string = std::string;
    utility::KVPairList<string, int> pl;

    std::thread tr[3];
    std::thread tr2[3];
    for(int j=0;j<3;++j)
    {
        tr[j] = std::thread([&](const int val)
        {
            auto id = std::this_thread::get_id();
            for(auto i='A';i<='z';++i)
            {
                pl.upsert(::TSMap::make_pair(std::to_string((char)i)+std::to_string(val), (int)i));
            }
        }, j);
    }

    for(int j=0;j<3;++j)
    {
        tr[j].join();
    }

    BOOST_TEST(pl.size() == ('z'-'A'+1)*3);

    for(int j=0;j<3;++j)
    {
        tr2[j] = std::thread([&](const int val)
        {
            for(auto i='A';i<='z';++i)
            {
                pl.erase(std::to_string((char)i)+std::to_string(val));
            }
        }, j);

    }
    for(int j=0;j<3;++j)
    {
        tr2[j].join();
    }

    BOOST_TEST(pl.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_multiple_threads_kvlist_erase_with_resize_validity)
{
    using namespace TSMap;
    using string = std::string;
    utility::KVPairList<string, int> pl(2);

    pl.upsert(::TSMap::make_pair(string("one"), 1));
    pl.upsert(::TSMap::make_pair(string("two"), 2));
    pl.upsert(::TSMap::make_pair(string("three"), 3));
    pl.upsert(::TSMap::make_pair(string("four"), 4));
    pl.upsert(::TSMap::make_pair(string("five"), 5));

    BOOST_TEST(pl.size()==5);

    std::thread t1([&](){pl.erase("one");});
    std::thread t2([&](){pl.erase("three");});
    std::thread t3([&](){pl.erase("five");});

    t1.join();
    t2.join();
    t3.join();

    BOOST_TEST(pl.size()==2);

    BOOST_TEST(pl["two"] == 2);
    BOOST_TEST(pl["four"] == 4);
}

BOOST_AUTO_TEST_CASE(TSMap_insert_test_single_thread)
{
    using string = std::string;
    TSMap::TSMap<string, int> map;
    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);
    map.insert("four", 4);
    map.insert("five", 5);
    BOOST_TEST(map["two"] == 2);
    BOOST_TEST(map["four"] == 4);
    BOOST_TEST(map["five"] == 5);
    map.insert("five", 7);
    BOOST_TEST(map["five"] == 7);
}

BOOST_AUTO_TEST_CASE(TSMap_erase_test_single_thread)
{
    using string = std::string;
    TSMap::TSMap<string, int> map;
    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);
    map.insert("four", 4);
    map.insert("five", 5);
    BOOST_TEST(map.count("five") == 1);
    BOOST_TEST(map.count("two") == 1);
    BOOST_TEST(map["five"] == 5);

    map.deleteByKey("five");
    map.deleteByKey("two");

    BOOST_TEST(map.count("five") == 0);
    BOOST_TEST(map.count("two") == 0);
}

BOOST_AUTO_TEST_CASE(TSMap_multithread_test_1)
{
    using string = std::string;
    TSMap::TSMap<int, int> map;

    std::thread tpool[32];
    for(auto i=0;i<32;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            int threadValue = tid;
            for(int j=threadValue*2; j<threadValue*3;++j){
                map.insert(j, tid);
            }
        }, i);
    }

    std::for_each(tpool, tpool+32, [&](std::thread &t)
    {
        t.join();
    });

    //test edge value to be sure. others may be overwritten
    BOOST_TEST(map[31*3-1]==31);
}

//multithread write with two conflicts
BOOST_AUTO_TEST_CASE(TSMap_multithread_test_2)
{
    using string = std::string;
    TSMap::TSMap<int, int> map;

    //on default bucket = 128, will have two hash conflicts
    std::thread tpool[256];
    for(auto i=0;i<256;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.insert(tid, tid);
        }, i);
    }

    std::for_each(tpool, tpool+256, [&](std::thread &t)
    {
        t.join();
    });

    //verify:
    for(auto i=0;i<256;++i)
    {
        BOOST_TEST(map[i]==i);
    }
}

//multithread write with duplicates: update
BOOST_AUTO_TEST_CASE(TSMap_multithread_test_with_duplicates)
{
    using string = std::string;
    TSMap::TSMap<int, int> map;

    std::thread tpool[256];
    for(auto i=0;i<256;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.insert(tid/2, tid);
            map.insert(tid, tid);
        }, i);
    }

    std::for_each(tpool, tpool+256, [&](std::thread &t)
    {
        t.join();
    });

    //verify:
    for(auto i=0;i<128;++i)
    {
        bool value = map[i]==i || map[i]==i*2 || map[i]==i*2+1;
        BOOST_TEST(value);
    }
}

BOOST_AUTO_TEST_CASE(TSMap_multithread_erase)
{
    using string = std::string;
    TSMap::TSMap<int, int> map;

    std::thread tpool[128];
    for(auto i=0;i<128;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.insert(tid, tid);
        }, i);
    }

    std::for_each(tpool, tpool+128, [&](std::thread &t)
    {
        t.join();
    });

    for(auto i=0;i<32;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.deleteByKey(tid);
        }, i);
    }
    std::for_each(tpool, tpool+32, [&](std::thread &t)
    {
        t.join();
    });

    //verify:
    for(auto i=32;i<128;++i)
    {
        bool value = map[i]==i;
        BOOST_TEST(value);
    }

    for(auto i=0;i<32;++i)
    {
        BOOST_TEST(map.count(i) == 0);
    }
}

BOOST_AUTO_TEST_CASE(TSMap_multithread_upsert)
{
    using string = std::string;
    TSMap::TSMap<int, int> map;

    std::thread tpool[128];
    for(auto i=0;i<128;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.insert(tid, tid);
        }, i);
    }

    std::for_each(tpool, tpool+128, [&](std::thread &t)
    {
        t.join();
    });

    //increment by one:
    for(auto i=0;i<128;++i)
    {
        tpool[i] = std::thread([&](const int tid){
            map.insert(tid, map[tid]+1);
        }, i);
    }

    std::for_each(tpool, tpool+128, [&](std::thread &t)
    {
        t.join();
    });

    for(auto i=0;i<128;++i)
    {
        BOOST_TEST(map[i] == i+1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
