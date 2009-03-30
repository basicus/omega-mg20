#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "24";
	static const char MONTH[] = "03";
	static const char YEAR[] = "2009";
	static const double UBUNTU_VERSION_STYLE = 9.03;
	
	//Software Status
	static const char STATUS[] = "Beta";
	static const char STATUS_SHORT[] = "b";
	
	//Standard Version Type
	static const long MAJOR = 0;
	static const long MINOR = 2;
	static const long BUILD = 21;
	static const long REVISION = 122;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 65;
	#define RC_FILEVERSION 0,2,21,122
	#define RC_FILEVERSION_STRING "0, 2, 21, 122\0"
	static const char FULLVERSION_STRING[] = "0.2.21.122";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY = 21;
	

#endif //VERSION_h
