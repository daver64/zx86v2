
#pragma once

#include <system.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
/*
 No stdlib, no RTTI and no exceptions... :)
*/
#include <cpp/list.hpp>
#include <cpp/stack.hpp>
#include <cpp/array.hpp>
#include <cpp/tree.hpp>
#include <cpp/string.hpp>
char *getline(FILE *fp);
list<char *> string_to_list(char *line);
void print_list(list<char *> &l);
void destroy_list(list<char *> &l);