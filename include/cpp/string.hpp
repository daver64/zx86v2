
#pragma once
#include <system.h>
#include <cstring>
#include <cstdint>

class string
{
public:
    string()
    {
        str = new char[16];
        length = 0;
        buffersize = 16;
        str[0] = 0;
    }
    string(const string &other)
    {
        str = new char[other.buffersize];
        memcpy(str, other.str, other.buffersize);
        buffersize = other.buffersize;
        length = other.length;
    }
    string(const char *other)
    {
        length = strlen(other);
        buffersize = next_pow2(length);
        str = new char[buffersize];
        memset(str,0,buffersize);
        strcat(str, other);
        //printf("len=%d, buffersize=%d, str=%s copied from %s\n",length,buffersize,str,other);
    }
    ~string()
    {
        delete[] str;
    }
    const char *c_str()
    {
        return str;
    }
    string& operator+(string other)
    {
        int total_len = length + other.length;
        int nbuffersize = next_pow2(total_len);
        char *nbuffer = new char[nbuffersize];
        nbuffer = strcat(nbuffer, str);
        buffersize = nbuffersize;
        delete[] str;
        str = nbuffer;
        strcat(str, other.str);
        length = strlen(str);
        return *this;
    }
    string& operator+(const char *other)
    {
        string o(other);
        return (*this + o);
    }
    char *str;
    int length;
    int buffersize;
};