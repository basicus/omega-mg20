#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "29";
	static const char MONTH[] = "04";
	static const char YEAR[] = "2009";
	static const double UBUNTU_VERSION_STYLE = 9.04;
	
	//Software Status
	static const char STATUS[] = "Beta";
	static const char STATUS_SHORT[] = "b";
	
	//Standard Version Type
	static const long MAJOR = 0;
	static const long MINOR = 2;
	static const long BUILD = 22;
	static const long REVISION = 132;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 65;
	#define RC_FILEVERSION 0,2,22,132
	#define RC_FILEVERSION_STRING "0, 2, 22, 132\0"
	static const char FULLVERSION_STRING[] = "0.2.22.132";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY = 22;
	

#endif //VERSION_h
