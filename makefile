all: g++ main.cpp sim_mem.cpp 
	g++ main.cpp sim_mem.cpp -o main
all-GDB: main.cpp sim_mem.cpp 
	g++ main.cpp sim_mem.cpp -o main