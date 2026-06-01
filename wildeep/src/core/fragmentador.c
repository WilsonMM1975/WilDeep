/**
 * fragmentador.c - Divide o arquivo em 500 fragmentos
 */

#include "../../include/wildeep.h"

int fragmentar_arquivo(const char *entrada, Fragmento *fragmentos) {
    FILE *f = fopen(entrada, "rb");
    if (!f) {
        perror("Erro ao abrir arquivo de entrada");
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    long tamanho_total = ftell(f);
    rewind(f);
    
    printf("[FRAGMENTADOR] Arquivo: %s (%ld bytes)\n", entrada, tamanho_total);
    printf("[FRAGMENTADOR] Dividindo em %d fragmentos...\n", NUM_FRAGMENTOS);
    
    int tamanho_por_fragmento = tamanho_total / NUM_FRAGMENTOS;
    if (tamanho_por_fragmento == 0) {
        tamanho_por_fragmento = 1;
    }
    
    if (tamanho_por_fragmento > TAMANHO_FRAGMENTO) {
        fprintf(stderr, "ERRO: Arquivo muito grande. Aumente TAMANHO_FRAGMENTO\n");
        fclose(f);
        return -1;
    }
    
    for (int i = 0; i < NUM_FRAGMENTOS; i++) {
        fragmentos[i].id = i + 1;
        fragmentos[i].total = NUM_FRAGMENTOS;
        
        int offset = i * tamanho_por_fragmento;
        fseek(f, offset, SEEK_SET);
        
        size_t lidos = fread(fragmentos[i].dados_visuais, 1, tamanho_por_fragmento, f);
        fragmentos[i].tamanho_visual = lidos;
        
        // DEBUG: Mostra os primeiros bytes do fragmento 0 e 1
        if (i == 0 || i == 1) {
            printf("[DEBUG] Fragmento %d primeiros 16 bytes: ", i+1);
            for (int k = 0; k < 16 && k < lidos; k++) {
                printf("%02X ", fragmentos[i].dados_visuais[k]);
            }
            printf("\n");
        }
        
        unsigned int soma = 0;
        for (size_t j = 0; j < lidos; j++) {
            soma += fragmentos[i].dados_visuais[j];
        }
        fragmentos[i].checksum = soma;
        
        fragmentos[i].share.x = i + 1;
        fragmentos[i].share.tamanho = 32;  /* tamanho da chave mestra */
        memset(fragmentos[i].share.y, 0, sizeof(fragmentos[i].share.y));
        
        strcpy(fragmentos[i].proximo_hop, "0.0.0.0");
    }
    
    fclose(f);
    printf("[FRAGMENTADOR] Sucesso! %d fragmentos criados.\n", NUM_FRAGMENTOS);
    return 0;
}

int remontar_arquivo(Fragmento *fragmentos, int num_recebidos, const char *saida) {
    if (num_recebidos < K_FRAGMENTOS) {
        fprintf(stderr, "[REMONTADOR] ERRO: Fragmentos insuficientes! (%d/%d)\n", 
                num_recebidos, K_FRAGMENTOS);
        return -1;
    }
    
    printf("[REMONTADOR] Remontando com %d fragmentos...\n", num_recebidos);
    
    for (int i = 0; i < num_recebidos - 1; i++) {
        for (int j = i + 1; j < num_recebidos; j++) {
            if (fragmentos[i].id > fragmentos[j].id) {
                Fragmento temp = fragmentos[i];
                fragmentos[i] = fragmentos[j];
                fragmentos[j] = temp;
            }
        }
    }
    
    FILE *f = fopen(saida, "wb");
    if (!f) {
        perror("Erro ao criar arquivo de saída");
        return -1;
    }
    
    for (int i = 0; i < num_recebidos; i++) {
        unsigned int soma = 0;
        for (int j = 0; j < fragmentos[i].tamanho_visual; j++) {
            soma += fragmentos[i].dados_visuais[j];
        }
        
       // if (soma != (unsigned int)fragmentos[i].checksum) {
           // fprintf(stderr, "[REMONTADOR] Aviso: Checksum inválido no fragmento %d\n", 
             //       fragmentos[i].id);
       // }
        
        fwrite(fragmentos[i].dados_visuais, 1, fragmentos[i].tamanho_visual, f);
    }
    
    fclose(f);
    printf("[REMONTADOR] Sucesso! Arquivo reconstruído: %s\n", saida);
    return 0;
}
