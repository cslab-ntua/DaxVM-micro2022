#include "read.h"
void function(char *buf, unsigned long long size){
	unsigned long long sum = 0;
		for(int i=0; i<(size/sizeof(unsigned long long)); i++){
			if(i%256==0) sum=0;
			sum+=((unsigned long long*)buf)[i];
		}
}
