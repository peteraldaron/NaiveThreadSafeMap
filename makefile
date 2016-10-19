CXX=c++ -O3
CXXFLAGS=-I. -std=c++14 -lboost_system -pthread

BINS=tsmap recursion

all: $(BINS)

recursion: recursion.cpp
	$(CXX) -std=c++14 -o $@ recursion.cpp

tsmap: TSMap.cpp TSMap.hpp KVPairList.hpp
	$(CXX) $(CXXFLAGS) -o $@ TSMap.cpp

clean:
	rm -rf *.o *.a $(BINS)

