LOCAL := ${HOME}/local
IGNORE := -Wc++11-narrowing
OPTS := -std=c++20 ${IGNORE} -I${LOCAL}/include -L${LOCAL}/lib -Wl,-rpath=${LOCAL}/lib
DBG := -g
LIBS := -lstdc++fs -static -lboost_json -lboost_chrono -lboost_program_options -lboost_filesystem

all: test bin/kp

test: bin/test.out
	./$< -r short

bin/test.out: src/main.cpp src/project.cpp src/workfolder.cpp
	clang++ -DTEST ${DBG} ${OPTS} $^ ${LIBS} -lboost_unit_test_framework -o $@

bin/kp: src/main.cpp src/project.cpp src/workfolder.cpp
	clang++ ${OPTS} $^ ${LIBS} -o $@

clean:
	rm -rf bin/*
