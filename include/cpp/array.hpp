
#pragma once


template<typename T, int capacity>
class array
{
    public:
    T& operator[](int index)
    {
        if(index<0 || index>=capacity)
        {
            static T n;
            return n;
        }
        return data[index];
    }
    int at(T& result, int index)
    {
        if(index<0 || index>=capacity)
        {
            return false;
        }
        result=data[index];
        return true;
    }
    T data[capacity];
};
