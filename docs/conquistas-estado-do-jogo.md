# Conquistas: onde o runtime nativo lê o estado do jogo

Este documento acompanha `native/runtime/msr/achievements.cpp`. Ele lista o que as conquistas leem da memória do jogo, onde fica cada dado, como foi encontrado e quanto dá para confiar. O mapeamento foi feito só por análise estática, sem o dump do jogo. Tudo que não foi confirmado por código que já funciona no projeto está marcado **NÃO VERIFICADO** e traz um passo de validação no Steam Deck.

Versão analisada: a do `cheats.h`/`cheats.cpp` do repositório (release USA, que é a que o projeto suporta).

## Como o inventário é lido

Fonte: `src/msr/cheats.h`, comentários das linhas 110–121, e `native/runtime/msr/cheats.cpp`, funções `SamusEntity`, `Inventory`, `ItemAddress` e `msr_item`.

- O objeto do jogo é `rd32(0x76F314)` (`C::GamePointer`).
- Os handles do jogador ficam em `game + 0xFC`, `+0xF0` e `+0xF4`. A entidade da Samus está em `rd32(handle + 8)` e o inventário em `rd32(entity + 0x18C)`.
- O inventário é uma tabela hash. Os buckets ficam em `+0x40`, o tamanho em `+0x44` e o número de buckets em `+0x48`. Cada nó tem o hash em `+0`, o ponteiro para o valor (`float`) em `+4` e o próximo nó em `+0x10`.
- Cada item é identificado por uma **global-chave**: um endereço fixo que aponta para um objeto-nome. O primeiro campo desse objeto é o texto (`"ITEM_WEAPON_ICE_BEAM"`) e `+0x10` é o hash.
- `msr_item(chave, "NOME")` confere se o texto bate com o nome. Se não bater, devolve `-1`, o que protege contra endereço errado ou versão diferente do jogo. Se bater, devolve o valor (`1` para upgrade obtido, ou uma contagem), `0` se o item não está no inventário e `-1` fora de jogo.

**Nível de certeza do mecanismo:** verificado. Os cheats de mísseis, a cor do feixe no compositor e o contador de Metroids do Discord já usam esse caminho.

## Tabela de estado

| Dado | Como lê | Tipo | Como achei | Certeza |
|---|---|---|---|---|
| Metroids derrotados | `msr_item(0x7086BC, "ITEM_METROID_COUNT")` | float | `discord.cpp`, `msr_discord_tick` | Verificado (código existente) |
| Total de Metroids | `msr_item(0x7086C0, "ITEM_METROID_TOTAL_COUNT")` | float | `discord.cpp` | Verificado |
| Metroids restantes | `max(0, total − derrotados)` se `total > 0` | int | `discord.cpp` e a barra da tela de baixo (`compositor.cpp`) | Verificado |
| DNA de Metroid | `msr_item(0x7086B8, "ITEM_ADN")` | float | `compositor.cpp` | Verificado (não usado na lista) |
| Ice Beam | `0x708718`, `ITEM_WEAPON_ICE_BEAM` | float | `compositor.cpp` | Verificado |
| Grapple Beam | `0x708714`, `ITEM_WEAPON_GRAPPLE_BEAM` | float | `compositor.cpp` | Verificado |
| Plasma Beam | `0x708724`, `ITEM_WEAPON_PLASMA_BEAM` | float | `compositor.cpp` | Verificado |
| Spazer Beam | `0x708730`, `ITEM_WEAPON_SPAZER_BEAM` | float | `compositor.cpp` | Verificado |
| Wave Beam | `0x70873C`, `ITEM_WEAPON_WAVE_BEAM` | float | `compositor.cpp` | Verificado |
| Mísseis máx. | `0x708720`, `ITEM_WEAPON_MISSILE_MAX` | float | `cheats.h`, refills | Verificado |
| Super Missiles máx. | `0x708738`, `ITEM_WEAPON_SUPER_MISSILE_MAX` (`> 0` = já tem) | float | `cheats.h`; `compositor.cpp` usa `== 0` como "não tem" | Verificado |
| Power Bombs máx. | `0x70872C`, `ITEM_WEAPON_POWER_BOMB_MAX` | float | `cheats.h` | Verificado |
| Área atual | Pasta do mapa do pacote lido (`packs/maps/<s>/<s>.pkg`), avisada por `layered_fs.cpp` | string | `discord.cpp` (`msr_discord_scenario`, tabela `Scenarios`) | Verificado |
| Energia atual/máx. | `CPlayerLifeComponent` (vtable `0x6D4F98`, máx. em `+0x34`, atual em `+0x38`), achado por varredura do heap | float | `cheats.h`, linha 153 | Verificado, mas **não usado**: a varredura é cara, e o inventário tem um item para isso (abaixo) |
| Bombas | `ITEM_WEAPON_BOMB` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Spider Ball | `ITEM_SPIDER_BALL` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Spring Ball | `ITEM_SPRING_BALL` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Varia Suit | `ITEM_VARIA_SUIT` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Gravity Suit | `ITEM_GRAVITY_SUIT` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Hi-Jump | `ITEM_HIGH_JUMP_BOOTS` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Space Jump | `ITEM_SPACE_JUMP` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Screw Attack | `ITEM_SCREW_ATTACK` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Energy Tanks | `ITEM_MAX_LIFE` (energia máxima: 99 + 100 por tanque → 1º tanque = 199, os 10 = 1099) | float | Nome confirmado no dump. Não existe item próprio de contagem (`ITEM_ENERGY_TANKS` não está no jogo) | Nome ✅; base 99, +100 e total de 10 **NÃO VERIFICADOS** |
| Scan Pulse | `ITEM_SPECIAL_ENERGY_SCANNING_PULSE` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Lightning Armor | `ITEM_SPECIAL_ENERGY_ENERGY_SHIELD` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Beam Burst | `ITEM_SPECIAL_ENERGY_ENERGY_WAVE` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Phase Drift | `ITEM_SPECIAL_ENERGY_PHASE_DISPLACEMENT` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA** |
| Bebê Metroid | `ITEM_BABY_HATCHLING` | float | Nome confirmado no dump | Nome ✅; chave **NÃO VERIFICADA**; **NÃO VERIFICADO** que vire ≥ 1 quando o bebê nasce (pode ser outro valor ou só no fim) |
| Volta à superfície depois da Rainha | Área `s110_surfaceb` | string | Está na tabela `Scenarios` do `discord.cpp` como segunda superfície | **NÃO VERIFICADO** que só se chega lá depois da Rainha |

