
#pragma once

/*
 No stdlib, no RTTI and no exceptions... :)
*/

template <typename T>
struct node
{
    T data;
    node *next;
};

template <typename T>
class list
{
private:
    node<T> *head;
    node<T> *tail;

public:
    list()
    {
        head = NULL;
        tail = NULL;
    }
    void push_back(T n)
    {
        node<T> *tmp = new node<T>;
        tmp->data = n;
        tmp->next = NULL;

        if (head == NULL)
        {
            head = tmp;
            tail = tmp;
        }
        else
        {
            tail->next = tmp;
            tail = tail->next;
        }
    }

    node<T> *gethead()
    {
        return head;
    }


    static void concatenate(node<T> *a, node<T> *b)
    {
        if (a != NULL && b != NULL)
        {
            if (a->next == NULL)
            {
                a->next = b;
            }
            else
            {
                concatenate(a->next, b);
            }
        }
        else
        {
            printf("Either a or b is NULL\n");
        }
    }

    void push_front(T n)
    {
        node<T> *tmp = new node<T>;
        tmp->data = n;
        tmp->next = head;
        head = tmp;
    }

    void push_after(node<T> *a, T value)
    {
        node<T> *p = new node<T>;
        p->data = value;
        p->next = a->next;
        a->next = p;
    }

    void del(node<T> *before_del)
    {
        node<T> *tmp;
        tmp = before_del->next;
        before_del->next = tmp->next;
        delete tmp;
    }
};
