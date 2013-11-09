#include "ring.h"

// return 1 if na<nb<id
int InRange(unsigned int id, unsigned int na, unsigned int nb){
  if (na < id){
    if (na < nb && nb < id)
      return 1;
    else
      return 0;
  }else if(na > id){
    if (na < nb || nb < id)
      return 1;
    else
      return 0;
  }else{
    return 0;
  }
}

//return 1 if n<=start<node
int InRangeA(unsigned int n, unsigned int start, unsigned int node){
  if (n < node){
    if(start >= n && start < node)
      return 1;
    else 
      return 0;
  }else if(n > node){
    if (start >= n || start < node)
      return 1;
    else 
      return 0;
  }else{  // exception
    //printf("InRangeA exception: n 0x%08x, start 0x%08x, node 0x%08x\n", n, start, node);
    return 1;
  }
}

// return 1 if not ia<id<=ib
int NotInRange(unsigned int id, unsigned int ia, unsigned int ib){
  if (ia == ib){  // this is the first node, and we are the second node, not not-in-range
    return 0;
  }else if(ia > ib){   // last node in the ring
    if (id > ia || id <= ib){
      return 0;
    }
  }else{
    if (id > ia && id <= ib){
      return 0;
    }
  }
  
  // otherwise, not-in-range
  return 1;
}

// return a+b, without overflow
unsigned int RingPlus(unsigned int a, unsigned int b){
  unsigned int diff;
  unsigned int ret;

  diff = HASHMAX - a;
  if (diff < b){
    ret = b - diff - 1;
  }else{
    ret = a + b;
  }

  return ret;
}

// return a-b, without overflow
unsigned int RingMinus(unsigned int a, unsigned int b){
  unsigned int ret;

  if (a >= b){
    ret = a - b;
  }else{
    ret = HASHMAX - (b - a - 1);
  }

  return ret;
}

