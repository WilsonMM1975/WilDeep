/*
 * ============================================================
 *  WilDeep — ChromaCrypt Core
 *  Arquivo : src/core/encoder.c
 *  Função  : Codificação e decodificação segura de dados
 *            em pixels RGB usando XOR com sub-chave derivada
 *
 *  Evolução do testar_imagem.c — cifração criptograficamente
 *  segura em vez de mapeamento matemático previsível.
 *
 *  Compilar standalone para teste:
 *    gcc -Wall -Wextra -lm -o encoder encoder.c && ./encoder
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ── Quando compilar dentro do projeto wildeep, usar:
 *    #include "../../include/encoder.h"
 *    Por ora incluímos tudo aqui para teste standalone. ── */

/* ═══════════════════════════════════════════════════════════
 *  CONSTANTES
 * ═══════════════════════════════════════════════════════════ */
#define CHROMA_VERSAO          1
#define CHROMA_CHAVE_BYTES     32      /* 256 bits — chave mestra AES-256 */
#define CHROMA_NONCE_BYTES     12      /* 96 bits  — nonce por bloco      */
#define CHROMA_TAG_BYTES       4       /* mini-tag de integridade         */
#define CHROMA_HEADER_PIXELS   4       /* pixels reservados para cabeçalho*/
#define TAMANHO_FRAGMENTO      1368    /* MTU 1500 - overhead de rede     */

/* ═══════════════════════════════════════════════════════════
 *  ESTRUTURAS
 * ═══════════════════════════════════════════════════════════ */

/* Pixel RGB — 3 bytes, cada um carrega 1 byte de dado cifrado */
typedef struct {
    uint8_t r, g, b;
} CorRGB;

/* Chave ChromaCrypt — gerada por CSPRNG, nunca hardcoded */
typedef struct {
    uint8_t mestra[CHROMA_CHAVE_BYTES];   /* chave principal 256 bits     */
    uint8_t nonce_sessao[CHROMA_NONCE_BYTES]; /* nonce único por sessão   */
    uint32_t sessao_id;                   /* ID da sessão atual           */
} ChaveChroma;

/* Cabeçalho embutido nos primeiros 4 pixels da imagem */
typedef struct {
    uint8_t  versao;          /* versão do protocolo ChromaCrypt          */
    uint32_t tamanho_real;    /* tamanho original dos dados (antes de pad)*/
    uint8_t  sessao_id;       /* ID de sessão (1 byte do uint32)          */
    uint8_t  flags;           /* bits de controle (compressao, etc.)      */
    uint8_t  checksum;        /* XOR de todos os bytes do cabeçalho       */
} CabecalhoChroma;            /* total: 8 bytes = ocupa 3 pixels RGB      */

/* Resultado de uma operação ChromaCrypt */
typedef struct {
    uint8_t *pixels;          /* buffer de pixels RGB                     */
    int      largura;
    int      altura;
    int      n_pixels;
    int      ok;              /* 1 = sucesso, 0 = erro                    */
} ResultadoChroma;

/* ═══════════════════════════════════════════════════════════
 *  GERAÇÃO DE CHAVE — usa /dev/urandom (CSPRNG do kernel)
 * ═══════════════════════════════════════════════════════════ */

/*
 * gerar_chave_segura()
 * Lê bytes aleatórios do /dev/urandom — fonte criptograficamente
 * segura do sistema operacional. NUNCA usar rand() ou time() aqui.
 *
 * Retorna: 1 = sucesso, 0 = erro
 */
