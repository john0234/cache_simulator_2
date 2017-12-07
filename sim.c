#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

//Ease of use for keeping track of hits and misses (search cache).
//Do we need to keep track?
#define HIT 1
#define MISS 0

typedef struct block_Type{
    int valid; //Lets us know if current block is Valid.
    int dirty; //Lets us know if current block is Dirty.
    int tag; //This is the tag of our current block(so we know where it belongs in our cache).
    int block_size_in_words; //This is how many words are in each of our blocks.
	int blockOffset;
	int setOffset;
    int addresses[]; //This is the addresses that are in each block.
}block_Type;

typedef struct set_Type{
    int set_size_in_blocks; //How many blocks are in our set. 
    // (TODO: Why is this 256?) We need to put a number in or else we get an error at least on CLion we do.
    int lru; //Holds the integer value of which block in the set is LRU.
    //clock_t times[]; //This will hold clock times for each block. Loop through to find LRU...
	block_Type block[256]; //Holds the blocks in out set.
	int lru_queue[];
}set_Type;

typedef struct cache_Type{
    int numSets; //Number of sets in our cache;
    int assoc; //The associativity of the cache. 1-blksize?
    int blkSize; //This is number of words in a block;
    set_Type cacheArray[]; //Array of sets that hold each block;
}cache_Type;

typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;

void run(stateType* state, cache_Type* cache);
double logbase (double y, int b);
int lineIdx (int addrs, cache_Type* cache, stateType* state);
int wayInx(int addrs, cache_Type* cache);
void writeToMemory(int aluResult, cache_Type* cache, stateType* state);
void memToCache(cache_Type* cache, stateType* state, int aluResult);
void findLRU(set_Type* set);
int find_mem_start(int aluResult, cache_Type* cache);
void cachToMemory(int aluResult, cache_Type* cache, stateType* state, block_Type* block);
int findInvalidBlock(set_Type* set);
int getSetOffset(int aluResult, cache_Type* cache);
int getBlockOffset(int aluResult, cache_Type* cache);
void updateLRU(set_Type* set, int used_block);

//all of our bitwise functions
int getTag(int aluResult, cache_Type* cache);
int getBlockOffsetBits(cache_Type* cache);
int getSetOffsetBits(cache_Type* cache);
int getTagBits(cache_Type* cache);

//TODO possibly write our own system clock function...

/*
* Log the specifics of each cache action.
*
* address is the starting word address of the range of data being transferred.
* size is the size of the range of data being transferred.
* type specifies the source and destination of the data being transferred.
*
* cacheToProcessor: reading data from the cache to the processor
* processorToCache: writing data from the processor to the cache
* memoryToCache: reading data from the memory to the cache
* cacheToMemory: evicting cache data by writing it to the memory
* cacheToNowhere: evicting cache data by throwing it away
*/

//TODO Figure out why we need this?
enum actionType{cacheToProcessor, processorToCache, memoryToCache, cacheToMemory,
    cacheToNowhere};


int field0(int instruction){
    return( (instruction>>19) & 0x7);
}

int field1(int instruction){
    return( (instruction>>16) & 0x7);
}

int field2(int instruction){
    return(instruction & 0xFFFF);
}

int opcode(int instruction){
    return(instruction>>22);
}

void printAction(int address, int size, enum actionType type)
{
    printf("transferring word [%i-%i] ", address, address + size - 1);
    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    } else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    } else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    } else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    } else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}

int signExtend(int num){
    // convert a 16-bit number into a 32-bit integer
    if (num & (1<<15) ) {
        num -= (1<<16);
    }
    return num;
}

void printState(stateType *statePtr)
{
	int i;
	printf("\n@@@\nstate:\n");
	printf("\tpc %d\n", statePtr->pc);
	printf("\tmemory:\n");
	for(i = 0; i < statePtr->numMemory; i++)
	{
		printf("\t\tmem[%d]=%d\n", i, statePtr->mem[i]);
	}
	printf("\tregisters:\n");
	for(i = 0; i < NUMREGS; i++)
	{
		printf("\t\treg[%d]=%d\n", i, statePtr->reg[i]);
	}
	printf("end state\n");
}

double logbase (double y, int b)
{
    double lg;
    lg = log10(y)/log10(b);
    return ceil((lg)); //returns us the ceiling of our logs
}

