/* 
* ThreadQueue.h 
* 
* Created on: 2015-10-26 
*/  

#ifndef THREADQUEUE_H_  
#define THREADQUEUE_H_  

#include <pthread.h>  
#include <iostream>  
#include <list>
#include <string>

using namespace std;
typedef list<string*> LISTOBJ;


class ThreadQueue
{
public:
	ThreadQueue(int QUEUESIZE = 20);
	~ThreadQueue();  
public:  
	bool Push(string *obj);
	string* Pop();
	bool IsEmpty();  
	bool IsFull();
	bool Remove(string* unit);
	bool isExist(string* unit);
private: 
	LISTOBJ* list_obj;
	unsigned int size;
	pthread_mutex_t queueMutex;
};  

#endif /* THREADQUEUE_H_ */  
