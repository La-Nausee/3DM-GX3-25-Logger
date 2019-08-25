CXXFLAGS := -lpthread -lm
all:logger serial
logger:logger.o
	g++ -o $@ $^ $(CXXFLAGS)
serial:serial.o
	g++ -o $@ $^ $(CXXFLAGS)
logger.o:logger.cpp
	g++ -c -o $@ $^ $(CXXFLAGS)
serial.o:serial.cpp
	g++ -c -o $@ $^ $(CXXFLAGS)	
clean:
	rm logger.o logger serial.o serial
