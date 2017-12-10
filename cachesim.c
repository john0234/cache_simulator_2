#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include<stdbool.h>

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

//#define NOOPINSTRUCTION 0x1c00000

enum actionType{cacheToProcessor, processorToCache, memoryToCache, cacheToMemory,
    cacheToNowhere};

int find_mem_start(int aluResult);
int getTag(int aluResult);
int getTagBits();
int getBlockOffsetBits();
int getSetOffsetBits();
int getSetOffset(int aluResult);
int getBlockOffset(int aluResult);
void printAction(int address, int size, enum actionType type);
int signExtend(int num);


int blockSize;
int numbrSets;
int associt;

typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;

typedef struct blockType {
    int valid;
    int dirty;
    int tag;
    int addresses[];
} blockType;

typedef struct cacheStruct {
    struct blockType cacheArray[256][256];
} cacheType;


/**************** Main Function Declaration *****************************/
cacheType* memToCache(cacheType* cache, int aluResult, stateType* state);
int searchCache(cacheType* cache, stateType* state, int aluResult);

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

void printInstruction(int instr) {
    char opcodeString[10];
    if (opcode(instr) == ADD) {
        strcpy(opcodeString, "add");
    } else if (opcode(instr) == NAND) {
        strcpy(opcodeString, "nand");
    } else if (opcode(instr) == LW) {
        strcpy(opcodeString, "lw");
    } else if (opcode(instr) == SW) {
        strcpy(opcodeString, "sw");
    } else if (opcode(instr) == BEQ) {
        strcpy(opcodeString, "beq");
    } else if (opcode(instr) == JALR) {
        strcpy(opcodeString, "jalr");
    } else if (opcode(instr) == HALT) {
        strcpy(opcodeString, "halt");
    } else if (opcode(instr) == NOOP) {
        strcpy(opcodeString, "noop");

    } else {
        strcpy(opcodeString, "data");
    }

    if(opcode(instr) == ADD || opcode(instr) == NAND){
        printf("%s %d %d %d\n", opcodeString, field2(instr), field0(instr), field1(instr));
    }else if(0 == strcmp(opcodeString, "data")){
        printf("%s %d\n", opcodeString, signExtend(field2(instr)));
    }else{
        printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
               signExtend(field2(instr)));
    }
}//printInstruction

