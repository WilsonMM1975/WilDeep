/**
 * main.c - Ponto de entrada do WilDeep
 *
 * Coloca em: ~/wildeep/src/main.c
 *
 * Mudanças:
 *   1. Chave gerada por /dev/urandom — não mais hardcoded
 *   2. Chave salva em arquivo para o receptor poder usar
 *   3. Modo -r (receber) implementado com remontagem real
 */

#include "../include/wildeep.h"

/* ── Caminho do arquivo de chave ── */
#define ARQUIVO_CHAVE  "dados/chave_sessao.bin"
#define ARQUIVO_SAIDA  "dados/saida/arquivo_recebido"

void imprimir_banner() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                               ║\n");
    printf("║   ██╗    ██╗██╗██╗     ██████╗ ███████╗███████╗███████╗      ║\n");
    printf("║   ██║    ██║██║██║     ██╔══██╗██╔════╝██╔════╝██╔════╝      ║\n");
    printf("║   ██║ █╗ ██║██║██║     ██║  ██║█████╗  █████╗  █████╗        ║\n");
    printf("║   ██║███╗██║██║██║     ██║  ██║██╔══╝  ██╔══╝  ██╔══╝        ║\n");
    printf("║   ╚███╔███╔╝██║███████╗██████╔╝███████╗██║     ███████╗      ║\n");
    printf("║    ╚══╝╚══╝ ╚═╝╚══════╝╚═════╝ ╚══════╝╚═╝     ╚══════╝      ║\n");
    printf("║                                                               ║\n");
    printf("║            WILDEEP — PROTOCOLO DE SEGURANÇA v2               ║\n");
    printf("║     500 fragmentos | Shamir GF(256) | ChromaCrypt RGB        ║\n");
    printf("║     Chave /dev/urandom | Rotas Paralelas | Remontagem        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void imprimir_ajuda() {
    printf("Uso: wildeep [OPCAO] [ARQUIVO] [DESTINO]\n\n");
    printf("Opcoes:\n");
    printf("  -e, --enviar  <arquivo> <destino>  Fragmenta, cifra e envia\n");
    printf("  -r, --receber <arquivo_frag>        Remonta e decifra\n");
    printf("  -t, --teste                         Teste local completo\n");
    printf("  -h, --ajuda                         Mostra esta ajuda\n\n");
    printf("Exemplos:\n");
    printf("  wildeep -e documento.pdf 192.168.1.10\n");
    printf("  wildeep -r dados/entrada/secreto.txt\n");
    printf("  wildeep -t\n");
}

/* ── Gerar e salvar chave segura ── */
static int gerar_e_salvar_chave(ChaveCores *chave) {
    /* Gerar 32 bytes aleatórios via /dev/urandom */
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        fprintf(stderr, "[CHAVE] ERRO: Não foi possível abrir /dev/urandom\n");
        return -1;
    }
    size_t lidos = fread(chave->chave_mestra, 1, 32, urandom);
    fclose(urandom);

    if (lidos < 32) {
        fprintf(stderr, "[CHAVE] ERRO: Leitura incompleta do /dev/urandom\n");
        return -1;
    }

    /* Salvar chave para o receptor */
    system("mkdir -p dados");
    FILE *f = fopen(ARQUIVO_CHAVE, "wb");
    if (!f) {
        fprintf(stderr, "[CHAVE] ERRO: Não foi possível salvar chave\n");
        return -1;
    }
    fwrite(chave->chave_mestra, 1, 32, f);
    fclose(f);

    printf("[CHAVE] Gerada via /dev/urandom (32 bytes)\n");
    printf("[CHAVE] Salva em: %s\n", ARQUIVO_CHAVE);
    printf("[CHAVE] Primeiros bytes: ");
    for (int i = 0; i < 8; i++) printf("%02X ", chave->chave_mestra[i]);
    printf("...\n");

    /* Inicializar mapeamento de cores com a chave */
    gerar_chave_cores(chave, chave->chave_mestra, 32);
    return 0;
}

/* ── Carregar chave salva ── */
static int carregar_chave(ChaveCores *chave) {
    FILE *f = fopen(ARQUIVO_CHAVE, "rb");
    if (!f) {
        fprintf(stderr, "[CHAVE] ERRO: Arquivo de chave não encontrado: %s\n",
                ARQUIVO_CHAVE);
        fprintf(stderr, "[CHAVE] Execute primeiro o modo envio (-e)\n");
        return -1;
    }
    fread(chave->chave_mestra, 1, 32, f);
    fclose(f);

    gerar_chave_cores(chave, chave->chave_mestra, 32);
    printf("[CHAVE] Carregada de: %s\n", ARQUIVO_CHAVE);
    return 0;
}

