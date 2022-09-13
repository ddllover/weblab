all:agent router
agent:agent.o
	g++ -g -o agent agent.o 
router:router.o
	g++ -g -o router router.o -pthread
agent.o:agent.cpp
	g++ -g -c agent.cpp
router.o:router.cpp
	g++ -g -c router.cpp -pthread
clean:all
	rm all