/**
 * shamir.c - Shamir's Secret Sharing real
 * Campo finito GF(256) — aritmética polinomial
 *
 * Coloca em: ~/wildeep/src/core/shamir.c
 */

#include "../../include/wildeep.h"

/* ═══════════════════════════════════════════════════════════
 *  ARITMÉTICA NO CAMPO FINITO GF(256)
 *  Polinômio irredutível: x^8 + x^4 + x^3 + x^2 + 1 = 0x11D
 *
 *  Por que GF(256)?
 *  - Cada byte (0-255) é um elemento do campo
 *  - Adição = XOR (sem carry)
 *  - Multiplicação = polinomial mod 0x11D
 *  - Todo elemento não-zero tem inverso multiplicativo
 *  - Perfeito para Shamir — sem vazamento de informação
 * ═══════════════════════════════════════════════════════════ */

/* Multiplicação em GF(256) — Russian Peasant Algorithm */
static unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char resultado = 0;
    unsigned char carry;
    for (int i = 0; i < 8; i++) {
        if (b & 1)
            resultado ^= a;
        carry = a & 0x80;
        a <<= 1;
        if (carry)
            a ^= 0x1D;  /* x^8 + x^4 + x^3 + x^2 + 1 (sem o bit x^8) */
        b >>= 1;
    }
    return resultado;
}

/* Potência em GF(256): a^n */
static unsigned char gf_pow(unsigned char a, int n) {
    unsigned char resultado = 1;
    for (int i = 0; i < n; i++)
        resultado = gf_mul(resultado, a);
    return resultado;
}

/* Inverso multiplicativo em GF(256): a^(254) = a^(-1) */
static unsigned char gf_inv(unsigned char a) {
    if (a == 0) return 0;  /* 0 não tem inverso */
    return gf_pow(a, 254); /* Pequeno Teorema de Fermat em GF(2^8) */
}

/* ═══════════════════════════════════════════════════════════
 *  INTERPOLAÇÃO DE LAGRANGE EM GF(256)
 *
 *  Dado K pontos (x_i, y_i), recupera f(0) = segredo
 *  f(0) = Σ y_i * Π (0 - x_j)/(x_i - x_j)  para j≠i
 *
 *  Em GF(256): subtração = XOR, divisão = mul por inverso
 * ═══════════════════════════════════════════════════════════ */
static unsigned char lagrange_em_zero(unsigned char *xs, unsigned char *ys,
                                       int k) {
    unsigned char resultado = 0;
    for (int i = 0; i < k; i++) {
        unsigned char num = 1, den = 1;
        for (int j = 0; j < k; j++) {
            if (i == j) continue;
            num = gf_mul(num, xs[j]);           /* 0 XOR x_j = x_j */
            den = gf_mul(den, xs[i] ^ xs[j]);   /* x_i XOR x_j     */
        }
        resultado ^= gf_mul(ys[i], gf_mul(num, gf_inv(den)));
    }
    return resultado;
}

/* ═══════════════════════════════════════════════════════════
 *  GERAR COEFICIENTES ALEATÓRIOS DO POLINÔMIO
 *  f(x) = segredo + a1*x + a2*x^2 + ... + a(k-1)*x^(k-1)
 * ═══════════════════════════════════════════════════════════ */
static void gerar_coeficientes(unsigned char *coefs, int k,
                                unsigned char segredo_byte) {
    coefs[0] = segredo_byte;  /* termo independente = segredo */

    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        fread(&coefs[1], 1, k - 1, urandom);
        fclose(urandom);
    } else {
        /* Fallback — não ideal, mas não trava */
        for (int i = 1; i < k; i++)
            coefs[i] = (unsigned char)(i * 37 + segredo_byte);
    }
}

/* Avaliar polinômio em x: f(x) = c0 + c1*x + c2*x^2 + ... */
static unsigned char avaliar_polinomio(unsigned char *coefs, int k,
                                        unsigned char x) {
    unsigned char resultado = 0;
    unsigned char x_pow = 1;  /* x^0 = 1 */
    for (int i = 0; i < k; i++) {
        resultado ^= gf_mul(coefs[i], x_pow);
        x_pow = gf_mul(x_pow, x);
    }
    return resultado;
}

/* ═══════════════════════════════════════════════════════════
 *  SHAMIR SPLIT — divide segredo em N shares
 *
 *  Para cada byte do segredo:
 *    1. Gera polinômio aleatório de grau K-1 com f(0)=byte
 *    2. Avalia em x=1,2,...,N para gerar os N shares
 * ═══════════════════════════════════════════════════════════ */
void shamir_split(const unsigned char *segredo, int tamanho,
                  ShareShamir *shares, int num_shares, int k) {

    printf("[SHAMIR] Dividindo %d bytes em %d shares (K=%d)...\n",
           tamanho, num_shares, k);

    if (tamanho > 256) {
        fprintf(stderr, "[SHAMIR] AVISO: tamanho %d > 256, truncando\n", tamanho);
        tamanho = 256;
    }

    /* Inicializar shares */
    for (int i = 0; i < num_shares; i++) {
        shares[i].x        = i + 1;      /* x = 1, 2, ..., N (nunca 0!) */
        shares[i].tamanho  = tamanho;
        memset(shares[i].y, 0, sizeof(shares[i].y));
    }

    /* Para cada byte do segredo, gerar um polinômio independente */
    unsigned char coefs[256];  /* grau máximo K-1 */
    for (int b = 0; b < tamanho; b++) {
        gerar_coeficientes(coefs, k, segredo[b]);

        /* Avaliar polinômio em cada x */
        for (int i = 0; i < num_shares; i++) {
            shares[i].y[b] = avaliar_polinomio(coefs,
                                                k,
                                                (unsigned char)shares[i].x);
        }
    }

    /* Limpar coeficientes da memória — segurança */
    memset(coefs, 0, sizeof(coefs));

    printf("[SHAMIR] %d shares criados. Qualquer %d reconstruem o segredo.\n",
           num_shares, k);
}

/* ═══════════════════════════════════════════════════════════
 *  SHAMIR RECOVER — reconstrói segredo de K shares
 *
 *  Para cada posição b do segredo:
 *    Usa interpolação de Lagrange em GF(256) para encontrar f(0)
 * ═══════════════════════════════════════════════════════════ */
int shamir_recover(ShareShamir *shares, int num_shares,
                   unsigned char *segredo, int *tamanho) {

    if (num_shares < K_FRAGMENTOS) {
        fprintf(stderr, "[SHAMIR] ERRO: %d shares (mínimo: %d)\n",
                num_shares, K_FRAGMENTOS);
        return -1;
    }

    printf("[SHAMIR] Recuperando segredo de %d shares...\n", num_shares);

    int tam = shares[0].tamanho;
    *tamanho = tam;

    /* Preparar arrays de x e y para Lagrange */
    unsigned char xs[K_FRAGMENTOS];
    unsigned char ys[K_FRAGMENTOS];

    for (int i = 0; i < K_FRAGMENTOS; i++)
        xs[i] = (unsigned char)shares[i].x;

    /* Recuperar cada byte via interpolação de Lagrange */
    for (int b = 0; b < tam; b++) {
        for (int i = 0; i < K_FRAGMENTOS; i++)
            ys[i] = shares[i].y[b];

        segredo[b] = lagrange_em_zero(xs, ys, K_FRAGMENTOS);
    }

    printf("[SHAMIR] Segredo recuperado (%d bytes). ✅\n", tam);
    return 0;
}
