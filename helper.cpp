#include "helper.h"
#include <sstream>
#include <iomanip>

std::string hexDump(const void *addr, size_t len, const std::string &desc)
{
    std::stringstream str;
    size_t i;
    std::string asciiBuf;
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (!desc.empty()) {
        str << desc << ":\n";
    }

    if (len == 0) {
        str << "  ZERO LENGTH\n";
        return str.str();
    }
    if (len < 0) {
        str << "  NEGATIVE LENGTH: " << len << std::endl;
        return str.str();
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                str << " " << asciiBuf << std::endl;
                asciiBuf.clear();
            }

            // Output the offset.
            str << std::setw(4) << std::setfill('0') << std::right << std::hex << i;
        }

        // Now the hex code for the specific character.
        str << " " << std::setw(2) << std::setfill('0') << std::right << std::hex << (uint)(pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            asciiBuf.push_back('.');
        }
        else {
            asciiBuf.push_back(pc[i]);
        }
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        str << "   ";
        i++;
    }

    // And print the final ASCII bit.
    str << " " << asciiBuf << std::endl;
    return str.str();
}
