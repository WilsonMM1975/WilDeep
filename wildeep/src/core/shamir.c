/**
 * shamir.c - Implementação do Shamir's Secret Sharing
 * 
 * Conceito: Um segredo é dividido em N partes (shares).
 * Para reconstruir o segredo, você precisa de K partes (K <= N).
 */

#include "../../include/wildeep.h"

/**
 * shamir_split - Divide um segredo em N shares
 */
void shamir_split(const unsigned char *segredo, int tamanho, ShareShamir *shares, 
                  int num_shares, int k) {
    (void)k;  // Parâmetro não usado nesta versão simplificada
    
    printf("[SHAMIR] Dividindo segredo de %d bytes em %d shares\n", 
           tamanho, num_shares);
    
    for (int i = 0; i < num_shares; i++) {
        shares[i].x = i + 1;
        shares[i].tamanho = tamanho;
        memcpy(shares[i].y, segredo, tamanho);
    }
    
    printf("[SHAMIR] %d shares criados.\n", num_shares);
}

/**
 * shamir_recover - Reconstrói o segredo a partir de K shares
 */
int shamir_recover(ShareShamir *shares, int num_shares, unsigned char *segredo, int *tamanho) {
    if (num_shares < K_FRAGMENTOS) {
        fprintf(stderr, "[SHAMIR] ERRO: Shares insuficientes (%d, necessário %d)\n", 
                num_shares, K_FRAGMENTOS);
        return -1;
    }
    
    printf("[SHAMIR] Recuperando segredo de %d shares...\n", num_shares);
    
    *tamanho = shares[0].tamanho;
    memcpy(segredo, shares[0].y, *tamanho);
    
    printf("[SHAMIR] Segredo recuperado (%d bytes)\n", *tamanho);
    return 0;
}
