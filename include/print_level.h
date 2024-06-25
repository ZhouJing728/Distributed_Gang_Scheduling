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
		PRINT_LEVEL_NODE,   
		PRINT_LEVEL_JOB,
		PRINT_LEVEL_NOPRT   
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


// PRT_LEVEL_E intToPL(int lel);
// void output(PRT_LEVEL_E levelParam, const char *fmt, ...);
// void read_config(const char*);

// #define P_ERR(...) do { if (current_level >= PRINT_LEVEL_ERR) output(PRINT_LEVEL_ERR, __VA_ARGS__); } while (0)
// #define P_WRN(...) do { if (current_level >= PRINT_LEVEL_WRN) output(PRINT_LEVEL_WRN, __VA_ARGS__); } while (0)
// #define P_NODE(...) do { if (current_level >= PRINT_LEVEL_NODE) output(PRINT_LEVEL_NODE, __VA_ARGS__); } while (0)
// #define P_DBG(...) do { if (current_level >= PRINT_LEVEL_DBG) output(PRINT_LEVEL_DBG, __VA_ARGS__); } while (0)
// #define P_JOB(...) do { if (current_level >= PRINT_LEVEL_JOB) output(PRINT_LEVEL_JOB, __VA_ARGS__); } while (0)
// #define P_NOPRT(...) do { if (current_level >= PRINT_LEVEL_NOPRT) output(PRINT_LEVEL_NOPRT, __VA_ARGS__); } while (0)