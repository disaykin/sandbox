all:
	clang -std=c++14 -Wall -Wextra -lpthread -lc++ rate_limit.cpp
