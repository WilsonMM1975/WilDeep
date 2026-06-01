#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE *f = fopen("dados/saida/imagem_wildeep.ppm", "rb");
    if (!f) {
        printf("Erro ao abrir imagem\n");
        return 1;
    }
    
    char formato[10];
    int largura, altura, maxval;
    fscanf(f, "%s\n%d %d\n%d\n", formato, &largura, &altura, &maxval);
    
    unsigned char *dados = malloc(3 * largura * altura);
    fread(dados, 1, 3 * largura * altura, f);
    fclose(f);
    
    printf("📥 Mensagem extraída da imagem:\n");
    for (int i = 0; i < 3 * largura * altura && dados[i] >= 32 && dados[i] <= 126; i++) {
        printf("%c", dados[i]);
    }
    printf("\n");
    
    free(dados);
    return 0;
}
