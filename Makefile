CXX="/opt/rh/devtoolset-2/root/usr/bin/g++"
CXXFLAGS="-std=gnu++11"
CFLAGS="-g"
INC="./"

all : example reflect_tool 

example : json11.hpp example.cpp reflect_base.h
	$(CXX) $(CXXFLAGS) $(CFLAGS) json11.cpp example.cpp -o ./example -I$(INC) 

reflect_tool: json11.hpp reflect_tool.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS) json11.cpp reflect_tool.cpp -o ./reflect_tool -I$(INC) 

	 
