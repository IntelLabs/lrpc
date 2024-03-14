# See README.txt.

.PHONY: all cpp clean

all: cpp

cpp:    pingpong

# rm -f simpletest simpletest.pb.cc simpletest.pb.h protoc_middleman
# not removing *.pb.* - because we implement functionality needed for gereating transfomration maps. 
clean:
	rm -f pingpong pingpong.pb.cc pingpong.pb.h protoc_middleman_2

protoc_middleman_2: pingpong.proto
	protoc $$PROTO_PATH --cpp_out=.  pingpong.proto
	@touch protoc_middleman_2

pingpong: pingpong.cc protoc_middleman_2
	pkg-config --cflags protobuf  # fails if protobuf is not installed
	g++ -std=c++11 -mbmi -mbmi2 pingpong.cc pingpong.pb.cc -o pingpong `pkg-config --cflags --libs protobuf` -O2 -g

