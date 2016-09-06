// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
#include <io.h>
#include <errno.h>

char * fname = "c:\\temp\\xmas.mid";

void readBinaryFile(char *fileName)
{
	FILE *fp;
	char *ptr;
	double *ptr1;
	int *ptr2;
	ptr = (char*)malloc(sizeof(char) * 5);
	ptr1 = (double*)malloc(sizeof(double) * 6);
	ptr2 = (int*)malloc(sizeof(int) * 6);

	fp = fopen(fname, "rb");
	fread(ptr, sizeof(char), 11, fp);
	while (*ptr != '\0')
	{
		printf("%c", *ptr);
		ptr++;

	}
	fclose(fp);
}


void test2()
{

}

int test1()
{

	freopen(fname, "rb", stdin);
	int c = 0;
	int i = 0;
	while (c!=EOF)
	{
		c = getchar();
		printf("%d pos:%d\n", c, i++);
	}

    return 0;
}

int main()
{

	test1();


}