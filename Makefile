OBJS = reciter.o sam.o render.o main.o debug.o processframes.o createtransitions.o

CXX = g++

# libsdl present
CXXFLAGS =  -Wall -O2 -DUSESDL `sdl-config --cflags`
LFLAGS = `sdl-config --libs`

# no libsdl present
#CCFLAGS =  -Wall -O2
#LFLAGS = 

sam: $(OBJS)
	$(CC) -o sam $(OBJS) $(LFLAGS)

%.o: src/%.cc
	$(CXX) $(CXXFLAGS) -c $<

package: 
	tar -cvzf sam.tar.gz README.md Makefile sing src/

clean:
	rm *.o
