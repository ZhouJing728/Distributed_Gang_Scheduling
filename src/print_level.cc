#include"../include/print_level.h"

PrintL::PrintL()
{  
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../config.ini", pt);
    int lel=pt.get<int>("print_level.value");
    PRINT_LEVEL=static_cast<PRT_LEVEL_E>(lel);
}

void PrintL::P_ERR(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_ERR) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_ERR, format, args);
        va_end(args);
    }
}

void PrintL::P_DBG(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_DBG) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_DBG, format, args);
        va_end(args);
    }
}

void PrintL::P_JOB(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_JOB) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_JOB, format, args);
        va_end(args);
    }
}

void PrintL::P_NODE(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_NODE) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_NODE, format, args);
        va_end(args);
    }
}

void PrintL::P_WRN(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_WRN) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_WRN, format, args);
        va_end(args);
    }
}

void PrintL::P_NOPRT(const char* format, ...)
{
    if (PRINT_LEVEL >= PrintL::PRT_LEVEL_E::PRINT_LEVEL_NOPRT) {
        va_list args;
        va_start(args, format);
        output(PrintL::PRT_LEVEL_E::PRINT_LEVEL_NOPRT, format, args);
        va_end(args);
    }
}

void PrintL::output(PrintL::PRT_LEVEL_E levelParam, const char *fmt, va_list args)
{
    const char* level_str;
    switch (levelParam) {
        case PRINT_LEVEL_ERR: level_str = "ERROR"; break;
        case PRINT_LEVEL_WRN: level_str = "WARNING"; break;
        case PRINT_LEVEL_JOB: level_str = "JOB"; break;
        case PRINT_LEVEL_DBG: level_str = "DEBUG"; break;
        case PRINT_LEVEL_NODE: level_str = "NODE"; break;
        default: level_str = "UNKNOWN"; break;
    }

    std::printf("[%s] ", level_str);
    std::vprintf(fmt, args);
    std::printf("\n");

}