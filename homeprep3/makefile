TARGET = log_check
CXX = g++
FLAGS = -Werror -Wall
LIBNAME = liblog



$(TARGET) :  main.cpp $(LIBNAME).so 
	$(CXX) $(FLAGS) $< -L. -llog -Wl,-rpath,'$$ORIGIN' -o $@

$(LIBNAME).so : log.o 
	$(CXX) -shared $(FLAGS) $< -o $@

log.o : log.cpp log.h
	$(CXX) -c -fPIC $(FLAGS) $< -o $@

clean:
	rm -f $(TARGET) *.o *.so