//this returns the number of bits we will need for the tag
int getTag(int aluResult, cache_Type* cache){
	int otherBits = 32 - getTagBits(cache);
	return aluResult >> otherBits;
}

int getTagBits(cache_Type* cache){
	return 32 - (getBlockOffsetBits(cache) + getSetOffsetBits(cache));
}

//this returns the number of bits we need for the block offset
int getBlockOffsetBits(cache_Type* cache){
	return(int)logbase(cache->blkSize, 2); //tells us how many bits we need for the blockOffset
}

//this returns the number of bits we will need for the set offset.
int getSetOffsetBits(cache_Type* cache){
	return (int)logbase(cache->numSets,2); //tells us how many bits we need for the setOffset
}

int getSetOffset(int aluResult, cache_Type* cache)
{
	int mask = 0;
	for(int i=0; i<getSetOffsetBits(cache); i++)
	{
		mask|= 1 << i;
	}
	mask = mask << getBlockOffsetBits(cache);
	return signExtend(aluResult&mask);
}

int getBlockOffset(int aluResult, cache_Type* cache)
{
	int mask =0;
	for(int i=0; i<getSetOffsetBits(cache); i++)
	{
		mask|= 1 << i;
	}
	return signExtend(aluResult&mask);
}

void searchCache(cache_Type* cache, int aluResult, stateType* state)
{
	int found = 0;
	printf("In search cache\n");
	/*
		TODO: figure out this tag shit.
		call the invalid tag function
	*/

   // int blkNum = getBlockOffset(aluResult, cache);
    int setNum = getSetOffset(aluResult, cache);
    set_Type* set = &(cache->cacheArray[setNum]);
    for(int i=0; i< set->set_size_in_blocks; i++)
    {
        block_Type* block = &set->block[i];
        if(block->valid == 1 && block->tag == getTag(aluResult, cache))
        {
        //TODO change the tag function above
        //exit out of loop a hit
        //TODO check dirty bit
            printAction(aluResult, cache->blkSize, cacheToNowhere);
            i=set->set_size_in_blocks;
			//set->times[i] = clock();
			updateLRU(set,i);
			found = 1;
			break;
        }
	
		//free(block);
    }
	if(found != 1){
		printAction(aluResult, cache->blkSize, memoryToCache);
		printf("Befor mem to cache in non-dirty search cache\n");
		memToCache(cache, state, aluResult); //move it to cache then update times.
		int blockOffset = getBlockOffset(aluResult,cache);
		//set->times[i] = clock(); //this should update the times when something is put into the cache.
		updateLRU(set, blockOffset);
	}
	
  //free(set);
}

int find_mem_start(int aluResult, cache_Type* cache){

    return  aluResult - (aluResult%(cache->blkSize));
}

