#ifndef HELPER_H
#define HELPER_H

#include <string>

std::string hexDump(const void *addr, size_t len, const std::string &desc = std::string());

#endif//HELPER_H
