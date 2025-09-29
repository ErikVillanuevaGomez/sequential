CXXFLAGS = -std=c++11 -pthread -Wall -O2 -I./rapidjson/include
LDFLAGS = -lcurl -pthread

CC = g++
LD = g++

all: level_client parallel_level_client

level_client: level_client.o
	$(LD) $^ -o $@ -lcurl
	
parallel_level_client: parallel_level_client.o
	$(LD) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
		$(CC) -c $(CXXFLAGS) $< -o $@

clean: rm -f level_client parallel_level_client
