
output: client-phase1.cc client-phase2.cc client-phase3.cc client-phase4.cc client-phase5.cc 
	g++ -std=c++17 -pthread -o client-phase1 client-phase1.cc
	g++ -std=c++17 -pthread -o client-phase2 client-phase2.cc 
	g++ -std=c++17 -pthread -o client-phase3 client-phase3.cc
	g++ -std=c++17 -pthread -o client-phase4 client-phase4.cc
	g++ -std=c++17 -pthread -o client-phase5 client-phase5.cc
	

phase1: client-phase1.cc
	g++ -std=c++17 -pthread -o client-phase1 client-phase1.cc

phase2: client-phase2.cc
	g++ -std=c++17 -pthread -o client-phase2 client-phase2.cc

phase3: client-phase3.cc
	g++ -std=c++17 -pthread -o client-phase3 client-phase3.cc

phase4: client-phase4.cc
	g++ -std=c++17 -pthread -o client-phase4 client-phase4.cc

phase5: client-phase5.cc
	g++ -std=c++17 -pthread -o client-phase5 client-phase5.cc

.PHONY: clean

# clean:
# 	rm -- !(*.cc|*.cpp|*.c) 