### Como os nomes foram confirmados

Em 26/11/2026, os identificadores `ITEM_*` foram extraídos do dump USA do jogo com `strings`, numa checagem só de leitura feita pelo dono do fork. Todos os nomes da lista estão no jogo, menos `ITEM_ENERGY_TANKS`, que não existe. Os Energy Tanks passaram a usar `ITEM_MAX_LIFE`.

Os `ITEM_ACQUIRED_*` que também aparecem no dump são textos das mensagens de item obtido, não itens do inventário.

Isso confirma que os **nomes** existem, mas não que a varredura encontra as **chaves** deles na memória. Essa parte continua **NÃO VERIFICADA** até a primeira linha `achievements: the game's item names: ...` aparecer no log.

### Como as chaves que faltam são encontradas

As chaves confirmadas ficam juntas, de 4 em 4 bytes, entre `0x7086B8` e `0x70873C`. Na primeira vez que a Samus está em jogo, o `achievements.cpp` varre de `0x708000` a `0x70A000`. Em cada palavra que aponta para um objeto-nome legível, ele lê o texto e guarda a chave dos nomes que a lista usa. **NÃO VERIFICADO:** que todas as chaves estejam nessa faixa.

Essa busca é segura:
- só lê;
- testa cada endereço com `k_region_state` antes;
- usa `msr_item`, que confere o nome de novo a cada leitura.

O log mostra o resultado:

```
achievements: the game's item names: ITEM_ADN ITEM_METROID_COUNT ...
achievements: no item ITEM_XYZ in this game: its achievements stay locked
```

Se um nome estiver errado, troque-o num `achievements.txt` ao lado do `settings.ini`. O formato é o mesmo de `native/runtime/msr/achievements.txt`, e não precisa recompilar.

## Thread e momento da leitura

- `msr_achievements_tick()` é chamada em `main.c`, logo depois de `msr_discord_tick()`: a cada 2 s, na thread principal, depois de `video_frame` e da espera do próximo tick.
- O `msr_item` é documentado como "main thread, as the cheats", e o `msr_cheats_frame` roda na mesma thread. Então a leitura não concorre com os cheats.
- O jogo roda na thread dele. A leitura de um `float` alinhado é atômica, e o pior caso é ver um valor um tick atrasado, que é inofensivo porque as conquistas só dependem de valores que crescem.
- A área chega pela thread do sistema de arquivos (`layered_fs.cpp`) e é guardada com mutex.
- A notificação é enfileirada sob mutex e desenhada pelo overlay.

