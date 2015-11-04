#include "ThreadQueue.h"

//------------------------------------------------------ 
  
ThreadQueue::ThreadQueue(int QUEUESIZE)
{  
	pthread_mutex_init(&queueMutex, NULL);
	//front = rear = 0;  
	size = QUEUESIZE;  
	list_obj = new LISTOBJ();
	//    pthread_mutex_lock(&queueMutex);
}  
//------------------------------------------------------  
  
bool ThreadQueue::Push(string* obj)
{  
	if(!obj)
		return false;
	pthread_mutex_lock(&queueMutex);  
	if(IsFull())  
	{  
		//cout << "Queue is full!" << endl;
		pthread_mutex_unlock(&queueMutex);  
		return false;
	}
	string* strp = new string(*obj);
	list_obj -> push_back(strp);
	//list[rear] = obj;
	//rear = (rear + 1) % size;

	pthread_mutex_unlock(&queueMutex);  

	return true;  
}  
//------------------------------------------------------  
  
string* ThreadQueue::Pop()
{  
	string* temp;
	pthread_mutex_lock(&queueMutex);  
	if(IsEmpty())  
	{  
		//cout << "Queue is empty!" << endl;
		pthread_mutex_unlock(&queueMutex);
		return NULL; 
	}  
	temp = list_obj->front();
	list_obj->pop_front();
	//temp = list[front];  
	//front = (front + 1) % size;  

	pthread_mutex_unlock(&queueMutex);  

	return temp;
}  
//------------------------------------------------------  

bool ThreadQueue::Remove(string* unit){
	// 删除正在队列中的元素
	LISTOBJ::iterator pointer;
	bool status = false;
	pthread_mutex_lock(&queueMutex); 
	for ( pointer = list_obj->begin(); pointer != list_obj->end();)
	{
		if ( **pointer == *unit ){
			pointer = list_obj->erase(pointer);
			status = true;
		}
		else
			pointer++;
	}
	pthread_mutex_unlock(&queueMutex);
	return status;
}

bool ThreadQueue::isExist(string* unit){
	// 删除正在队列中的元素
	LISTOBJ::iterator pointer;
	bool status = false;
	pthread_mutex_lock(&queueMutex); 
	for ( pointer = list_obj->begin(); pointer != list_obj->end();)
	{
		if ( **pointer == *unit ){
			status = true;
			break;
		}
		else
			pointer++;
	}
	pthread_mutex_unlock(&queueMutex);
	return status;
}

bool ThreadQueue::IsEmpty()  
{
	if(list_obj->empty())  
		return true;  
	else  
		return false;
	/*	if(rear == front)  
	return true;  
	else  
	return false;*/  
}  
//------------------------------------------------------  
  
bool ThreadQueue::IsFull()  
{
	if(list_obj->size() == size)  
		return true;  
	else  
		return false;
	//if((rear + 1) % size == front)  
	//	return true;  
	//else  
	//	return false;
}  
//------------------------------------------------------  

ThreadQueue::~ThreadQueue()  
{
	pthread_mutex_unlock(&queueMutex);
	delete list_obj;
	list_obj = NULL;
	//delete []list;
}  
//------------------------------------------------------  
