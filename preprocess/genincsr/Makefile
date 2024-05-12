exe = main

cc = "$(shell which g++)" 
flags = -std=c++11 -O3 -w -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-local-typedefs -Wno-unused-local-typedefs -Wno-unused-value 

src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)

$(exe): $(obj)
	$(cc) $(flags) -o $@ $^

%.o: %.cpp
	$(cc) $(flags) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(obj) $(exe)