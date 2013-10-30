/*
*	@author maggarwa@usc.edu
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/time.h>

#include "my402list.h"


int  My402ListLength(My402List* list){
	return list->num_members;
}

int  My402ListEmpty(My402List* list){
	 return list->num_members==0;
}

int  My402ListAppend(My402List* list, void* data){
	int success = 0;
	My402ListElem* anchor = &(list->anchor);
	
	//create new element of the list
	My402ListElem* element = (My402ListElem*) malloc(sizeof(My402ListElem));
	if(element!=NULL){
		memset(element, 0, sizeof(My402ListElem));
		element->obj = data;
			
		//Adding first element in the list
		if(anchor->next == NULL){
			anchor->next = element;
			element->next = anchor;
			element->prev = anchor;
			anchor->prev = element;
		} else {
			My402ListElem *last = My402ListLast(list);
			element->next = anchor;
			last->next = element;
			element->prev = last;
			anchor->prev = element;
		}
		success = 1;
		list->num_members++;
	}
		
	return success;
}

int  My402ListPrepend(My402List* list, void* data){
	int success = 0;
	My402ListElem* anchor = &(list->anchor);
	
	//create new element of the list
	My402ListElem* element = (My402ListElem*) malloc(sizeof(My402ListElem));
	if(element!=NULL){
		memset(element, 0, sizeof(My402ListElem));
		element->obj = data;
			
		//Adding first element in the list
		if(anchor->next == NULL){
			anchor->next = element;
			element->next = anchor;
			element->prev = anchor;
			anchor->prev = element;
		} else {
			My402ListElem *first = My402ListFirst(list);
			anchor->next = element;
			element->next = first;
			first->prev = element;
			element->prev = anchor;
		}
		success = 1;
		list->num_members++;
	}
		
	return success;
}

void My402ListUnlink(My402List* list, My402ListElem* elem){
	//if(elem!=&(list->anchor)){
	My402ListElem* next = elem->next;
	My402ListElem* prev = elem->prev;
	next->prev = prev;
	prev->next = next;
	list->num_members--;
	if(My402ListEmpty(list)){
		list->anchor.next=NULL;
		list->anchor.prev=NULL;
	}
	
	//delete the elem
	elem->next = NULL;
	elem->prev = NULL;
	elem->obj = NULL;
	free(elem);
	
	//}else{
		//list->anchor.next=NULL;
		//list->anchor.prev=NULL;
	//}
	
}

void My402ListUnlinkAll(My402List* list){
	/*My402ListElem* p = &(list->anchor);
	while(p!=NULL){
		My402ListElem* elem = p->next;
		My402ListUnlink(list,elem);
		p=p->next;
	}*/
	My402ListElem* p = My402ListLast(list);
	while(p!=NULL){
		My402ListUnlink(list,p);
		p = My402ListLast(list);
	}
}

int  My402ListInsertAfter(My402List* list, void* data, My402ListElem* elem){
	int success = 0;
		
	//create new element of the list
	My402ListElem* newElem = (My402ListElem*) malloc(sizeof(My402ListElem));
	if(newElem!=NULL){
		memset(newElem, 0, sizeof(My402ListElem));
		newElem->obj = data;
		
		My402ListElem* nextElem = elem->next;
		newElem->next = nextElem;
		elem->next = newElem;
		newElem->prev = elem;
		nextElem->prev = newElem;
		
		success = 1;
		list->num_members++;
	}
		
	return success;
}

int  My402ListInsertBefore(My402List* list, void* data, My402ListElem* elem){
	int success = 0;
		
	//create new element of the list
	My402ListElem* newElem = (My402ListElem*) malloc(sizeof(My402ListElem));
	if(newElem!=NULL){
		memset(newElem, 0, sizeof(My402ListElem));
		newElem->obj = data;
		
		My402ListElem* prevElem = elem->prev;
		newElem->next = elem;
		newElem->prev = prevElem;
		elem->prev = newElem;
		prevElem->next = newElem;
				
		success = 1;
		list->num_members++;
	}
		
	return success;
}


My402ListElem *My402ListFirst(My402List* list){
	My402ListElem* first = NULL;
	if(list->num_members!=0){
		first = list->anchor.next;
	}
	return first;
}

My402ListElem *My402ListLast(My402List* list){
My402ListElem* last = NULL;
	if(list->num_members!=0){
		last = list->anchor.prev;
	}
	return last;
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){
	My402ListElem* nextElem = NULL;
	if(elem->next!=&(list->anchor)){
		nextElem = elem->next;
	}
	return nextElem;
}

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){
	My402ListElem* prevElem = NULL;
	if(elem->prev!=&(list->anchor)){
		prevElem = elem->prev;
	}
	return prevElem;
}

My402ListElem *My402ListFind(My402List* list, void* data){
	My402ListElem* p = list->anchor.next;
	My402ListElem* elem = NULL;
	while(p!=NULL && p!=&(list->anchor)){
		if(p->obj==data){
			elem = p;
			break;
		}
		p=p->next;
	}
	return elem;
}

int My402ListInit(My402List* list){
	int result = 0;
	if(list!=NULL){
		list->num_members = 0;
		list->anchor.next = NULL;
		list->anchor.prev = NULL;
		list->anchor.obj = NULL;
		result = 1;
	}
	return result;
}


