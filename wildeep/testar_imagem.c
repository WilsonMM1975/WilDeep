#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TAMANHO_FRAGMENTO 1400

typedef struct {
    unsigned char r, g, b;
} CorRGB;

typedef struct {
    CorRGB mapeamento_byte_para_cor[256];
    unsigned char chave_mestra[32];
} ChaveCores;

void gerar_chave_cores(ChaveCores *chave) {
    for (int i = 0; i < 256; i++) {
        chave->mapeamento_byte_para_cor[i].r = i;
        chave->mapeamento_byte_para_cor[i].g = (i * 7) % 256;
        chave->mapeamento_byte_para_cor[i].b = (i * 13) % 256;
    }
}

unsigned char* dados_para_imagem(const unsigned char *dados, int tamanho, ChaveCores *chave, int *largura, int *altura) {
    int num_pixels = (tamanho + 2) / 3;
    *largura = (int)sqrt(num_pixels) + 1;
    *altura = (num_pixels + *largura - 1) / *largura;
    
    unsigned char *imagem = malloc(3 * (*largura) * (*altura));
    memset(imagem, 0, 3 * (*largura) * (*altura));
    
    int pixel_idx = 0;
    for (int i = 0; i < tamanho; i += 3) {
        int x = pixel_idx % (*largura);
        int y = pixel_idx / (*largura);
        int idx = (y * (*largura) + x) * 3;
        
        imagem[idx] = dados[i];
        imagem[idx+1] = (i+1 < tamanho) ? dados[i+1] : 0;
        imagem[idx+2] = (i+2 < tamanho) ? dados[i+2] : 0;
        pixel_idx++;
    }
    return imagem;
}

void salvar_imagem_ppm(const unsigned char *imagem, int largura, int altura, const char *nome) {
    FILE *f = fopen(nome, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", largura, altura);
    fwrite(imagem, 3, largura * altura, f);
    fclose(f);
    printf("[CORES] Imagem salva: %s (%dx%d)\n", nome, largura, altura);
}

int main() {
    // Mensagem de exemplo
    const char *mensagem = "HELLO WILDEEP! Esta e uma mensagem secreta.";
    int tamanho = strlen(mensagem) + 1;
    
    printf("📝 Mensagem original: %s\n", mensagem);
    printf("📊 Tamanho: %d bytes\n", tamanho);
    
    ChaveCores chave;
    gerar_chave_cores(&chave);
    
    int largura, altura;
    unsigned char *imagem = dados_para_imagem((unsigned char*)mensagem, tamanho, &chave, &largura, &altura);
    
    salvar_imagem_ppm(imagem, largura, altura, "dados/saida/imagem_wildeep.ppm");
    
    printf("✅ Imagem criada! Abra com: eog dados/saida/imagem_wildeep.ppm\n");
    
    free(imagem);
    return 0;
}
