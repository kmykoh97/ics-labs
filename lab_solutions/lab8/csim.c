/*Meng Yit Koh ics517030990022*/

#include "cachelab.h"

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>



#define HIT 1
#define MISS 2
#define EVICTION 4

typedef struct Cache
{
	int v, s, E, b, hit, miss, eviction, cacheAccessNo;
	int * tag;
	int * history;
} Cache;

void printHelpMessage(char * programName);
Cache * initCache(int v, int s, int E, int b);
void performCaching(Cache * cache, long address, long size);
void freeCache(Cache * cache);

void printHelpMessage(char * programName)
{
	const char * helpOption = 
		"Options:\n"
			"  -h         Print this help message.\n"
            		"  -v         Optional verbose flag.\n"
            		"  -s <num>   Number of set index bits.\n"
            		"  -E <num>   Number of lines per set.\n"
            		"  -b <num>   Number of block offset bits.\n"
            		"  -t <file>  Trace file.\n";
	printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", programName);
	printf("%s\n", helpOption);
	printf("Examples:\n"
               "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
               "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
           programName, programName);
}

Cache * initCache(int v, int s, int E, int b)
{
	Cache * cache = (Cache *)malloc(sizeof(Cache));
	cache -> v = v;
	cache -> s = s;
	cache -> E = E;
	cache -> b = b;
	cache -> hit = cache -> miss = cache -> eviction, cache -> cacheAccessNo = 0;
	
	size_t sizeToAllocate = (1 << s) * E * sizeof(int); // allocate size of number of set(2**s) * association(E). Each element in array can hold tag or history of size 4 bytes(int)
	cache -> tag = (int *)malloc(sizeToAllocate);
	cache -> history = (int *)malloc(sizeToAllocate);

	memset(cache -> tag, 0, sizeToAllocate);
	memset(cache -> history, 0, sizeToAllocate);

	return cache;
}

void performCaching(Cache * cache, long address, long size)
{
	int set, tag, result = 0;

	address = address >> cache -> b; // remove the the block offsets bits from the address as they are not needed in this simulator. A hit tag and set means the data is there in cache. We do not need to get the value.
	set = address & ((1 << cache -> s) - 1); // this is to get the set bits from address
	tag = address >>= cache -> s; //  this is to get the tag bits from the address

	int setNo = set * cache -> E; // to get the position of the set for this address if available
	cache -> cacheAccessNo++;
	int temp = setNo;
	int tempHistory = 0x7FFFFFFF; // initiate this variable to the largest possible int value

	for (int i = setNo; i < setNo + cache -> E; i++) {
		if (cache -> history[i] && cache -> tag[i] == tag) { // first access in the set must miss(cache -> history[i] must be 0) and if cache hit, both tags must same
			cache -> history[i] = cache -> cacheAccessNo;
			result = result | HIT;
		}
		
		// get the oldest set from cache(the one with lowest history value)
		if (cache -> history[i] < tempHistory) {
			tempHistory = cache -> history[i];
			temp = i;
		}
	}

	if ((result & HIT) == 0) {
		result = result | MISS; // if not hit confirm miss
	
	       	// other than first access, eviction occurs in cache
		if (cache -> history[temp] > 0) {
			result = result | EVICTION;
		}
		cache -> tag[temp] = tag;
		cache -> history[temp] = cache -> cacheAccessNo;
	}

	// update hit, miss, eviction status
	if (result & HIT) {cache -> hit++;}
	if (result & MISS) {cache -> miss++;}
	if (result & EVICTION) {cache -> eviction++;}

	// if user demands for extra information via -v extra command
	if (cache -> v) {
		if (result & HIT) {printf("hit ");}
		if (result & MISS) {printf("miss ");}
		if (result & EVICTION) {printf("eviction ");}
	}
}

// to free all the virtual cache memory upon task completion
void freeCache(Cache * cache)
{
	free(cache -> tag);
	free(cache -> history);
	free(cache);
}

int main(int argc, char **argv)
{
	int v, s, E, b;
	char * fileName;
    
	int opt;
	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        	       switch (opt) {
        		         case 'h':
				printHelpMessage(argv[0]);
		   		return 0;
			case 'v':
				v = 1;
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				fileName = optarg;
				break;
                           default:
                                    break;
        	       }
	}

	Cache * cache = initCache(v, s, E, b);
	FILE * file = fopen(fileName, "r");
	char instruction[32];

	while (fgets(instruction, 32, file) != NULL) {
		if (instruction[0] != ' ') {continue;} // if not space means it is an instruction. Skips it
		char i;
		long address;
		long size;
		
		sscanf(instruction, " %c %lx, %lx", &i, &address, &size);

		if (v) {printf("%c %lx, %lx ", i, address, size);}
		performCaching(cache, address, size);
		if (i == 'M') {performCaching(cache, address, size);}
		printf("\n");
	}

	freeCache(cache);
   	printSummary(cache -> hit, cache -> miss, cache -> eviction);
   	return 0;
}
