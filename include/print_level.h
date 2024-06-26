#include<stdio.h>
#include<stdarg.h>
#include <functional>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>


class PrintL{
	public:
	enum PRT_LEVEL_E
	{
		PRINT_LEVEL_ERR,      
		PRINT_LEVEL_WRN,       
		PRINT_LEVEL_DBG,       
		PRINT_LEVEL_NODE, //FOR GLOBAL_SCHEDULER | LOCAL_SCHEDULER  
		PRINT_LEVEL_JOB,
		PRINT_LEVEL_NOPRT //FOR BASIC SERVER | CLIENT PRINTFS
	};
	PrintL();
	void P_ERR(const char* format, ...);
	void P_WRN(const char* format, ...);
	void P_DBG(const char* format, ...);
	void P_NODE(const char* format, ...);
	void P_JOB(const char* format, ...);
	void P_NOPRT(const char* format, ...);

	private:
	PRT_LEVEL_E PRINT_LEVEL;

	void output(PRT_LEVEL_E levelParam, const char *fmt, va_list args);
};