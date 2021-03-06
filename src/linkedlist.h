// Based on https://github.com/StevenCyb/SecuredLinkedList

#ifndef LinkedList_h
#define LinkedList_h

template <class T>
struct LinkedListNode
{
	T data;
	LinkedListNode<T> *next;
};

template <class T>
class LinkedList
{
private:
	unsigned int listSize;
	LinkedListNode<T> *root;
	LinkedListNode<T> *leaf;

public:
	LinkedList(void);
	~LinkedList(void);
	virtual unsigned int size();
	virtual void push(T t);
	virtual T pop();
	virtual void add(unsigned int index, T t);
	virtual T get(unsigned int index);
	virtual void unshift(T t);
	virtual T shift();
	virtual void clear();
	virtual T rotate();
};

template <typename T>
LinkedList<T>::LinkedList()
{
	listSize = 0;
}

template <typename T>
LinkedList<T>::~LinkedList()
{
	listSize = 0;
	delete root;
	delete leaf;
}

template <typename T>
unsigned int LinkedList<T>::size()
{
	return listSize;
}

template <typename T>
void LinkedList<T>::push(T t)
{
	LinkedListNode<T> *cache = new LinkedListNode<T>();
	cache->data = t;
	if (listSize <= 0)
	{
		root = cache;
		leaf = cache;
		listSize += 1;
	}
	else if (listSize == 1)
	{
		root->next = cache;
		leaf = cache;
		listSize += 1;
	}
	else
	{
		leaf->next = cache;
		leaf = cache;
		listSize += 1;
	}
}

template <typename T>
T LinkedList<T>::pop()
{
	if (listSize <= 0)
	{
		return T();
	}
	else if (listSize > 1)
	{
		T cache = leaf->data;
		LinkedListNode<T> *previous = root;
		while (previous->next->next != NULL)
		{
			previous = previous->next;
		}
		leaf = previous;
		leaf->next = NULL;
		listSize -= 1;
		return cache;
	}
	else
	{
		T cache = root->data;
		root = NULL;
		leaf = NULL;
		listSize -= 1;
		return cache;
	}
}

template <typename T>
void LinkedList<T>::add(unsigned int index, T t)
{
	if (index <= 0)
	{
		unshift(t);
	}
	else if (index >= (listSize - 1))
	{
		push(t);
	}
	else
	{
		LinkedListNode<T> *previous = root;
		for (unsigned int i = 1; i < index; i++)
		{
			previous = previous->next;
		}
		LinkedListNode<T> *cache = new LinkedListNode<T>();
		cache->data = t;
		cache->next = previous->next;
		previous->next = cache;
		listSize += 1;
	}
}

template <typename T>
T LinkedList<T>::get(unsigned int index)
{
	if (index <= 0)
	{
		return root->data;
	}
	else
	{
		LinkedListNode<T> *previous = root;
		for (unsigned int i = 0; i < index; i++)
		{
			previous = previous->next;
		}
		return previous->data;
	}
}

template <typename T>
void LinkedList<T>::unshift(T t)
{
	if (listSize <= 0)
	{
		push(t);
	}
	else
	{
		LinkedListNode<T> *cache = new LinkedListNode<T>();
		cache->data = t;
		cache->next = root;
		root = cache;
		listSize += 1;
		if (listSize == 2)
		{
			leaf = root->next;
		}
	}
}

template <typename T>
T LinkedList<T>::shift()
{
	if (listSize <= 0)
	{
		return T();
	}
	if (listSize > 1)
	{
		T cache = root->data;
		root = root->next;
		listSize -= 1;
		return cache;
	}
	else
	{
		return pop();
	}
}

template <typename T>
void LinkedList<T>::clear()
{
	while (listSize > 0)
	{
		shift();
	}
}

template <typename T>
T LinkedList<T>::rotate()
{
	if (listSize > 1)
	{
		T cache = root->data;
		leaf->next = root;
		leaf = root;
		root = root->next;
		leaf->next = NULL;
		return cache;
	}
	else
	{
		return root->data;
	}
}

#endif