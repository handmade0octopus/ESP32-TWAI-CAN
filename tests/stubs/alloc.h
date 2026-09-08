#pragma once
#include <cstdlib>
void* stubCalloc(size_t count, size_t size);
void stubFree(void* ptr);
#define calloc stubCalloc
#define free stubFree