void memToCache(cache_Type* cache, stateType* state, int aluResult)
{
	printf("In memtoCache\n");

	//int blk = getBlockOffset(aluResult, cache); //blk is the SET that our memory should be in.
	int setOffset = getSetOffset(aluResult, cache); //get set offset
	set_Type* set = &cache->cacheArray[setOffset];//this grabs the SET that our memory should be in.
	//findLRU(set); //sets the LRU to the actual LRU
	//updateLRU(set,blk);
	
	printf("After findLRU\n");

	//********* This checks for any invalid cache blocks before using the LRU ***************
	if(findInvalidBlock(set) != -1){
		set->lru = findInvalidBlock(set);
	}
	printf("After check valid\n");

	int LRU = set->lru;
    block_Type* oldBlock = &set->block[LRU]; //Initialize block types that we will use. newBlock will overwrite oldBlock
    block_Type newBlock; //this is the block that is LRU (one we will replace).
    int mem_start_location = find_mem_start(aluResult,cache); //this is the start of the block in memory.
	printf("Before dirty bit check\n");
    if(oldBlock->dirty == 1) //check if block is dirty AND LRU (then we writeback)
    {
	
        cachToMemory(aluResult, cache, state, oldBlock);
        for(int i=0; i<cache->blkSize; i++)
        {
            newBlock.addresses[i] = state->mem[mem_start_location];
            mem_start_location++;
        }
        newBlock.valid = 1;
        newBlock.tag = getTag(aluResult, cache);
        newBlock.dirty = 0;
		newBlock.setOffset = getSetOffset(aluResult, cache);
		newBlock.blockOffset = getBlockOffset(aluResult, cache);
		//set->block[LRU] = newBlock;  ********This might have been our issue*************
        set->block[LRU].valid = newBlock.valid;
		set->block[LRU].tag = newBlock.tag;
		set->block[LRU].dirty = newBlock.dirty;
		set->block[LRU].setOffset = newBlock.setOffset;
		set->block[LRU].blockOffset = newBlock.blockOffset;
		
		//set->times[LRU] = clock();
		updateLRU(set,newBlock.blockOffset); //using our new priority queue
    }
    else
    {	
	printf("In the else\n");
        for(int i=0; i<cache->blkSize; i++)
        {
            newBlock.addresses[i] = state->mem[mem_start_location];
            mem_start_location++;
        }
		printf("After for loop\n");
        newBlock.valid =1;
        newBlock.tag = getTag(aluResult, cache);
        newBlock.dirty = 0;
		printf("Before get setoffset\n");
		newBlock.setOffset = getSetOffset(aluResult, cache);
		newBlock.blockOffset = getBlockOffset(aluResult, cache);
		printf("After get block offset\n");
        //set->block[LRU] = newBlock; ********This might have been our issue*************
		printf("Before set LRU clock\n");
		//set->times[LRU] = clock();
		set->block[LRU].valid = newBlock.valid;
		set->block[LRU].tag = newBlock.tag;
		set->block[LRU].dirty = newBlock.dirty;
		set->block[LRU].setOffset = newBlock.setOffset;
		set->block[LRU].blockOffset = newBlock.blockOffset;
		updateLRU(set,newBlock.blockOffset);
		printf("After set LRU\n");
    }
   printf("At the end of MemtoCache\n");
}

void cachToMemory(int aluResult, cache_Type* cache, stateType* state, block_Type* block)
{
	printf("in cache to memory");
    //printAction(aluResult, cache->blkSize, cacheToMemory);
    /*TODO:

        ***find what memory block it belongs in...
        ***After that loop through the STARTING memory location of the block and loop through replacing the block memory with what in the cache.
    */
    int j = find_mem_start(aluResult, cache);
    for(int i = 0; i < cache->blkSize; i++){

        state->mem[j] = block->addresses[i];
        j++;
    }
}

void regToCache(stateType* state, cache_Type* cache,int regA, int aluResult){
	printf("In reg to cache");
	searchCache(cache, aluResult, state);
    int setOffset = getSetOffset(aluResult, cache); //blk is the SET that our memory should be in.
    set_Type* set = &cache->cacheArray[setOffset]; //this grabs the SET that our memory should be in.
	int blockOffset = getBlockOffset(aluResult, cache);
	
    block_Type* oldBlock = &set->block[blockOffset];; //Initialize block types that we will use. newBlock will overwrite oldBlock
    int mem_line = aluResult%cache->blkSize; //this is the start of the block in memory.
	oldBlock->addresses[mem_line] = regA;
	oldBlock->dirty = 1;
	//set->times[blockOffset] = clock();
	updateLRU(set,blockOffset);

	/*
		TODO:
			  1.Search cache for memory block we want to store the Reg value in.
			  2.If hit, store reg value to cache and change dirty bit to 1.
			  3.If miss, go grab the block from memory and store the reg value to cache.
			  4.Update times(LRU)?
	*/
}

