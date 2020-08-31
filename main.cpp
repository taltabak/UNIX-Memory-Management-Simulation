#include <stdio.h>
#include "sim_mem.hh"
#include <string.h>

int main()
{
    sim_mem mem((char*)"exec_file",(char*)"swap_file",25,50,25,25,25,5);
    int i,j,n=0;
    char c = 'A';

    for(j=0;j<2;j++)
    {                       //Filling the memory with chars
        for(i=25;i<125;i++)
        {
            mem.store(i,c);
            mem.load(i);
            n++;
            if(n==5)
            {
                n=0;
                c++;
            }
        }
        for(i=0;i<24;i++)
            mem.load(i);
    }


    mem.print_memory();
    printf("\n");
    mem.print_page_table();
    printf("\n");
    mem.print_swap();
    printf("\n\n");
    return 0;
}