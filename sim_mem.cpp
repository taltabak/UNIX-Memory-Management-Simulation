#include "sim_mem.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   
#include <unistd.h>
#include <fcntl.h>

sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size,  int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size)
{   
    //initial all data structures for the program.
    this->program_fd = open(exe_file_name, O_RDONLY,0);
    if(this->program_fd == -1)
    {
        perror("OPEN EXE FILE FAILED"); 
        exit(1);
    }    
    this->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT ,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(this->swapfile_fd == -1)
    {
        perror("OPEN SWAP FILE FAILED"); 
        exit(1);
    }
    this->text_size = text_size;
    this->data_size = data_size;
    this->bss_size = bss_size;
    this->heap_stack_size = heap_stack_size;
    this->num_of_pages = num_of_pages;
    this->page_size = page_size;
    this->frames = new int[MEMORY_SIZE/page_size];
    for(int i;i<(MEMORY_SIZE/page_size);i++)
        frames[i] = 0;
    this->frames_queue = new int[MEMORY_SIZE/page_size];
    for(int i;i<(MEMORY_SIZE/page_size);i++)
        frames_queue[i] = -1;    

    //fill the main memory in zeros
    for(int i=0;i<MEMORY_SIZE;i++)
        main_memory[i] = '0';

    //create the page table and initial all values,  
    page_table = new page_descriptor[num_of_pages];
    for(int i = 0; i < num_of_pages; i++)
    {
        page_table[i].D = 0;
        page_table[i].V = 0;
        
        if(i < text_size/page_size)
        { 
            page_table[i].P = 0;
            page_table[i].frame = i;
        }    
        else
        {
            page_table[i].P = 1;
            page_table[i].frame = -1;
        }    
    }

    //string to fill the swap in zeros
    char s[page_size*num_of_pages];
    for(int i = 0;i<page_size*num_of_pages;i++)    
        s[i] = '0';

    //fille the swap file in zeros
    if(write(this->swapfile_fd, s, page_size*num_of_pages) == -1)
    {
        perror("WRITING TO SWAP FAILED"); 
        exit(1);
    }
}

char sim_mem::load( int address)
{
    int page = address/page_size ;
	int offset = address%page_size;
    if(page_table[page].V == 1) //page in memory
    {
        int frame = page_table[page].frame;
        return(main_memory[frame*page_size+offset]);
    }
    if(page_table[page].P == 0)
    {
        int frame = check_for_frame();
        if(frame == -1)
            frame = swap_out(page);
        lseek(program_fd, page*page_size,SEEK_SET);
        read(program_fd,&main_memory[frame*page_size],page_size);
        page_table[page].V = 1;
        page_table[page].frame = frame;
        push(frame);
        frames[frame] = 1;
        return(main_memory[frame*page_size+offset]);  
    }
    if(page_table[page].D == 1)
    {
        int frame = check_for_frame();
        if(frame == -1)
            frame = swap_out(page);
        lseek(swapfile_fd, page*page_size,SEEK_SET);
        read(swapfile_fd,&main_memory[frame*page_size],page_size);
        page_table[page].V = 1;
        page_table[page].frame = frame;
        push(frame);
        frames[frame] = 1;
        return(main_memory[frame*page_size+offset]);  
    }
    else
    {
        if(address < text_size+data_size) //case data
        {
            int frame = check_for_frame();
            if(frame == -1)
                frame = swap_out(page);  
            lseek(program_fd, page*page_size,SEEK_SET);
            read(program_fd,&main_memory[frame*page_size],page_size);
            page_table[page].V = 1;
            page_table[page].frame = frame;
            push(frame);
            frames[frame] = 1;
            return(main_memory[frame*page_size+offset]);    
        }
        else if(address < text_size+data_size+bss_size) //case BSS
        {
            int frame = check_for_frame();
            if(frame == -1)
                frame = swap_out(page);
            page_table[page].V = 1;
            page_table[page].frame = frame;
            for(int i =0; i<page_size;i++)
                main_memory[(frame*page_size)+i] = '0';
            return(main_memory[frame*page_size+offset]);                 
        }
        else // case heap or stack and V=0 , D=0.
        {
            perror("CANNOT LOAD FROM HEAP OR STACK"); 
            exit(1);    
        }        
    } 
}