void run(stateType *state, cache_Type *cache) {
	printf("In run\n");
    // Reused variables;
    int instr = 0;
    int regA = 0;
    int regB = 0;
    int offset = 0;
    int branchTarget = 0;
    int aluResult = 0;

    int total_instrs = 0;
    // Primary loop
    while(1){
        total_instrs++;
	printf("In while loop\n");

        //printState(state);

        // Instruction Fetch
        instr = state->mem[state->pc];

        /*searches the cache for the instruction*/
        //searchCache(cache->size, cache->assoc, instr ,cacheArray[cache->blkSize][cache->assoc]);

        /* check for halt */
        if (opcode(instr) == HALT) {
            printf("machine halted\n");
            break;
        }

        // Increment the PC
        state->pc = state->pc+1;

        // Set reg A and B
	printf("This is regB before assign %d\n", regB);
	printf("This is field 1 instruction %d\n", field1(instr));
        regA = state->reg[field0(instr)];
        regB = state->reg[field1(instr)];
	printf("%d\n", regB);
        // Set sign extended offset
        offset = signExtend(field2(instr));

        // Branch target gets set regardless of instruction
        branchTarget = state->pc + offset;

        // ADD
        if(opcode(instr) == ADD){
			/*
				********* cache isnt used for add **********
			*/

            // Add
            aluResult = regA + regB;
            // Save result
            state->reg[field2(instr)] = aluResult;
        }
            // NAND
        else if(opcode(instr) == NAND){
			/*
				********* cache isnt used for add **********
			*/
            // NAND
            aluResult = ~(regA & regB);
            // Save result
            state->reg[field2(instr)] = aluResult;
        }
            // LW or SW
        else if(opcode(instr) == LW || opcode(instr) == SW){



            aluResult = regB + offset; //Calculate memory address.

            searchCache(cache, aluResult, state);

            if(opcode(instr) == LW){

				//Put value we are loading into cache then into register from cache.

         	       // Load
				printf("This is RegB: %d\n", regB);
				printf("This is the offset %d\n", offset);
				printf("This is aluResult %d \n", aluResult);
                		printState(state);
				printf("In LW before Mem to cache\n");
				searchCache(cache, aluResult, state);
				printf("After mem to cache in LW\n");
				int setOffset = getSetOffset(aluResult, cache);
				int blockOffset = getBlockOffset(aluResult, cache);
				int mem_line = aluResult%cache->blkSize;
				set_Type* set = &cache->cacheArray[setOffset];
				block_Type* block = &set->block[blockOffset];
				int addrs = block->addresses[mem_line];
				state->reg[regA] = addrs;
            }else if(opcode(instr) == SW){
                //Store

                //SW should write to cache and not worry about writing to memory! (set dirty bit to 1 if it writes to cache).
                //writeToCache();


                regToCache(state,cache, regA, aluResult);
            }
        }
            // JALR
        else if(opcode(instr) == JALR){
            // Save pc+1 in regA
            state->reg[field0(instr)] = state->pc;
            //Jump to the address in regB;
            state->pc = state->reg[field1(instr)];
        }
            // BEQ
        else if(opcode(instr) == BEQ){
            // Calculate condition
            aluResult = (regA == regB);

            // ZD
            if(aluResult){
                // branch
                state->pc = branchTarget;
            }
        }
    } // While
}

int findInvalidBlock(set_Type* set){
	int ret = -1;

	for(int i = 0; i < set->set_size_in_blocks; i++){
		if(set->block[i].valid == 0){
			//block is invalid
			return i;
		}
	}
	return ret;
}

/*
void findLRU(set_Type* set){

    clock_t holder = set->times[0];
    int lru = 0;

    for(int i = 1; i < set->set_size_in_blocks; i++){
        if(((double)(set->times[i] - holder)) < 0){

            holder = set->times[i];
            lru = i;
        }
    }
    set->lru = lru;
}
*/

void updateLRU(set_Type* set, int used_block){
	
	int newArr[set->set_size_in_blocks];
	newArr[0] = used_block;
	int j = 1;
	for(int i = 0; i < set->set_size_in_blocks; i ++){
		
		if(set->lru_queue[i] != used_block){
			newArr[j] = set->lru_queue[i];
		}
		j++;
	}
	
	for(int i = 0; i < set->set_size_in_blocks; i++){
		
		set->lru_queue[i] = newArr[i];
	}
	
	set->lru = set->lru_queue[set->set_size_in_blocks - 1];
	
}