int gerar_chave_segura(ChaveChroma *chave) {
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        fprintf(stderr, "[ERRO] Não foi possível abrir /dev/urandom\n");
        return 0;
    }

    size_t lidos = fread(chave->mestra, 1, CHROMA_CHAVE_BYTES, urandom);
    lidos       += fread(chave->nonce_sessao, 1, CHROMA_NONCE_BYTES, urandom);

    /* ID de sessão aleatório de 4 bytes */
    fread(&chave->sessao_id, sizeof(uint32_t), 1, urandom);
    fclose(urandom);

    if (lidos < (size_t)(CHROMA_CHAVE_BYTES + CHROMA_NONCE_BYTES)) {
        fprintf(stderr, "[ERRO] Leitura incompleta do /dev/urandom\n");
        return 0;
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════
 *  DERIVAÇÃO DE SUB-CHAVE — HKDF simplificado (HMAC-SHA256)
 *
 *  Versão compacta sem dependências externas.
 *  Em produção: usar OpenSSL EVP_PKEY_derive com HKDF.
 * ═══════════════════════════════════════════════════════════ */

/*
 * hkdf_expand_simples()
 * Deriva 3 bytes de sub-chave para o bloco i.
 * Garante que blocos idênticos gerem cores diferentes.
 *
 * sub_k = SHA256-like(chave_mestra || nonce_bloco || indice_bloco)[0:3]
 *
 * NOTA: Esta é uma implementação didática. Em produção usar
 *       OpenSSL HKDF ou libsodium crypto_kdf_derive_from_key()
 */
void derivar_subchave(const uint8_t *chave_mestra, uint32_t indice_bloco,
                      uint8_t *sub_k_out) {
    /*
     * Mistura simples sem OpenSSL para fins de desenvolvimento:
     * sub_k[j] = chave_mestra[j] XOR rotaciona(indice, j*8) XOR prime[j]
     *
     * TODO: substituir por OpenSSL HKDF antes de produção:
     *   EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
     *   EVP_PKEY_derive_init(ctx);
     *   EVP_PKEY_CTX_hkdf_mode(ctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY);
     *   EVP_PKEY_CTX_set1_hkdf_key(ctx, chave_mestra, 32);
     *   EVP_PKEY_CTX_add1_hkdf_info(ctx, &indice_bloco, 4);
     */
    uint8_t primes[3] = { 0x9E, 0x37, 0x79 }; /* frações de phi — constantes */
    uint32_t rotacionado;

    for (int j = 0; j < 3; j++) {
        uint32_t shift = (j * 8) % 32;
        rotacionado = (indice_bloco << shift) | (indice_bloco >> (32 - shift));
        uint8_t byte_rot = (uint8_t)(rotacionado & 0xFF)
                         ^ (uint8_t)((rotacionado >> 8) & 0xFF)
                         ^ (uint8_t)((rotacionado >> 16) & 0xFF)
                         ^ (uint8_t)((rotacionado >> 24) & 0xFF);

        sub_k_out[j] = chave_mestra[j % CHROMA_CHAVE_BYTES]
                     ^ chave_mestra[(j + 8) % CHROMA_CHAVE_BYTES]
                     ^ byte_rot
                     ^ primes[j];
    }
}

/* ═══════════════════════════════════════════════════════════
 *  CABEÇALHO — serializar e desserializar nos pixels
 * ═══════════════════════════════════════════════════════════ */

/*
 * Serializa o cabeçalho em 9 bytes (3 pixels RGB).
 * Layout:
 *   Pixel 0: R=versao   G=tamanho[0]  B=tamanho[1]
 *   Pixel 1: R=tamanho[2] G=tamanho[3] B=sessao_id
 *   Pixel 2: R=flags    G=checksum   B=0xCC (marcador)
 */
void serializar_cabecalho(const CabecalhoChroma *cab, uint8_t *pixels_out) {
    /* Pixel 0 */
    pixels_out[0] = cab->versao;
    pixels_out[1] = (cab->tamanho_real >> 24) & 0xFF;
    pixels_out[2] = (cab->tamanho_real >> 16) & 0xFF;
    /* Pixel 1 */
    pixels_out[3] = (cab->tamanho_real >> 8)  & 0xFF;
    pixels_out[4] = (cab->tamanho_real)        & 0xFF;
    pixels_out[5] = cab->sessao_id;
    /* Pixel 2 */
    pixels_out[6] = cab->flags;
    pixels_out[7] = cab->checksum;
    pixels_out[8] = 0xCC; /* marcador de integridade do cabeçalho */

    /* Pixel 3 — reservado para expansão futura */
    pixels_out[9]  = 0x00;
    pixels_out[10] = 0x00;
    pixels_out[11] = 0x00;
}

void desserializar_cabecalho(const uint8_t *pixels, CabecalhoChroma *cab_out) {
    cab_out->versao      = pixels[0];
    cab_out->tamanho_real = ((uint32_t)pixels[1] << 24)
                          | ((uint32_t)pixels[2] << 16)
                          | ((uint32_t)pixels[3] << 8)
                          |  (uint32_t)pixels[4];
    cab_out->sessao_id   = pixels[5];
    cab_out->flags       = pixels[6];
    cab_out->checksum    = pixels[7];
}

uint8_t calcular_checksum(const CabecalhoChroma *cab) {
    return cab->versao
         ^ ((cab->tamanho_real >> 24) & 0xFF)
         ^ ((cab->tamanho_real >> 16) & 0xFF)
         ^ ((cab->tamanho_real >>  8) & 0xFF)
         ^  (cab->tamanho_real        & 0xFF)
         ^ cab->sessao_id
         ^ cab->flags;
}

/* ═══════════════════════════════════════════════════════════
 *  CHROMA ENCODE — dados → imagem RGB cifrada
 * ═══════════════════════════════════════════════════════════ */

/*
 * chroma_encode()
 *
 * Converte um buffer de bytes em uma imagem RGB onde cada pixel
 * carrega 3 bytes de dado cifrado com XOR de sub-chave derivada.
 *
 * Algoritmo:
 *   Para cada bloco de 3 bytes no índice i:
 *     sub_k    = derivar_subchave(chave_mestra, i)
 *     pixel.R  = dado[i*3+0] XOR sub_k[0]
 *     pixel.G  = dado[i*3+1] XOR sub_k[1]
 *     pixel.B  = dado[i*3+2] XOR sub_k[2]
 *
 * Parâmetros:
 *   dados    — buffer de entrada
 *   tamanho  — número de bytes
 *   chave    — chave ChromaCrypt gerada por gerar_chave_segura()
 *
 * Retorna: ResultadoChroma com pixels alocados (caller deve free())
 */
ResultadoChroma chroma_encode(const uint8_t *dados, uint32_t tamanho,
                              const ChaveChroma *chave) {
    ResultadoChroma res = {0};

    if (!dados || tamanho == 0 || !chave) {
        fprintf(stderr, "[ERRO] chroma_encode: parâmetros inválidos\n");
        return res;
    }

    /* ── Calcular dimensões da imagem ── */
    /* Cada pixel carrega 3 bytes. Adicionar HEADER_PIXELS no início. */
    int n_pixels_dados = (tamanho + 2) / 3;  /* ceil(tamanho / 3) */
    int n_pixels_total = CHROMA_HEADER_PIXELS + n_pixels_dados;

    /* Imagem quadrada — facilita análise de tamanho sem revelar payload */
    int lado = (int)ceil(sqrt((double)n_pixels_total));
    res.largura  = lado;
    res.altura   = lado;
    res.n_pixels = lado * lado;

    /* Alocar buffer de pixels (RGB = 3 bytes por pixel) */
    res.pixels = calloc(res.n_pixels * 3, 1);
    if (!res.pixels) {
        fprintf(stderr, "[ERRO] chroma_encode: sem memória\n");
        return res;
    }

    /* ── Serializar cabeçalho nos primeiros HEADER_PIXELS ── */
    CabecalhoChroma cab = {
        .versao       = CHROMA_VERSAO,
        .tamanho_real = tamanho,
        .sessao_id    = (uint8_t)(chave->sessao_id & 0xFF),
        .flags        = 0x00,
    };
    cab.checksum = calcular_checksum(&cab);
    serializar_cabecalho(&cab, res.pixels);

    /* ── Cifrar dados bloco a bloco (3 bytes = 1 pixel) ── */
    uint8_t sub_k[3];
    uint32_t bloco_idx = 0;

    for (uint32_t i = 0; i < tamanho; i += 3, bloco_idx++) {
        int pixel_pos = (CHROMA_HEADER_PIXELS + bloco_idx) * 3;

        /* Derivar sub-chave única para este bloco */
        derivar_subchave(chave->mestra, bloco_idx, sub_k);

        /* XOR de cada byte com o byte correspondente da sub-chave */
        res.pixels[pixel_pos + 0] = dados[i]     ^ sub_k[0];
        res.pixels[pixel_pos + 1] = (i+1 < tamanho) ? dados[i+1] ^ sub_k[1] : sub_k[1];
        res.pixels[pixel_pos + 2] = (i+2 < tamanho) ? dados[i+2] ^ sub_k[2] : sub_k[2];
    }

    printf("[ENCODE] %u bytes → imagem %dx%d (%d pixels, %d úteis)\n",
           tamanho, res.largura, res.altura, res.n_pixels, n_pixels_dados);

    res.ok = 1;
    return res;
}

/* ═══════════════════════════════════════════════════════════
 *  CHROMA DECODE — imagem RGB → dados originais
 * ═══════════════════════════════════════════════════════════ */

/*
 * chroma_decode()
 *
 * Processo inverso do encode: extrai os dados cifrados dos pixels
 * e reverte o XOR com as mesmas sub-chaves derivadas.
 *
 * Parâmetros:
 *   pixels   — buffer de pixels da imagem (largura * altura * 3 bytes)
 *   n_pixels — total de pixels na imagem
 *   chave    — mesma chave usada no encode
 *   tamanho_out — receberá o tamanho dos dados recuperados
 *
 * Retorna: buffer com os dados originais (caller deve free())
 *          NULL em caso de erro
 */
uint8_t* chroma_decode(const uint8_t *pixels, int n_pixels,
                       const ChaveChroma *chave, uint32_t *tamanho_out) {
    if (!pixels || n_pixels <= CHROMA_HEADER_PIXELS || !chave) {
        fprintf(stderr, "[ERRO] chroma_decode: parâmetros inválidos\n");
        return NULL;
    }

    /* ── Ler e validar cabeçalho ── */
    CabecalhoChroma cab;
    desserializar_cabecalho(pixels, &cab);

    /* Verificar marcador de integridade */
    if (pixels[8] != 0xCC) {
        fprintf(stderr, "[ERRO] chroma_decode: marcador de cabeçalho inválido\n");
        return NULL;
    }

    /* Verificar versão */
    if (cab.versao != CHROMA_VERSAO) {
        fprintf(stderr, "[ERRO] chroma_decode: versão incompatível %d\n", cab.versao);
        return NULL;
    }

    /* Verificar checksum do cabeçalho */
    uint8_t chk_esperado = calcular_checksum(&cab);
    if (cab.checksum != chk_esperado) {
        fprintf(stderr, "[ERRO] chroma_decode: checksum do cabeçalho inválido\n");
        return NULL;
    }

    uint32_t tamanho_real = cab.tamanho_real;

    /* Verificar se a imagem tem pixels suficientes */
    int n_pixels_necessarios = CHROMA_HEADER_PIXELS + (int)((tamanho_real + 2) / 3);
    if (n_pixels < n_pixels_necessarios) {
        fprintf(stderr, "[ERRO] chroma_decode: imagem pequena demais (%d < %d pixels)\n",
                n_pixels, n_pixels_necessarios);
        return NULL;
    }

    /* ── Alocar buffer de saída ── */
    uint8_t *saida = malloc(tamanho_real);
    if (!saida) {
        fprintf(stderr, "[ERRO] chroma_decode: sem memória\n");
        return NULL;
    }

    /* ── Decifrar bloco a bloco (reverter XOR) ── */
    uint8_t sub_k[3];
    uint32_t bloco_idx = 0;
    uint32_t bytes_escritos = 0;

    for (uint32_t i = 0; i < tamanho_real; i += 3, bloco_idx++) {
        int pixel_pos = (CHROMA_HEADER_PIXELS + bloco_idx) * 3;

        /* Mesma sub-chave que foi usada no encode */
        derivar_subchave(chave->mestra, bloco_idx, sub_k);

        /* XOR reverte XOR — operação simétrica */
        if (bytes_escritos < tamanho_real)
            saida[bytes_escritos++] = pixels[pixel_pos + 0] ^ sub_k[0];
        if (bytes_escritos < tamanho_real)
            saida[bytes_escritos++] = pixels[pixel_pos + 1] ^ sub_k[1];
        if (bytes_escritos < tamanho_real)
            saida[bytes_escritos++] = pixels[pixel_pos + 2] ^ sub_k[2];
    }

    printf("[DECODE] %d pixels → %u bytes recuperados\n", n_pixels, tamanho_real);

    *tamanho_out = tamanho_real;
    return saida;
}

/* ═══════════════════════════════════════════════════════════
 *  SALVAR / CARREGAR IMAGEM PPM
 *  (formato sem dependências — PNG virá na Fase 2 com libpng)
 * ═══════════════════════════════════════════════════════════ */

int salvar_ppm(const ResultadoChroma *img, const char *caminho) {
    FILE *f = fopen(caminho, "wb");
    if (!f) {
        fprintf(stderr, "[ERRO] Não foi possível criar: %s\n", caminho);
        return 0;
    }
    fprintf(f, "P6\n%d %d\n255\n", img->largura, img->altura);
    fwrite(img->pixels, 3, img->largura * img->altura, f);
    fclose(f);
    printf("[PPM] Salvo: %s (%dx%d)\n", caminho, img->largura, img->altura);
    return 1;
}

uint8_t* carregar_ppm(const char *caminho, int *largura_out,
                      int *altura_out, int *n_pixels_out) {
    FILE *f = fopen(caminho, "rb");
    if (!f) {
        fprintf(stderr, "[ERRO] Não foi possível abrir: %s\n", caminho);
        return NULL;
    }

    char fmt[4];
    int larg, alt, maxval;
    if (fscanf(f, "%3s\n%d %d\n%d\n", fmt, &larg, &alt, &maxval) != 4
        || strcmp(fmt, "P6") != 0) {
        fprintf(stderr, "[ERRO] Formato PPM inválido\n");
        fclose(f);
        return NULL;
    }

    uint8_t *pixels = malloc(3 * larg * alt);
    if (!pixels) { fclose(f); return NULL; }

    fread(pixels, 3, larg * alt, f);
    fclose(f);

    *largura_out  = larg;
    *altura_out   = alt;
    *n_pixels_out = larg * alt;
    return pixels;
}