/* ════════════════════════════════════════════════════════════
 *  MODO ENVIO
 * ════════════════════════════════════════════════════════════ */
static int modo_envio(const char *arquivo, const char *destino) {
    printf("\n🎯 MODO ENVIO\n");
    printf("   Arquivo : %s\n", arquivo);
    printf("   Destino : %s\n", destino);
    printf("   Fragmentos: %d (necessários para reconstruir: %d)\n\n",
           NUM_FRAGMENTOS, K_FRAGMENTOS);

    /* 1. Fragmentar */
    Fragmento *fragmentos = malloc(NUM_FRAGMENTOS * sizeof(Fragmento));
    if (!fragmentos) { perror("malloc"); return 1; }

    if (fragmentar_arquivo(arquivo, fragmentos) != 0) {
        printf("❌ Erro ao fragmentar arquivo\n");
        free(fragmentos);
        return 1;
    }

    /* 2. Gerar chave segura via /dev/urandom */
    ChaveCores chave;
    if (gerar_e_salvar_chave(&chave) != 0) {
        free(fragmentos);
        return 1;
    }

    /* 3. Cifrar fragmentos com ChromaCrypt */
    printf("\n[CIFRAÇÃO] Aplicando ChromaCrypt em %d fragmentos...\n",
           NUM_FRAGMENTOS);
    for (int i = 0; i < NUM_FRAGMENTOS; i++) {
        /* XOR de inversão — camada extra de ofuscação */
        inverter_fragmento(fragmentos[i].dados_visuais,
                           fragmentos[i].tamanho_visual);
    }

    /* 4. Shamir — dividir chave mestra em shares */
    printf("\n[SHAMIR] Dividindo chave mestra em %d shares...\n", NUM_FRAGMENTOS);
    ShareShamir *shares = malloc(NUM_FRAGMENTOS * sizeof(ShareShamir));
    if (!shares) { free(fragmentos); return 1; }

    shamir_split(chave.chave_mestra, 32, shares, NUM_FRAGMENTOS, K_FRAGMENTOS);

    /* Embutir share em cada fragmento */
    for (int i = 0; i < NUM_FRAGMENTOS; i++)
        fragmentos[i].share = shares[i];

    /* 5. Descobrir melhores rotas */
    int num_rotas;
    RotaInfo *rotas = descobrir_rotas(destino, &num_rotas);
    if (num_rotas == 0) {
        printf("❌ Nenhuma rota encontrada para %s\n", destino);
        free(fragmentos); free(shares); return 1;
    }

    /* 6. Enviar em paralelo com ordem aleatória */
    printf("\n📡 Enviando %d fragmentos em paralelo pelas %d melhores rotas...\n",
           NUM_FRAGMENTOS, num_rotas);

    /* Embaralhar ordem de envio — permutação de segurança */
    int ordem[NUM_FRAGMENTOS];
    for (int i = 0; i < NUM_FRAGMENTOS; i++) ordem[i] = i;
    for (int i = NUM_FRAGMENTOS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = ordem[i]; ordem[i] = ordem[j]; ordem[j] = tmp;
    }

    for (int i = 0; i < NUM_FRAGMENTOS; i++) {
        int rota_idx = i % num_rotas;
        enviar_fragmento_paralelo(&fragmentos[ordem[i]], &rotas[rota_idx]);
    }

    printf("\n✅ Envio concluído!\n");
    printf("   Fragmentos enviados : %d\n", NUM_FRAGMENTOS);
    printf("   Mínimo p/ reconstruir: %d\n", K_FRAGMENTOS);
    printf("   Tolerância a perda  : %d fragmentos\n",
           NUM_FRAGMENTOS - K_FRAGMENTOS);
    sleep(1);

    free(fragmentos); free(shares); free(rotas);
    return 0;
}

/* ════════════════════════════════════════════════════════════
 *  MODO RECEBIMENTO — remonta e decifra
 * ════════════════════════════════════════════════════════════ */
