#pragma once

template <typename T>
class avl_node
{
public:
    T key;
    avl_node<T> *left;
    avl_node<T> *right;
    int height;
};

int max(int a, int b);

// Calculate height
template<typename T>
int avl_height(avl_node<T> *N)
{
    if (N == NULL)
        return 0;
    return N->height;
}


// New node creation
template<typename T>
avl_node<T> *avl_new_node(T key)
{
    avl_node<T> *node = new avl_node<T>;
    node->key = key;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    return (node);
}

// Rotate right
template<typename T>
avl_node<T> *avl_right_rotate(avl_node<T> *y)
{
    avl_node<T> *x = y->left;
    avl_node<T> *T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = max(avl_height(y->left),
                    avl_height(y->right)) +
                1;
    x->height = max(avl_height(x->left),
                    avl_height(x->right)) +
                1;
    return x;
}

// Rotate left
template<typename T>
avl_node<T> *avl_left_rotate(avl_node<T> *x)
{
    avl_node<T> *y = x->right;
    avl_node<T> *T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = max(avl_height(x->left),
                    avl_height(x->right)) +
                1;
    y->height = max(avl_height(y->left),
                    avl_height(y->right)) +
                1;
    return y;
}

// Get the balance factor of each node
template<typename T>
int avl_get_balance_factor(avl_node<T> *N)
{
    if (N == NULL)
        return 0;
    return avl_height(N->left) -
           avl_height(N->right);
}

// Insert a node
template<typename T>
avl_node<T> *avl_insert_node(avl_node<T> *node, int key)
{
    // Find the correct postion and insert the node
    if (node == NULL)
        return (avl_new_node<T>(key));
    if (key < node->key)
        node->left = avl_insert_node(node->left, key);
    else if (key > node->key)
        node->right = avl_insert_node(node->right, key);
    else
        return node;

    // Update the balance factor of each node and
    // balance the tree
    node->height = 1 + max(avl_height(node->left),
                           avl_height(node->right));
    int balanceFactor = avl_get_balance_factor(node);
    if (balanceFactor > 1)
    {
        if (key < node->left->key)
        {
            return avl_right_rotate(node);
        }
        else if (key > node->left->key)
        {
            node->left = avl_left_rotate(node->left);
            return avl_right_rotate(node);
        }
    }
    if (balanceFactor < -1)
    {
        if (key > node->right->key)
        {
            return avl_left_rotate(node);
        }
        else if (key < node->right->key)
        {
            node->right = avl_right_rotate(node->right);
            return avl_left_rotate(node);
        }
    }
    return node;
}

// Node with minimum value
template<typename T>
avl_node<T> *avl_node_with_minimum_value(avl_node<T> *node)
{
    avl_node<T> *current = node;
    while (current->left != NULL)
        current = current->left;
    return current;
}

// Delete a node
template<typename T>
avl_node<T> *avl_delete_node(avl_node<T> *root, int key)
{
    // Find the node and delete it
    if (root == NULL)
        return root;
    if (key < root->key)
        root->left = avl_delete_node(root->left, key);
    else if (key > root->key)
        root->right = avl_delete_node(root->right, key);
    else
    {
        if ((root->left == NULL) ||
            (root->right == NULL))
        {
            avl_node<T> *temp = root->left ? root->left : root->right;
            if (temp == NULL)
            {
                temp = root;
                root = NULL;
            }
            else
                *root = *temp;
            delete temp;
        }
        else
        {
            avl_node<T> *temp = avl_node_with_minimum_value(root->right);
            root->key = temp->key;
            root->right = avl_delete_node(root->right,
                                     temp->key);
        }
    }

    if (root == NULL)
        return root;

    // Update the balance factor of each node and
    // balance the tree
    root->height = 1 + max(avl_height(root->left),
                           avl_height(root->right));
    int balanceFactor = avl_get_balance_factor(root);
    if (balanceFactor > 1)
    {
        if (avl_get_balance_factor(root->left) >= 0)
        {
            return avl_right_rotate(root);
        }
        else
        {
            root->left = avl_left_rotate(root->left);
            return avl_right_rotate(root);
        }
    }
    if (balanceFactor < -1)
    {
        if (avl_get_balance_factor(root->right) <= 0)
        {
            return avl_left_rotate(root);
        }
        else
        {
            root->right = avl_right_rotate(root->right);
            return avl_left_rotate(root);
        }
    }
    return root;
}

// Print the tree
template<typename T>
void avl_print_tree(avl_node<T> *root, char *indent, bool last)
{
    if (root != nullptr)
    {
        printf(indent);
        if (last)
        {
            printf("R----");
            printf(indent);
            indent=strcat(indent,"    ");
            //printf("   ");
        }
        else
        {
            printf("L----");
            printf(indent);
            indent=strcat(indent,"|   ");
            //printf("|  ");
        }
        printf("%d\n",root->key);
       // cout << root->key << endl;
        avl_print_tree(root->left, indent, false);
        avl_print_tree(root->right, indent, true);
    }
}