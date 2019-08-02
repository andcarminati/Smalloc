#include "allocation.h" 
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(){
	printf("Teste de alocacao:\n\n");
	
	printf("Estado inicial do alocador:\n\n");
	dump();

	printf("Alocando um Array (1) de 100 bytes:\n\n");
	char *array1;
	array1 = Alloc(100);
	dump();


	printf("\n\nAlocando um Array (2) de 2000 bytes:\n\n");
	char *array2;
	array2 = Alloc(2000);
	dump();

	printf("\n\nAlocando um Array (3) de 105 bytes:\n\n");
	char *array3;
	array3 = Alloc(105);
	dump();

	printf("\n\nAlocando um array (4) de 600 bytes:\n\n");
	char *array4;
	array4 = Alloc(600);
	dump();

	printf("\n\nAlocando um Array (5) de 71 bytes:\n\n");
	char *array5;
	array5 = Alloc(71);
	dump();
	
        printf("\n\nDesalocando array (3)\n\n");
	Free(array3);
	dump();
	
        printf("\n\nDesalocando array (5)\n\n");
	Free(array5);
	dump();
	
	printf("\n\nDesalocando array (1)\n\n");
	Free(array1);
	dump();
	
	printf("\n\nAlocando um Array (6) de 50 bytes:\n\n");
	char *array6;
	array6 = Alloc(50);
	dump();

	printf("\n\nDesalocando array (2)\n\n");
	Free(array2);
	dump();


	printf("\n\nDesalocando array (4)\n\n");
	Free(array4);
	dump();
	
	printf("\n\nDesalocando array (6)\n\n");
	Free(array6);
	dump();


}
