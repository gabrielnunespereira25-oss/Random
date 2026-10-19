/*
 * RANDOM ALPHA
 * MAPA - PSP / PSPSDK
 *
 * SOMENTE O MAPA
 *
 * 6 ZONAS:
 * 0 - Caverna do Labirinto
 * 1 - Floresta Esquecida
 * 2 - Cidade Abandonada
 * 3 - Castelo Caído
 * 4 - Torre Antiga
 * 5 - Arena Final
 *
 * ESTILO:
 * Preto e branco
 * Superfície, céu e subterrâneo
 * Labirinto vertical
 * Corredores conectados
 * Salas secretas e bifurcações
 * Indicador de zona
 */

#define ZONA_CAVERNA  0
#define ZONA_FLORESTA 1
#define ZONA_CIDADE   2
#define ZONA_CASTELO  3
#define ZONA_TORRE    4
#define ZONA_ARENA    5

#define MAPA_LARGURA  24000
#define MAPA_ALTURA   3200

/*
 * A distribuição do mapa deve contemplar:
 *
 * CAVERNA:
 * Túneis estreitos, labirinto vertical,
 * bifurcações e salas secretas.
 *
 * FLORESTA:
 * Terreno aberto, árvores, plataformas
 * elevadas e passagens subterrâneas.
 *
 * CIDADE:
 * Ruas, edifícios, telhados e túneis.
 *
 * CASTELO:
 * Salões, torres partidas e masmorras.
 *
 * TORRE:
 * Percurso vertical, alturas extremas
 * e acessos subterrâneos.
 *
 * ARENA FINAL:
 * Região isolada, conectada por uma
 * passagem difícil de encontrar.
 *
 * INDICADOR:
 * Atualizar o nome da zona conforme
 * a posição do jogador no mapa.
 *
 * IMPORTANTE:
 * Todas as zonas usam preto e branco.
 * Os corredores devem ter aberturas reais
 * para permitir a passagem entre salas.
 *
 * A meta de 5 minutos ou mais andando
 * depende da velocidade do personagem
 * e do percurso efetivamente construído.
 */