## Cheats

`MSR::Cheats::enabled` (`src/msr/cheats.h`). Se `InfiniteEnergy`, `InfiniteMissiles` ou `InfiniteAeion` estiver ligado, que são as três opções da aba Cheats, nada desbloqueia. O log mostra `achievements: a cheat is on: nothing unlocks while it is`.

Galerias, Fusion Mode, Hard Mode e Fusion Suit só liberam conteúdo ou mudam a aparência, por isso não bloqueiam.

## Mudanças em relação ao Metroid II

| Metroid II (RA) | No remake | Por quê |
|---|---|---|
| Bombas, Ice, Spider Ball, Varia, Wave, Hi-Jump, Spring Ball, Space Jump, Spazer, Plasma, Screw Attack, 1º Energy Tank | Mantidas | Todos existem no remake |
| — | Novas: Grapple Beam, Gravity Suit, Super Missiles, Power Bombs e as 4 habilidades Aeion | Itens que o Metroid II não tem |
| Marcos de mísseis | 50, 100 e 175 (o máximo do remake) | O máximo é diferente |
| Todos os Energy Tanks | Energia máxima ≥ 1099 (`ITEM_MAX_LIFE`) | O jogo não tem um item que conte tanques. **NÃO VERIFICADO:** que a energia comece em 99, cada tanque some 100 e o total seja 10 |
| Gamma, Zeta, Omega | Trocados por 10 e 25 Metroids derrotados, e por "contador de Metroids em zero" | O inventário conta os Metroids derrotados, não o tipo. Volto a usar os nomes do Metroid II quando aparecer uma flag por tipo (blackboard ou Lua do jogo) |
| Rainha | "Voltar à superfície depois da Rainha" (área `s110_surfaceb`) | Não há flag de chefe conhecida |
| Bebê Metroid (o final clássico) | `baby_metroid`, "Imprinted" (`ITEM_BABY_HATCHLING ≥ 1`) | O item existe no inventário do remake |

## Como validar no Steam Deck

Prepare antes:
1. Ative **Achievements** e **Achievement notices** na aba Game (F1 ou Select).
2. Deixe os cheats desligados.
3. Depois de jogar, abra o log do jogo e procure as linhas `achievements:`.

| O que validar | Onde no jogo | Resultado esperado |
|---|---|---|
| Lista e progresso carregados | Ao abrir o jogo | `achievements: 30 in the built-in list, 0 unlocked (on)` |
| Varredura das chaves | Primeiros 2 s jogando, em qualquer save | Linha `the game's item names: ...`. Anote os nomes `no item ...` e mande para corrigirmos a lista |
| Mecanismo verificado | Save novo, no começo do jogo | Nada desbloqueia nos primeiros segundos. Se algo desbloquear, a leitura está errada |
| `first_metroid` | Primeiro Metroid Alpha (Área 1) | Toast "First Contact" |
| `ice_beam` | Ao pegar o Ice Beam | Toast "Cold Front" |
| `bomb` **(NÃO VERIFICADO)** | Morph Ball Bombs (Área 1) | Toast "Bombs Away". Se não aparecer, procure no log o nome certo da bomba |
| `scan_pulse` **(NÃO VERIFICADO)** | Primeira habilidade Aeion (Scan Pulse) | Toast "Lay of the Land" |
| `energy_tank` **(NÃO VERIFICADO)** | Primeiro Energy Tank | Toast "Reserve Power". Se aparecer antes do tanque ou não aparecer, a energia inicial não é 99: anote a energia máxima do HUD antes e depois do tanque |
| `missiles_50` | Ao chegar a 50 mísseis de capacidade | Toast "Stocked Up" |
| `baby_metroid` **(NÃO VERIFICADO)** | Quando o ovo choca, depois da Rainha | Toast "Imprinted". Se não aparecer, ou aparecer antes, anote o momento: o valor do item pode significar outra coisa |
| Persistência | Desbloqueie algo, feche o jogo pelo menu e depois mate o processo | `user/msr/achievements.ini` tem `[unlocked]` com o id e a data, e a contagem continua ao reabrir |
| Bloqueio por cheat | Ligue "Unlimited missiles" e pegue um item novo | Nada desbloqueia, e o log diz `a cheat is on`. Desligue: o item já obtido desbloqueia no tick seguinte |
| Notificações desligadas | Desligue "Achievement notices" | Desbloqueia (aparece no log e no .ini), mas sem toast |
| Conquistas desligadas | Padrão | Nada é lido e nada é gravado |
