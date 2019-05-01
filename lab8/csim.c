/*
 * Name:Kang Huquan
 * LoginID:ics517021910839
 */

#include "cachelab.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdio.h"
#include "getopt.h"
#include "string.h"

/*
 * Definition of bool type
 */
typedef int bool;
#define true 1
#define false 0

/*
 * Declaration of used funcion
 */
void printHelp();
void alloSpace();
void cacheSim(const char* t);
int load(int addr, int size);

/*
 * Definition of global function
 */
int s,E,b;
int missNum = 0;
int hitNum=0;
int evictNum=0;
int **set;//The set simulate the true set with multi-set of multi-line. e.g. set[setIndex][lineNum]
bool verbose = false;//Controled by argument <-v>. If verbose is true, print detail information of hit, miss or evict.

int main(int argc, char* argv[])
{
    	int opt;//arguments parser
    	const char* t;//filename
    	while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1){
		switch(opt){
		case 'h':
		    	printHelp();
			return -1;
		case 'v':
			verbose = true;
			break;
		case 's':
			s=atoi(optarg);
			break;
		case 'E':
			E=atoi(optarg);
			break;
		case 'b':
			b=atoi(optarg);
			break;
		case 't':
			t=optarg;
			break;
		default:
			printHelp();
			return -1;
		}
	}
	
	cacheSim(t);

    	printSummary(hitNum, missNum, evictNum);

    	return 0;
}

/*
 * Function Name: printHelp
 * Description: Print help message to user when wrong input or input argument option <-h>
 */
void printHelp(){
	printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <    num> -t <file>\n");
	printf("Options:\n");
	printf("\t-h\tPrint this help message.\n");
	printf("\t-v\tOptional verbose flag.\n");
	printf("\t-s <num>\tNumber of set index bits.\n");
	printf("\t-E <num>\tNumber of lines p    er set.\n");
	printf("\t-b <num>\t Number of block offset bits.\n");
	printf("\t-t <file>\tTrace file.\n");
	printf("\n");
	printf("Examples:\n");
	printf("linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
	printf("linux>  ./c    sim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

/*
 * Function Name: catheSim
 * Description: Simulate the behavior of cathe.
 *		This function allocate space for lines of set and open file to read it.
 */
void cacheSim(const char* t){
	set = malloc((1<<s)*sizeof(int*));
	memset(set, 0, 1<<s);

	FILE* file = fopen(t, "r");
	if(!file){
		printf("File %s can't be opened!", t);
		return;
	}
	
	char ch;
	while((ch = fgetc(file))!=EOF){
		if(ch == ' '){
			int addr, size;
			fscanf(file,"%c %x,%d", &ch, &addr, &size);
			
			int result = load(addr, size);
			if(ch == 'M')
				hitNum++;
			
			if(verbose){
				printf("%c %x,%d ", ch, addr, size);
				switch(result){
					case 0:
						printf("miss\n");
						break;
					case 1:
						printf("hit\n");
						break;
					case -1:
						printf("evict\n");
						break;
				}
			}
		}
		while(fgetc(file) != '\n');
	}
}

/*
 *****************************************************************************************
 ******************Some small tools to serve function load(addr,size)*********************
 */

/*
 * Get the SetIndex according to the address number.
 */
int getSetIndex(int addr){
	return (addr>>b) & ((0x1<<s)-1);
}

/*
 * Get the tag code.
 * Note: The tag include a parity bit at last, which indicate the validity of line.
 */
int getTag(int addr){
	return (addr & ~((0x1<<(b+s))-1)) | 0x1;
}

/*
 * Put the least-recently used addr to the top line, and flush others downside.
 */
void flushSet(int setIndex, int index){
	int temp = set[setIndex][index];
	for(int j=index; j>0; j--)
		set[setIndex][j] = set[setIndex][j-1];
	set[setIndex][0] = temp;
}

/*
 * Access one set by address directly and tell whether the address has existed.
 */
bool accessSet(int addr){
	int setIndex = getSetIndex(addr);
	bool flag = false;
	for(int i=0; i<E; i++){
		if(set[setIndex][i] == getTag(addr)){
			flushSet(setIndex, i);
			flag = true;		
		}
	}
	return flag;
}

/*
 * Check all lines by setIndex, and tell whether one set has been full with all lines.
 */
bool noValidLine(int setIndex){
	bool flag = true;
	for(int i=0; i<E; i++)
		if(!(set[setIndex][i] & 0x1))
			flag = false;
	return flag;
}
/*
 *******************************small tools END****************************************
 **************************************************************************************
 */


/*
 * Function Name: load
 * Description: Simulate a behavior like read a data from addr.
 *		The cache will record the address information and judge 
 *	what's the result(hit, miss or evict), acting with strategy of
 *	LRU.
 *		The return value 0 indicate miss, value 1 indicate hit,
 *	and value -1 indicate evict.
 *		We do not record the true block value, just record the
 * 	address and one bit at the least significant position to show
 * 	the validity of special line.
 */
int load(int addr, int size){
	int setIndex = getSetIndex(addr);
	if(set[setIndex]==NULL){
		set[setIndex] = malloc(E*sizeof(int));
		memset(set[setIndex], 0, E*sizeof(int));
		set[setIndex][0] = getTag(addr);
		missNum++;
		return 0;
	}
	else if(accessSet(addr)){
		hitNum++;
		return 1;
	}
	else{
		missNum++;
		if(noValidLine(setIndex))
			evictNum++;
		set[setIndex][E-1] = getTag(addr);
		flushSet(setIndex, E-1);
		return -1;
	}
}
