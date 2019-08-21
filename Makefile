CXXFLAGS := -lpthread -lm -lwiringPi

logger:logger.o
	g++ -o $@ $^ $(CXXFLAGS)
logger.o:logger.cpp
	g++ -c -o $@ $^ $(CXXFLAGS)
clean:
	rm logger.o logger
