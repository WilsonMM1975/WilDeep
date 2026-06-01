/**
 * cores.c - Criptografia visual baseada em cores RGB
 *
 * Correções aplicadas:
 *   1. Bug: salvar_imagem_ppm estava dentro de inverter_fragmento — corrigido
 *   2. srand(seed fraca) substituído por /dev/urandom
 *   3. dados_para_imagem agora usa XOR com sub-chave derivada (cifração real)
 *   4. imagem_para_dados reverte o XOR corretamente
 */

#include "../../include/wildeep.h"

/* ═══════════════════════════════════════════════════════════
 *  DERIVAÇÃO DE SUB-CHAVE (igual ao encoder.c)
 *  sub_k[j] = mistura da chave_mestra com o índice do bloco
 * ═══════════════════════════════════════════════════════════ */
static void derivar_subchave(const unsigned char *chave_mestra,
                              unsigned int indice_bloco,
                              unsigned char *sub_k_out) {
    unsigned char primes[3] = { 0x9E, 0x37, 0x79 };

    for (int j = 0; j < 3; j++) {
        unsigned int shift = (j * 8) % 32;
        unsigned int rot = (indice_bloco << shift) | (indice_bloco >> (32 - shift));
        unsigned char byte_rot = (unsigned char)(rot & 0xFF)
                               ^ (unsigned char)((rot >> 8)  & 0xFF)
                               ^ (unsigned char)((rot >> 16) & 0xFF)
                               ^ (unsigned char)((rot >> 24) & 0xFF);

        sub_k_out[j] = chave_mestra[j % 32]
                     ^ chave_mestra[(j + 8) % 32]
                     ^ byte_rot
                     ^ primes[j];
    }
}

/* ═══════════════════════════════════════════════════════════
 *  GERAR CHAVE DE CORES
 *  Usa /dev/urandom em vez de srand() fraco
 * ═══════════════════════════════════════════════════════════ */
void gerar_chave_cores(ChaveCores *chave, const unsigned char *semente,
                       int tamanho_semente) {
    printf("[CORES] Gerando chave de criptografia visual...\n");

    /* Se semente fornecida, usar. Senão, gerar do /dev/urandom */
    if (semente && tamanho_semente > 0) {
        int copiar = (tamanho_semente > 32) ? 32 : tamanho_semente;
        memcpy(chave->chave_mestra, semente, copiar);
        printf("[CORES] Chave derivada da semente fornecida.\n");
    } else {
        /* ✅ CSPRNG real — sem srand/rand */
        FILE *urandom = fopen("/dev/urandom", "rb");
        if (!urandom) {
            fprintf(stderr, "[CORES] ERRO: Não foi possível abrir /dev/urandom\n");
            return;
        }
        fread(chave->chave_mestra, 1, 32, urandom);
        fclose(urandom);
        printf("[CORES] Chave gerada via /dev/urandom (CSPRNG).\n");
    }

    /* Mapeamento de cores — Fisher-Yates com bytes da chave_mestra */
    CorRGB cores[256];
    for (int i = 0; i < 256; i++) {
        cores[i].r = i;
        cores[i].g = (i * 7)  % 256;
        cores[i].b = (i * 13) % 256;
    }

    /* Shuffle usando bytes da chave — determinístico e derivado da chave */
    for (int i = 255; i > 0; i--) {
        /* Índice j derivado da chave mestra e de i */
        int j = (chave->chave_mestra[i % 32] ^ (unsigned char)i) % (i + 1);
        CorRGB temp   = cores[i];
        cores[i]      = cores[j];
        cores[j]      = temp;
    }

    for (int i = 0; i < 256; i++) {
        chave->mapeamento_byte_para_cor[i]  = cores[i];
        chave->mapeamento_cor_para_byte[i]  = i;
    }

    printf("[CORES] Chave gerada com sucesso.\n");
}

/* ═══════════════════════════════════════════════════════════
 *  DADOS → IMAGEM RGB (com cifração XOR real)
 *
 *  Cada bloco de 3 bytes é cifrado com XOR da sub-chave
 *  derivada do índice do bloco — igual ao encoder.c
 * ═══════════════════════════════════════════════════════════ */