void printState(stateType *statePtr){
    int i;
    printf("\n@@@\nstate:\n");
    printf("\tpc %d\n", statePtr->pc);
    printf("\tmemory:\n");
    for(i = 0; i < statePtr->numMemory; i++){
        printf("\t\tmem[%d]=%d\n", i, statePtr->mem[i]);
    }
    printf("\tregisters:\n");
    for(i = 0; i < NUMREGS; i++){
        printf("\t\treg[%d]=%d\n", i, statePtr->reg[i]);
    }
    printf("end state\n");
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

void print_stats(int n_instrs){
    printf("INSTRUCTIONS: %d\n", n_instrs);
}

double logbase (double y, int b)
{
    double lg;
    lg = log10(y)/log10(b);
    return ceil((lg)); //returns us the ceiling of our logs
}

int find_mem_start(int aluResult){

    return  aluResult - (aluResult%blockSize);
}

//this returns the number of bits we will need for the tag
int getTag(int aluResult){
    int otherBits = 32 - getTagBits();
    //printf("Tag Results: %d\n", aluResult >> otherBits);
    return aluResult >> otherBits;
}

int getTagBits(){
    return 32 - (getBlockOffsetBits() + getSetOffsetBits());
}

//this returns the number of bits we need for the block offset
int getBlockOffsetBits(){
    return(int)logbase(blockSize, 2); //tells us how many bits we need for the blockOffset
}

//this returns the number of bits we will need for the set offset.
int getSetOffsetBits(){
    return (int)logbase(numbrSets,2); //tells us how many bits we need for the setOffset
}

int getSetOffset(int aluResult)
{
    int mask = 0;
    for(int i=0; i<getSetOffsetBits(); i++)
    {
        mask|= 1 << i;
    }
    mask = mask << getBlockOffsetBits();
    return signExtend(aluResult&mask);
}

int getBlockOffset(int aluResult)
{
    int mask =0;
    for(int i=0; i<getSetOffsetBits(); i++)
    {
        mask|= 1 << i;
    }
    return signExtend(aluResult&mask);
}




void cacheToMem(cacheType* cache, int aluResult, stateType* state)
{
    printAction(find_mem_start(aluResult), blockSize, cacheToMemory);
    //printf("In cache to mem\n");
    int wayNum = searchCache(cache, state, aluResult);
    int setNum = getSetOffset(aluResult);
    int memStart = find_mem_start(aluResult);
    for(int i=0; i<associt; i++)
    {
        state->mem[memStart] = cache->cacheArray[setNum][wayNum].addresses[i];
        memStart++;
    }
}

int cacheToRegs(cacheType* cache, stateType* state, int aluResult, int instr)
{
    printAction(aluResult, 1, cacheToProcessor);
   // printf("In cache to regs\n");
    int wayNum = searchCache(cache, state, aluResult);
    int setNum = getSetOffset(aluResult);
    int lineNum = aluResult%blockSize;
    //printf("Whats in the block: %d\n", cache->cacheArray[setNum][wayNum].addresses[lineNum]);
    return cache->cacheArray[setNum][wayNum].addresses[lineNum];
}

void regsToCache(cacheType* cache, int aluResult, stateType* state, int regA)
{
    printAction(aluResult, 0, processorToCache);
    int wayNum = searchCache(cache, state, aluResult);
   // printf("Way number: %d\n", wayNum);
    int setNum = getSetOffset(aluResult);
    //printf("Set Number: %d\n", setNum);
    int memLine = aluResult%blockSize;
    //printf("Mem line: %d\n", memLine);

    cache->cacheArray[setNum][wayNum].addresses[memLine] = regA;
    cache->cacheArray[setNum][wayNum].dirty = 1;

    //printf("Dirty bit: %d\n", cache->cacheArray[setNum][wayNum].dirty);
}

int searchCache(cacheType* cache, stateType* state, int aluResult)
{
   // printf("In search Cache\n");
    int setNum = getSetOffset(aluResult);
    //printf("Set Number: %d\n", setNum);
    int tagNum = getTag(aluResult);
    //printf("Tag Number: %d\n", tagNum);
    int result = -1;
    blockType block;
    for(int i=0; i<associt; i++)
    {
        block = cache->cacheArray[setNum][i];
        //printf("Tag Num for Block: %d\n", block.tag);
       // printf("Valid bit for Block: %d\n", block.valid);
        if(block.tag == tagNum && block.valid == 1 )
        {
            result = i;
           // printf("IN CACHE\n");
            printAction(find_mem_start(aluResult), blockSize, cacheToNowhere);
        }
    }
    //printf("Before result = -1\n");
   if(result == -1)
   {
       cache = memToCache(cache, aluResult, state);
       result = searchCache(cache, state, aluResult);
       //printf("Result in result = -1: %d\n", result);
   }
    return result;
}

cacheType* memToCache(cacheType* cache, int aluResult, stateType* state)
{

    //printf("In Mem to Cache\n");
    int setNum = getSetOffset(aluResult);
   // printf("Set number: %d \n", setNum);
    int tagNum = getTag(aluResult);
   // printf("Tag number: %d\n", tagNum);
    int wayNum = -1;
    blockType* block;
    //TODO: Make sure to check all the blocks to make sure that it is in cache
    for(int i=associt-1; i >=0; i--)
    {
        block = &cache->cacheArray[setNum][i];
        if(block->valid == 0)
        {
            wayNum = i;
            break;
        }
    }
    int memStart = find_mem_start(aluResult);
    printAction(memStart, blockSize, memoryToCache);
    //printf("Mem Start: %d\n", memStart);
    //block = cacheArray[setNum][associt-1];

    if(wayNum == -1)
    {
        block = &cache->cacheArray[setNum][associt-1];
        if(block->dirty == 1)
        {
            cacheToMem(cache, aluResult, state);
        }
        for(int i=0; i<blockSize; i++)
        {
           // printf("Mem Addrs: %d\n", state->mem[memStart]);
            block->addresses[i] = state->mem[memStart];
           // printf("Block Mem Addrs: %d\n", block->addresses[i]);
            memStart++;
        }
        block->tag = tagNum;
        block->valid=1;
        for(int i=0; i<associt; i++)
        {
            cache->cacheArray[setNum][i+1] = cache->cacheArray[setNum][i];
        }
        //printf("Addrs: %d\n", cache->cacheArray[setNum][0].addresses[0]);
        cache->cacheArray[setNum][0] = *block;
    }
    else {
        block = &cache->cacheArray[setNum][wayNum];
        if(block->dirty == 1)
        {
            cacheToMem(cache, aluResult, state);
        }

        for (int i = 0; i < blockSize; i++) {
            //printf("Mem Address: %d\n", state->mem[memStart]);
            block->addresses[i] = state->mem[memStart];
            memStart++;
        }
        block->tag = tagNum;
        block->valid = 1;

        for (int i = 0; i < wayNum; i++) {
            cache->cacheArray[setNum][i + 1] = cache->cacheArray[setNum][i];
        }
        cache->cacheArray[setNum][0] = *block;
        //printf("Tag for new block: %d\n", cache->cacheArray[setNum][0].tag);
        //printf("Valid bit for new block: %d\n", cache->cacheArray[setNum][0].valid);
        /*for (int i = 0; i < numbrSets; i++) {
            //printf("first foor loop\n");
            for (int j = 0; j < associt; j++) {
                // printf("second for loop\n");
                //printf("Tag at block %d: %d\n", i, cache->cacheArray[i][j].valid);
            }
        }*/
    }
    return cache;
}


void run(stateType* state, cacheType* cache){

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

        //printState(state);


        // Instruction Fetch
        int wayNum = searchCache(cache, state, state->pc);
        int setNum = getSetOffset(state->pc);
        int lineNum = state->pc%blockSize;
        //printf("Line num: %d\n", lineNum);
        /*for(int i =0; i<blockSize; i++)
        {
            printf("Address: %d\n", cache->cacheArray[setNum][wayNum].addresses[i]);
        }*/
        instr = cache->cacheArray[setNum][wayNum].addresses[lineNum];
        //printf("Instruction: %d\n", instr);
        //instr = state->mem[state->pc];

        /* check for halt */
        if (opcode(instr) == HALT) {
            printf("machine halted\n");
            break;
        }

        // Increment the PC
        state->pc = state->pc+1;

        // Set reg A and B
        regA = state->reg[field0(instr)];
        regB = state->reg[field1(instr)];

        // Set sign extended offset
        offset = signExtend(field2(instr));

        // Branch target gets set regardless of instruction
        branchTarget = state->pc + offset;

        /**
         *
         * Action depends on instruction
         *
         **/
        //printInstruction(instr);
        // ADD
        if(opcode(instr) == ADD){
            // Add
            aluResult = regA + regB;
            // Save result
            state->reg[field2(instr)] = aluResult;
        }
            // NAND
        else if(opcode(instr) == NAND){
            // NAND
            aluResult = ~(regA & regB);
            // Save result
            state->reg[field2(instr)] = aluResult;
        }
            // LW or SW
        else if(opcode(instr) == LW || opcode(instr) == SW){
            // Calculate memory address
            aluResult = regB + offset;
            int wayNum = searchCache(cache, state, aluResult);
            if(opcode(instr) == LW){
                // Load

                state->reg[field0(instr)] = cacheToRegs(cache, state, aluResult, instr);
               //cacheToRegs(cache, state, aluResult, instr);
            }else if(opcode(instr) == SW){
                // Store
                //state->mem[aluResult] = regA;

               regsToCache(cache, aluResult, state, regA);
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
    print_stats(total_instrs);
}

int main(int argc, char** argv) {

    /** Get command line arguments **/
    char *fname = (char *) malloc(sizeof(char) * 100);;
    FILE *fp = (FILE *) malloc(sizeof(FILE));

    if (argc == 5) {
        int strsize = strlen(argv[1]);

        fname[0] = '\0';

        strcat(fname, argv[1]);
        fp = fopen(fname, "r");
        if (fp == NULL) {
            printf("Cannot open file '%s' : %s\n", fname, strerror(errno));
            return -1;
        }

        blockSize = atoi(argv[2]);
        numbrSets = atoi(argv[3]);
        associt = atoi(argv[4]);
        // printf("%d, %d, %d\n", blockSize, numbrSets, associt);

    } else {
        //TODO error check the input

        //Takes in the parameters if they are not in the commnad line


        printf("\nEnter the machine code program to simulate: ");
        fgets(fname, 100, stdin);
        fname[strlen(fname) - 1] = '\0';
        fp = fopen(fname, "r");
        while (1) {
            if (fp == NULL) {
                printf("The file you entered does not exist. Please enter another one: ");
                fgets(fname, 100, stdin);
                fname[strlen(fname) - 1] = '\0';
                fp = fopen(fname, "r");
            } else {
                break;
            }
        }

        printf("\nEnter the block size of the cache (in words): ");
        scanf("%d", &blockSize);
        while (blockSize > 256 || blockSize < 1) {
            printf("\nThe block size you entered is not within the parameters (1-256). Please enter again: ");
            scanf("%d", &blockSize);
        }

        printf("\nEnter the number of sets in the cache (1 or greater): ");
        scanf("%d", &numbrSets);
        while (numbrSets < 1) {
            printf("\nThe number you entered is not in the range (1 or greater). Please enter again: ");
            scanf("%d", &numbrSets);
        }

        printf("\nEnter the associativity of the cache (1 or greater): ");
        scanf("%d", &associt);
        while (associt < 1) {
            printf("\nThe number you entered is not in the range (1 or greater). Please enter again: ");
            scanf("%d", &associt);
        }
    }//else if

    /* count the number of lines by counting newline characters */
    int line_count = 0;
    int c;
    while (EOF != (c = getc(fp))) {
        if (c == '\n') {
            line_count++;
        }
    }
    // reset fp to the beginning of the file
    rewind(fp);

    stateType *state = (stateType *) malloc(sizeof(stateType));

    state->pc = 0;
    memset(state->mem, 0, NUMMEMORY * sizeof(int));
    memset(state->reg, 0, NUMREGS * sizeof(int));

    state->numMemory = line_count;

    char line[256];

    int i = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* note that fgets doesn't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        state->mem[i] = atoi(line);
        i++;
    }
    fclose(fp);
    cacheType *cache = (cacheType *) malloc(sizeof(cacheType));
    //printf("After block initialization\n");

    for (int i = 0; i < numbrSets; i++) {
        //printf("first foor loop\n");
        for (int j = 0; j < associt; j++) {
            // printf("second for loop\n");
            //printf("Tag at block %d: %d\n", i, cache->cacheArray[i][j].tag);
            blockType temp = cache->cacheArray[i][j];
            temp.tag = 0;
            temp.dirty = 0;
            temp.valid = 0;
            cache->cacheArray[i][j] = temp;
        }
    }


    /** Run the simulation **/
    //run(state);

    //printf("Way location: %d\n", searchCache(cacheArray, state, 9));
    /*for(int i=0; i<associt; i++)
    {
        printf("Tag: %d\n", cache->cacheArray[i][0].tag);
    }*/
    //memToCache(cache, 9, state);
    //searchCache(cache, state, 9);
  // regsToCache(cache, 9, state, 3);
    //cacheToMem(cache, 9, state);
    run(state, cache);


    free(cache);
    free(state);
    free(fname);

}