void sim_mem::store(int address, char value)
{
    int page = address/page_size ;
	int offset = address%page_size;
    if(page_table[page].P == 0)
    {
        perror("READ ONLY PERMISSION"); 
        exit(1);         
    }
    if(page_table[page].V == 1) //page in memory
    {
        int frame = page_table[page].frame;
        main_memory[frame*page_size+offset] = value;
        page_table[page].D = 1;
        return;
    }
    if(page_table[page].D == 1)
    {
        int frame = check_for_frame();
        if(frame == -1)
            frame = swap_out(page);
        lseek(swapfile_fd, page*page_size,SEEK_SET);
        read(swapfile_fd,&main_memory[frame*page_size],page_size);
        main_memory[frame*page_size+offset] = value;
        page_table[page].frame = frame;
        page_table[page].V = 1;
        push(frame);
        frames[frame] = 1;
        return;
    }
    else
    {
        if(address < text_size+data_size) //case data
        {
            int frame = check_for_frame();
            if(frame == -1)
                frame = swap_out(page);  
            lseek(program_fd, page*page_size,SEEK_SET);
            read(program_fd,&main_memory[frame*page_size],page_size);
            page_table[page].V = 1;
            page_table[page].D = 1;
            page_table[page].frame = frame;
            push(frame);
            frames[frame] = 1;
            main_memory[frame*page_size+offset] = value;
            return;    
        }
        else
        {
            int frame = check_for_frame();
            if(frame == -1)
                frame = swap_out(page);
            page_table[page].V = 1;
            page_table[page].D = 1;
            page_table[page].frame = frame;
            push(frame);
            frames[frame] = 1;
            for(int i =0; i<page_size;i++)
                main_memory[(frame*page_size)+i] = '0';
            return;    
        }  
    }
}

int sim_mem::check_for_frame()
{
    for(int i=0 ;i<(MEMORY_SIZE/page_size);i++)
        if(frames[i] == 0)
            return i;
    return -1;
} 

int sim_mem::swap_out(int page)
{
    int frame = pop();
    frames[frame] = 0;
    lseek(swapfile_fd, page*page_size,SEEK_SET);
    write(swapfile_fd,&main_memory[frame*page_size],page_size);
    page_table[page].V = 0;
    page_table[page].frame = -1;
    return frame;
}

int sim_mem::pop()
{
    int temp = frames_queue[0];
    for(int i = (MEMORY_SIZE/page_size)-1 ; i > 0 ; i-- )
        frames_queue[i-1] = frames_queue[i];
    frames_queue[(MEMORY_SIZE/page_size)-1] = -1;
    return temp;
}

int sim_mem::push(int frame)
{
    for(int i;i<(MEMORY_SIZE/page_size);i++)
        if(frames_queue[i] == -1)
            frames_queue[i] = frame; 
}

void sim_mem::print_memory() {
	int i;
	printf("\n Physical memory\n");
	for(i = 0; i < MEMORY_SIZE; i++) {
		printf("[%c]\n", main_memory[i]);
	}
}

void sim_mem::print_swap() {
	char* str = (char*)malloc(this->page_size *sizeof(char));
	int i;
	printf("\n Swap memory\n");
	lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
	while(read(swapfile_fd, str, this->page_size) == this->page_size) {
		for(i = 0; i < page_size; i++) {
			printf("%d - [%c]\t", i, str[i]);
		}
		
		printf("\n");
	}
}

void sim_mem::print_page_table() { 
	int i;
	printf("\n page table \n");
	printf("Valid\t Dirty\t Permission \t Frame\n");
	for(i = 0; i < num_of_pages; i++) {
		printf("[%d]\t[%d]\t[%d]\t[%d]\n", 
                        page_table[i].V,
                        page_table[i].D, 
				page_table[i].P, 
                        page_table[i].frame);
	}
}

sim_mem::~sim_mem()
{
    if(close(swapfile_fd) == -1 || close(program_fd) == -1 )
    {
        perror("CLOSE FD FAILED"); 
        exit(1);
    }
    delete [] frames;
    delete [] page_table;
}

