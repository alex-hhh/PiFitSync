#include "Tools.h"

#include <iostream>
#include <iomanip>
#include <locale>

namespace FitSync {

/** Print a hex dump of 'data' to the stream 'o'.  The data is printed on
 * lines with the address, character representation and hex representation on
 * each line.  This hopefully makes it easy to determine the contents of both
 * character and binary data.
 */
void DumpData (const unsigned char *data, int size, std::ostream &o)
{
    int ncols = 16;
    int nrows = size / ncols;
    int spill = size - nrows * ncols;
    std::ios::fmtflags saved = o.flags();

    auto pchar = [] (char c, std::locale &loc) -> char
        {
            char npc = '?';         // char to print if the character is not
            // printable
            if (std::isspace (c, loc) && c != ' ')
                return npc;

            if (std::isprint (c, loc))
                return c;

            return npc;
        };

    std::locale loc = o.getloc();
    o << std::hex << std::setfill ('0');

    for (int row = 0; row < nrows; ++row)
    {
        o << std::setw (4) << row*ncols << " ";
        for (int col = 0; col < ncols; ++col)
        {
            int idx = row * ncols + col;
            o << pchar(data[idx], loc);
        }
        o << '\t';
        for (int col = 0; col < ncols; ++col)
        {
            int idx = row * ncols + col;
            o << std::setw (2) << static_cast<unsigned>(data[idx]) << " ";
        }
        o << '\n';
    }

    if (spill)
    {
        o << std::setw (4) << nrows*ncols << " ";
        for (int idx = size - spill; idx < size; ++idx)
        {
            o << pchar (data[idx], loc);
        }

        for (int idx = 0; idx < ncols - spill; ++idx)
        {
            o << ' ';
        }

        o << '\t';
        for (int idx = size - spill; idx < size; ++idx)
        {
            o << std::setw (2) << static_cast<unsigned>(data[idx]) << " ";
        }

        o << '\n';
    }

    o.flags (saved);
}

void PutTimestamp(std::ostream &o)
{
    struct timespec tsp;
    
    if (clock_gettime(CLOCK_REALTIME, &tsp) < 0) {
        perror("clock_gettime");
        return;
    }
    struct tm tm = *localtime(&tsp.tv_sec);

    unsigned msec = tsp.tv_nsec / 1000000;
    
    o << std::setfill('0')
      << std::setw(4) << tm.tm_year + 1900
      << '-' << std::setw(2) << tm.tm_mon + 1
      << '-' << std::setw(2) << tm.tm_mday
      << ' ' << std::setw(2) << tm.tm_hour
      << ':' << std::setw(2) << tm.tm_min
      << ':' << std::setw(2) << tm.tm_sec
      << '.' << std::setw(4) << msec << ' ';
}


};                                      // end namespace AntfsTool

