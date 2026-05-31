/**
 * cores.c - Criptografia visual baseada em cores RGB
 */

#include "../../include/wildeep.h"

void gerar_chave_cores(ChaveCores *chave, const unsigned char *semente, int tamanho_semente) {
    printf("[CORES] Gerando chave de criptografia visual...\n");
    
    int copiar = (tamanho_semente > 32) ? 32 : tamanho_semente;
    memcpy(chave->chave_mestra, semente, copiar);
    
    unsigned int seed = 0;
    for (int i = 0; i < copiar; i++) {
        seed += chave->chave_mestra[i];
    }
    srand(seed);
    
    // Cria mapeamento aleatório byte -> cor
    CorRGB cores_disponiveis[256];
    for (int i = 0; i < 256; i++) {
        cores_disponiveis[i].r = i;
        cores_disponiveis[i].g = i;
        cores_disponiveis[i].b = i;
    }
    
    // Fisher-Yates shuffle
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1);
        CorRGB temp = cores_disponiveis[i];
        cores_disponiveis[i] = cores_disponiveis[j];
        cores_disponiveis[j] = temp;
    }
    
    for (int i = 0; i < 256; i++) {
        chave->mapeamento_byte_para_cor[i] = cores_disponiveis[i];
        chave->mapeamento_cor_para_byte[i] = i;
    }
    
    printf("[CORES] Chave gerada.\n");
}

unsigned char* dados_para_imagem(const unsigned char *dados, int tamanho, 
                                  ChaveCores *chave, int *largura, int *altura) {
    (void)chave;
    printf("[CORES] Codificando %d bytes em imagem...\n", tamanho);
    
    int num_pixels = (tamanho + 2) / 3;
    *largura = (int)sqrt(num_pixels) + 1;
    *altura = (num_pixels + *largura - 1) / *largura;
    
    unsigned char *imagem = malloc(3 * (*largura) * (*altura));
    if (!imagem) {
        perror("Erro ao alocar memória para imagem");
        return NULL;
    }
    memset(imagem, 0, 3 * (*largura) * (*altura));
    
    int pixel_idx = 0;
    for (int i = 0; i < tamanho; i += 3) {
        int x = pixel_idx % (*largura);
        int y = pixel_idx / (*largura);
        int idx = (y * (*largura) + x) * 3;
        
        imagem[idx] = dados[i];
        imagem[idx+1] = (i + 1 < tamanho) ? dados[i + 1] : 0;
        imagem[idx+2] = (i + 2 < tamanho) ? dados[i + 2] : 0;
        
        pixel_idx++;
    }
    
    printf("[CORES] Imagem criada: %dx%d (%d bytes)\n", 
           *largura, *altura, (*largura) * (*altura) * 3);
    return imagem;
}

unsigned char* imagem_para_dados(const unsigned char *imagem, int largura, int altura,
                                  ChaveCores *chave, int *tamanho) {
    (void)chave;
    printf("[CORES] Decodificando imagem %dx%d...\n", largura, altura);
    
    int num_pixels = largura * altura;
    *tamanho = num_pixels * 3;
    
    unsigned char *dados = malloc(*tamanho);
    if (!dados) {
        perror("Erro ao alocar memória para dados");
        return NULL;
    }
    
    for (int i = 0; i < num_pixels; i++) {
        dados[i*3] = imagem[i*3];
        dados[i*3+1] = imagem[i*3+1];
        dados[i*3+2] = imagem[i*3+2];
    }
    
    printf("[CORES] Decodificado: %d bytes\n", *tamanho);
    return dados;
}

void inverter_fragmento(unsigned char *dados, int tamanho) {
    for (int i = 0; i < tamanho; i++) {
        dados[i] = ~dados[i];
    }
}
