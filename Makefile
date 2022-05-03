LOCAL := ${HOME}/local
IGNORE := -Wc++11-narrowing
OPTS := -std=c++20 ${IGNORE} -I${LOCAL}/include
DBG := -g
REL := -O3
LIBS := -L${LOCAL}/lib -Wl,-rpath=${LOCAL}/lib -lstdc++fs -static -lboost_json -lboost_chrono -lboost_program_options -lboost_filesystem
SRCS := $(wildcard src/*.cpp)
T_BIN := build/test
T_REL := build/release
T_OBJS := $(addprefix ${T_BIN}/,$(patsubst %.cpp,%.o,$(SRCS)))
R_OBJS := $(addprefix ${T_REL}/,$(patsubst %.cpp,%.o,$(SRCS)))

all: pre test bin/kp

build/test/src:
	mkdir -p $@

build/release/src:
	mkdir -p $@

pre: build/test/src build/release/src

test: ${T_BIN}/test.out
	./$< -r short

${T_BIN}/%.o:%.cpp
	clang++ -DTEST ${DBG} ${OPTS} -c -o $@ $<

${T_REL}/%.o:%.cpp
	clang++ ${OPTS} ${REL} -c -o $@ $<

${T_BIN}/test.out: ${T_OBJS}
	clang++ -DTEST ${DBG} $^ ${LIBS} -lboost_unit_test_framework -o $@

bin/kp: ${R_OBJS}
	clang++ -DTEST ${REL} $^ ${LIBS} -o $@

clean:
	rm -rf bin/* build
