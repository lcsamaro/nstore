CC=gcc
CXX=g++

INC=-I../lib/liblmdb -I../lib/spdlog/include -I../lib/rapidjson/include -isystem../lib/asio/include
LNK=-lsodium
CXXFLAGS+=$(INC) -pthread -std=c++11 -Wall -pedantic
CPPFLAGS=-DASIO_STANDALONE

all: release
release: CXXFLAGS += -O3 -funroll-loops
release: CCFLAGS += -O3 -funroll-loops
release: server tools
debug: CXXFLAGS += -O1 -DDEBUG -g
debug: CCFLAGS += -O1 -DDEBUG -g
debug: server tools

test: CXXFLAGS += -O1 -DDEBUG -g -fprofile-arcs -ftest-coverage
test: CCFLAGS += -O1 -DDEBUG -g -fprofile-arcs -ftest-coverage
test: CXXFLAGS += -I../lib/catch -I../lib/json
test: CPPFLAGS += -DNSTORE_UNIT_TESTING
test: test.cpp mocks.o handler.o db.o stats.o mdb.o midl.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o test test.cpp mocks.o handler.o db.o stats.o mdb.o midl.o $(LNK)

# ( lcov --directory . --extract src --rc lcov_branch_coverage=1 -c --output-file coverage.info ) ; \

cov: test
	( gcov handler.cpp -b -s . -r -b ) ; \
		( lcov --directory . --extract . --output-file coverage.info ) ; \
		( genhtml coverage.info --output-directory out ) ; \
		( google-chrome ./out/index.html & )

tools: indices mdb_tools
clean:
	rm -f *.o server indices

server: server.cpp server.h handler.o db.o stats.o session.o channel.o mdb.o midl.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o server server.cpp handler.o db.o stats.o session.o channel.o mdb.o midl.o $(LNK)

mdb.o: ../lib/liblmdb/mdb.c ../lib/liblmdb/lmdb.h ../lib/liblmdb/midl.h
	$(CC) $(CCFLAGS) -c ../lib/liblmdb/mdb.c

midl.o: ../lib/liblmdb/midl.c ../lib/liblmdb/lmdb.h ../lib/liblmdb/midl.h
	$(CC) $(CCFLAGS) -c ../lib/liblmdb/midl.c

indices: indices.cpp db.o mdb.o midl.o
	$(CXX) $(CXXFLAGS) -O3 -funroll-loops -o indices indices.cpp db.o mdb.o midl.o $(INCLUDE)

mdb_tools:
	( $(MAKE) -C ../lib/liblmdb ) && \
		( cp ../lib/liblmdb/mdb_copy . && \
			cp ../lib/liblmdb/mdb_dump . && \
			cp ../lib/liblmdb/mdb_load . && \
			cp ../lib/liblmdb/mdb_stat . )

