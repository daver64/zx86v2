#pragma once

/*
 No stdlib, no RTTI and no exceptions... :)
*/

template<typename T>
class stack
{
public:
    stack()
    {
        capacity=16;
        data = new T[capacity];
        stack_index=0;
    }
    ~stack()
    {
        delete[] data;
    }
    int push(const T d)
    {
        data[stack_index]=d;
        stack_index++;
        if(stack_index>=capacity)
        {
            T *ndata=new T[capacity*2];
            for(int i=0;i<stack_index;i++)
            {
                ndata[i]=data[i];
            }
            T *d=data;
            data=ndata;
            capacity*=2;
            delete[] d;
            return true;
        }
        return true;
    }
    int pop(T& result)
    {
        if(stack_index<1)
            return false;
        result=data[--stack_index];
        return true;
    }
    int count()
    {
        return stack_index;
    }
    T *data;
    int stack_index;
    int capacity;
};