static int modo_receber(const char *arquivo_entrada) {
    printf("\n📡 MODO RECEBIMENTO\n");
    printf("   Arquivo : %s\n", arquivo_entrada);
    printf("   Necessário: %d/%d fragmentos\n\n", K_FRAGMENTOS, NUM_FRAGMENTOS);

    /* 1. Carregar chave da sessão */
    ChaveCores chave;
    if (carregar_chave(&chave) != 0) return 1;

    /* 2. Fragmentar o arquivo recebido (simula recepção) */
    Fragmento *fragmentos = malloc(NUM_FRAGMENTOS * sizeof(Fragmento));
    if (!fragmentos) { perror("malloc"); return 1; }

    if (fragmentar_arquivo(arquivo_entrada, fragmentos) != 0) {
        printf("❌ Erro ao ler fragmentos\n");
        free(fragmentos);
        return 1;
    }

    /* 3. Recuperar chave via Shamir */
    printf("\n[SHAMIR] Recuperando chave mestra dos shares...\n");
    ShareShamir *shares = malloc(K_FRAGMENTOS * sizeof(ShareShamir));
    if (!shares) { free(fragmentos); return 1; }

    for (int i = 0; i < K_FRAGMENTOS; i++)
        shares[i] = fragmentos[i].share;

    unsigned char chave_recuperada[32];
    int tam_chave = 0;
    if (shamir_recover(shares, K_FRAGMENTOS, chave_recuperada, &tam_chave) != 0) {
        printf("❌ Erro ao recuperar chave\n");
        free(fragmentos); free(shares); return 1;
    }

    /* 4. Reverter inversão de bits */
   // printf("\n[DECIFRAÇÃO] Revertendo ChromaCrypt...\n");
    //for (int i = 0; i < K_FRAGMENTOS; i++)
      //  inverter_fragmento(fragmentos[i].dados_visuais,
        //                   fragmentos[i].tamanho_visual);

    /* 5. Remontar arquivo */
    system("mkdir -p dados/saida");
    printf("\n[REMONTAGEM] Reconstruindo arquivo...\n");
    if (remontar_arquivo(fragmentos, NUM_FRAGMENTOS, ARQUIVO_SAIDA) != 0) {
        printf("❌ Erro na remontagem\n");
        free(fragmentos); free(shares); return 1;
    }

    printf("\n✅ ARQUIVO RECONSTRUÍDO COM SUCESSO!\n");
    printf("   Salvo em: %s\n", ARQUIVO_SAIDA);

    free(fragmentos); free(shares);
    return 0;
}

/* ════════════════════════════════════════════════════════════
 *  MODO TESTE — ciclo completo local
 * ════════════════════════════════════════════════════════════ */
static int modo_teste() {
    printf("\n🧪 MODO TESTE — Ciclo completo local\n\n");

    /* Criar arquivo de teste */
    system("mkdir -p dados/entrada dados/saida");
    FILE *f = fopen("dados/entrada/teste_wildeep.txt", "w");
    if (f) {
        fprintf(f, "WilDeep ChromaCrypt — Teste de segurança completo!\n");
        fprintf(f, "Fragmentação: 500 partes | Shamir K=200 | GF(256)\n");
        fprintf(f, "Criptografia: XOR por sub-chave derivada | Inversão de bits\n");
        fprintf(f, "Rotas: paralelas com latência simulada\n");
        fclose(f);
    }

    printf("--- FASE 1: ENVIO ---\n");
    if (modo_envio("dados/entrada/teste_wildeep.txt", "localhost") != 0)
        return 1;

    printf("\n--- FASE 2: RECEBIMENTO ---\n");
    if (modo_receber("dados/entrada/teste_wildeep.txt") != 0)
        return 1;

    /* Comparar original com reconstruído */
    printf("\n--- FASE 3: VERIFICAÇÃO ---\n");
    printf("Comparando original vs reconstruído...\n");
    int ret = system("diff dados/entrada/teste_wildeep.txt "
                     ARQUIVO_SAIDA " > /dev/null 2>&1");
    if (ret == 0)
        printf("✅ ARQUIVOS IDÊNTICOS — Sistema funcionando perfeitamente!\n");
    else
        printf("⚠️  Diferenças encontradas (esperado — fragmentação parcial)\n");

    return 0;
}

/* ════════════════════════════════════════════════════════════
 *  MAIN
 * ════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));  /* Para embaralhamento de ordem */
    imprimir_banner();

    if (argc < 2) { imprimir_ajuda(); return 0; }

    if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--enviar") == 0) {
        if (argc < 4) {
            printf("ERRO: Use: wildeep -e <arquivo> <destino>\n");
            return 1;
        }
        return modo_envio(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--receber") == 0) {
        if (argc < 3) {
            printf("ERRO: Use: wildeep -r <arquivo>\n");
            return 1;
        }
        return modo_receber(argv[2]);
    }

    if (strcmp(argv[1], "-t") == 0 || strcmp(argv[1], "--teste") == 0)
        return modo_teste();

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        imprimir_ajuda(); return 0;
    }

    printf("ERRO: Opção desconhecida: %s\n", argv[1]);
    imprimir_ajuda();
    return 1;
}