int main(int argc, char** argv){

    /** Get command line arguments **/

    char* fname = (char*)malloc(sizeof(char)*100);;
    FILE* fp = (FILE*)malloc(sizeof(FILE));
   // FILE *fp;
    cache_Type* cache = (cache_Type*)malloc(sizeof(cache_Type));


    //TODO error check the input
    if(argc == 5){
		int strsize = strlen(argv[1]);      

        fname[0] = '\0';

        strcat(fname, argv[1]);
		fp = fopen(fname, "r");
		 if (fp == NULL) {
         printf("Cannot open file '%s' : %s\n", fname, strerror(errno));
         return -1;
     }

        cache->blkSize =(int)argv[2];
        cache->numSets =(int)argv[3];
        cache->assoc =(int)argv[4];

    }
    else{
        //TODO error check the input

        //Takes in the parameters if they are not in the commnad line
        

        printf("\nEnter the machine code program to simulate: ");
        fgets(fname, 100, stdin);
        fname[strlen(fname)-1] = '\0';
        fp = fopen(fname, "r");
        while(1)
        {
        	if(fp == NULL)
        	{
          	  printf("The file you entered does not exist. Please enter another one: ");
         	  fgets(fname, 100, stdin);
         	  fname[strlen(fname)-1] = '\0';
          	  fp = fopen(fname, "r");
        	}
        	else {
        		break;
        	}
        }

        printf("\nEnter the block size of the cache (in words): ");
        scanf("%d", &cache->blkSize);
        while(cache->blkSize > 256 || cache->blkSize <1)
        {
            printf("\nThe block size you entered is not within the parameters (1-256). Please enter again: ");
            scanf("%d",&cache->blkSize);
        }

        printf("\nEnter the number of sets in the cache (1 or greater): ");
        scanf("%d", &cache->numSets);
        while(cache->numSets < 1)
        {
            printf("\nThe number you entered is not in the range (1 or greater). Please enter again: ");
            scanf("%d", &cache->numSets);
        }

        printf("\nEnter the associativity of the cache (1 or greater): ");
        scanf("%d", &cache->assoc);
        while(cache->assoc < 1)
        {
            printf("\nThe number you entered is not in the range (1 or greater). Please enter again: ");
            scanf("%d", &cache->assoc);
        }
		printf("end of else\n");
    }//else if
    //Take in a file
   // fname[strlen(fname)-1] = '\0';// gobble up the \n with a \0

    //FILE *fp = fopen(fname, "r");

    /* if (fp == NULL) {
         printf("Cannot open file '%s' : %s\n", fname, strerror(errno));
         return -1;
     }*/

    /* count the number of lines by counting newline characters */
    int line_count = 0;
    int c;
    printf("before file detect\n");
    /*while (EOF != (c=getc(fp))) {
        if ( c == '\n' ){
            line_count++;
        }
    }*/
    // reset fp to the beginning of the file
	printf("after mem & reg set");
    rewind(fp);
    printf("Block size %d", cache->blkSize);


    stateType* state = (stateType*)malloc(sizeof(stateType));

    state->pc = 0;
    memset(state->mem, 0, NUMMEMORY*sizeof(int));
    memset(state->reg, 0, NUMREGS*sizeof(int));
	printState(state);
  

    char line[256];
	printf("before inputting mem\n");
    int i = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* note that fgets doesn't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        state->mem[i] = atoi(line);
        i++;
		line_count++;
    }
    fclose(fp);
	state->numMemory = line_count;
    cache->cacheArray[cache->numSets]; //Initializes how many Sets there will be.

    for(int i = 0; i < cache->numSets; i++){

	set_Type* set = &cache->cacheArray[i];
        set->lru = 0;
        /*
            TODO:**I THINK ITS SOLVED**

             ***We need to figure out how many blocks should be in our set here*** (instead of setting it to 0).
        */
        set->set_size_in_blocks = cache->assoc;
        set->block[set->set_size_in_blocks];
        //set->times[set->set_size_in_blocks];
		set->lru_queue[set->set_size_in_blocks];

        for(int j = 0; j < set->set_size_in_blocks; j++){
            block_Type* block = &set->block[j];
            block->valid = 0;
            block->dirty = 0;
            block->tag = 0;
            block->block_size_in_words = cache->blkSize; //initiallizes blocksize(block) to blocksize(cache).

            //set->times[j] = clock(); //Sets each time to clock time (just so its set to something);
			set->lru_queue[j] = 0;

            for(int k = 0; k < block->block_size_in_words; k++){
                block->addresses[k] = 0; //initiallizes all of the addresses to 0 to begin with.
            }

        }

    }
    printf("before run\n");
    run(state, cache);
    
    free(state);
    free(fname);

}


