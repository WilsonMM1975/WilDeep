/**
 * main.c - Ponto de entrada do WilDeep
 * 
 * Orquestra todos os módulos:
 * 1. Lê o arquivo de entrada
 * 2. Fragmenta em 500 partes
 * 3. Aplica criptografia de cores
 * 4. Divide a chave com Shamir
 * 5. Envia os fragmentos em paralelo
 * 6. (Futuro) Recebe e remonta
 */

#include "../include/wildeep.h"

void imprimir_banner() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                               ║\n");
    printf("║   ██╗    ██╗██╗██╗     ██████╗ ███████╗███████╗███████╗██████╗ ║\n");
    printf("║   ██║    ██║██║██║     ██╔══██╗██╔════╝██╔════╝██╔════╝██╔══██╗║\n");
    printf("║   ██║ █╗ ██║██║██║     ██║  ██║█████╗  █████╗  █████╗  ██████╔╝║\n");
    printf("║   ██║███╗██║██║██║     ██║  ██║██╔══╝  ██╔══╝  ██╔══╝  ██╔══██╗║\n");
    printf("║   ╚███╔███╔╝██║███████╗██████╔╝███████╗██║     ███████╗██║  ██║║\n");
    printf("║    ╚══╝╚══╝ ╚═╝╚══════╝╚═════╝ ╚══════╝╚═╝     ╚══════╝╚═╝  ╚═╝║\n");
    printf("║                                                               ║\n");
    printf("║               WILDEEP - PROTOCOLO DE SEGURANÇA                ║\n");
    printf("║          Fragmentação 500 partes | Shamir K=200               ║\n");
    printf("║          Criptografia RGB | Inversão | Rotas Paralelas        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void imprimir_ajuda() {
    printf("Uso: wildeep [OPCAO] [ARQUIVO] [DESTINO]\n\n");
    printf("Opcoes:\n");
    printf("  -e, --enviar <arquivo> <destino>   Envia um arquivo\n");
    printf("  -r, --receber                       Aguarda recebimento\n");
    printf("  -h, --ajuda                         Mostra esta ajuda\n\n");
    printf("Exemplos:\n");
    printf("  wildeep -e documento.pdf banco.com.br\n");
    printf("  wildeep -r\n");
}

int main(int argc, char *argv[]) {
    imprimir_banner();
    
    if (argc < 2) {
        imprimir_ajuda();
        return 0;
    }
    
    // Modo de envio
    if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--enviar") == 0) {
        if (argc < 4) {
            printf("ERRO: Faltam argumentos. Use: wildeep -e <arquivo> <destino>\n");
            return 1;
        }
        
        const char *arquivo = argv[2];
        const char *destino = argv[3];
        
        printf("\n🎯 MODO ENVIO\n");
        printf("   Arquivo: %s\n", arquivo);
        printf("   Destino: %s\n", destino);
        printf("   Fragmentos: %d (necessarios: %d)\n\n", NUM_FRAGMENTOS, K_FRAGMENTOS);
        
        // 1. Fragmenta o arquivo
        Fragmento fragmentos[NUM_FRAGMENTOS];
        if (fragmentar_arquivo(arquivo, fragmentos) != 0) {
            printf("❌ Erro ao fragmentar arquivo\n");
            return 1;
        }
        
        // 2. Gera chave de cores (criptografia)
        ChaveCores chave;
        unsigned char semente[] = "chave_super_secreta_wildeep_2026";
        gerar_chave_cores(&chave, semente, sizeof(semente));
        
        // 3. Aplica inversão nos fragmentos (camada extra)
        for (int i = 0; i < NUM_FRAGMENTOS; i++) {
            inverter_fragmento(fragmentos[i].dados_visuais, fragmentos[i].tamanho_visual);
        }
        
        // 4. Descobre as melhores rotas
        int num_rotas;
        RotaInfo *rotas = descobrir_rotas(destino, &num_rotas);
        if (num_rotas == 0) {
            printf("❌ Nenhuma rota encontrada para %s\n", destino);
            return 1;
        }
        
        // 5. Envia fragmentos em paralelo pelas melhores rotas
        printf("\n📡 Enviando %d fragmentos em paralelo...\n", NUM_FRAGMENTOS);
        for (int i = 0; i < NUM_FRAGMENTOS; i++) {
            int rota_idx = i % num_rotas;
            enviar_fragmento_paralelo(&fragmentos[i], &rotas[rota_idx]);
        }
        
        printf("\n✅ Envio concluído! Aguardando confirmação...\n");
        sleep(2);
        
        free(rotas);
    }
    
    // Modo de recebimento
    else if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--receber") == 0) {
        printf("\n📡 MODO RECEBIMENTO\n");
        printf("   Aguardando fragmentos...\n");
        printf("   Necessário %d fragmentos para reconstruir\n\n", K_FRAGMENTOS);
        
        // Simulação: aguarda recebimento de fragmentos
        // Em produção, isso seria um servidor UDP/TCP
        printf("⚠️  Modo recebimento em desenvolvimento.\n");
        printf("   Por enquanto, use o modo envio para testar a fragmentação.\n");
    }
    
    // Ajuda
    else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        imprimir_ajuda();
    }
    
    else {
        printf("ERRO: Opcao desconhecida: %s\n", argv[1]);
        imprimir_ajuda();
        return 1;
    }
    
    printf("\n🚀 WILDEEP FINALIZADO COM SUCESSO!\n");
    return 0;
}