unsigned char* dados_para_imagem(const unsigned char *dados, int tamanho,
                                  ChaveCores *chave, int *largura, int *altura) {
    printf("[CORES] Codificando %d bytes em imagem (XOR cifrado)...\n", tamanho);

    int num_pixels = (tamanho + 2) / 3;
    *largura = (int)sqrt((double)num_pixels) + 1;
    *altura  = (num_pixels + *largura - 1) / *largura;

    unsigned char *imagem = calloc(3 * (*largura) * (*altura), 1);
    if (!imagem) {
        perror("[CORES] Erro ao alocar memória para imagem");
        return NULL;
    }

    unsigned char sub_k[3];
    unsigned int bloco_idx = 0;

    for (int i = 0; i < tamanho; i += 3, bloco_idx++) {
        int x   = (int)bloco_idx % (*largura);
        int y   = (int)bloco_idx / (*largura);
        int idx = (y * (*largura) + x) * 3;

        /* ✅ Derivar sub-chave única para este bloco */
        derivar_subchave(chave->chave_mestra, bloco_idx, sub_k);

        /* ✅ XOR — cifração real */
        imagem[idx + 0] = dados[i]           ^ sub_k[0];
        imagem[idx + 1] = (i+1 < tamanho) ? dados[i+1] ^ sub_k[1] : sub_k[1];
        imagem[idx + 2] = (i+2 < tamanho) ? dados[i+2] ^ sub_k[2] : sub_k[2];
    }

    printf("[CORES] Imagem cifrada: %dx%d (%d bytes)\n",
           *largura, *altura, (*largura) * (*altura) * 3);
    return imagem;
}

/* ═══════════════════════════════════════════════════════════
 *  IMAGEM RGB → DADOS (decifração XOR — reverte encode)
 * ═══════════════════════════════════════════════════════════ */
unsigned char* imagem_para_dados(const unsigned char *imagem, int largura, int altura,
                                  ChaveCores *chave, int *tamanho) {
    printf("[CORES] Decodificando imagem %dx%d (XOR decifrado)...\n", largura, altura);

    int num_pixels = largura * altura;
    *tamanho = num_pixels * 3;

    unsigned char *dados = malloc(*tamanho);
    if (!dados) {
        perror("[CORES] Erro ao alocar memória para dados");
        return NULL;
    }

    unsigned char sub_k[3];
    unsigned int bloco_idx = 0;

    for (int i = 0; i < num_pixels; i++, bloco_idx++) {
        /* Mesma sub-chave do encode */
        derivar_subchave(chave->chave_mestra, bloco_idx, sub_k);

        /* XOR reverte XOR — operação simétrica */
        dados[i*3 + 0] = imagem[i*3 + 0] ^ sub_k[0];
        dados[i*3 + 1] = imagem[i*3 + 1] ^ sub_k[1];
        dados[i*3 + 2] = imagem[i*3 + 2] ^ sub_k[2];
    }

    printf("[CORES] Decodificado: %d bytes\n", *tamanho);
    return dados;
}

/* ═══════════════════════════════════════════════════════════
 *  INVERTER FRAGMENTO — camada extra de ofuscação (~XOR bit flip)
 *  CORRIGIDO: era a causa do bug — estava dentro de outra função
 * ═══════════════════════════════════════════════════════════ */
void inverter_fragmento(unsigned char *dados, int tamanho) {
    for (int i = 0; i < tamanho; i++) {
        dados[i] = ~dados[i];   /* flip de todos os bits */
    }
}

/* ═══════════════════════════════════════════════════════════
 *  SALVAR IMAGEM PPM
 *  CORRIGIDO: estava incorretamente aninhada dentro de
 *  inverter_fragmento() no código original
 * ═══════════════════════════════════════════════════════════ */
void salvar_imagem_ppm(const unsigned char *imagem, int largura,
                       int altura, const char *nome) {
    FILE *f = fopen(nome, "wb");
    if (!f) {
        fprintf(stderr, "[CORES] Erro ao criar: %s\n", nome);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", largura, altura);
    fwrite(imagem, 3, largura * altura, f);
    fclose(f);
    printf("[CORES] Imagem salva: %s (%dx%d)\n", nome, largura, altura);
}
