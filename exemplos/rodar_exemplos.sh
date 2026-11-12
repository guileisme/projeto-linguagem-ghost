#!/usr/bin/env bash
# =====================================================================
#  rodar_exemplos.sh — executa os exemplos da linguagem Ghost e mostra
#  o comando, a saida e o codigo de saida de cada um.
#
#  Uso:  make exemplos     (ou)     ./exemplos/rodar_exemplos.sh
#
#  Cada exemplo roda numa pasta temporaria, que e apagada no final:
#  nada do seu computador e alterado.
# =====================================================================
cd "$(dirname "$0")/.."
GHOST="$PWD/ghost"
EX="$PWD/exemplos"
TESTES="$PWD/tests/suite"

[ -x "$GHOST" ] || { echo "Compile primeiro com: make"; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

titulo() {
    echo
    echo "================================================================"
    echo " $1"
    echo "================================================================"
}

# roda <arquivo-relativo-ao-repo> [argumentos do script...]
# OPC="--tokens" roda ...  passa uma opcao ao interpretador (ex.: so a fase lexica)
# SEM_STDOUT=1 roda ...     esconde a saida padrao (mostra so as mensagens de erro)
roda() {
    local arq="$1"; shift
    echo "\$ ghost ${OPC:+$OPC }$arq${*:+ $*}"
    if [ -n "$SEM_STDOUT" ]; then
        ( cd "$TMP" && "$GHOST" $OPC "$PWD_REPO/$arq" "$@" >/dev/null )
    else
        ( cd "$TMP" && "$GHOST" $OPC "$PWD_REPO/$arq" "$@" )
    fi
    echo "[codigo de saida: $?]"
}
PWD_REPO="$PWD"

titulo "1. Diagnostico do sistema (variavel de ambiente, shell como expressao)"
roda exemplos/01_diagnostico.ghost

titulo "2. Backup de diretorio (arg, exists, when/otherwise, signal, eject)"
mkdir "$TMP/projeto" && echo "dados" > "$TMP/projeto/arquivo.txt"
roda exemplos/02_backup.ghost projeto
echo "backup criado: $(cd "$TMP" && ls -d backup_*)"
echo
echo "--- diretorio que nao existe: o script termina com eject 1 ---"
roda exemplos/02_backup.ghost pasta_inexistente

titulo "3. Limpeza de temporarios (route percorre as linhas do shell)"
( cd "$TMP" && touch a.tmp b.tmp c.tmp manter.txt )
roda exemplos/03_limpeza.ghost
echo "arquivos que sobraram: $(cd "$TMP" && ls | grep -v '^backup_\|^projeto$' | tr '\n' ' ')"

titulo "4. Espera por arquivo de trava (protocol, cycle, cloak) — sem a trava"
roda exemplos/04_espera_lock.ghost
echo
echo "--- agora com o arquivo app.lock ja existindo ---"
( cd "$TMP" && touch app.lock )
roda exemplos/04_espera_lock.ghost

titulo "5. Protocolos, recursao, sweep, cloak e bypass"
roda exemplos/05_protocolos_e_lacos.ghost

titulo "6. Programas invalidos: o erro e detectado antes de executar"
echo "--- erro lexico (so a fase lexica, com --tokens) ---"
OPC="--tokens" SEM_STDOUT=1 roda tests/suite/erros_lexicos/04_caractere_invalido.ghost
echo
echo "--- erro sintatico ---"
roda tests/suite/erros_sintaticos/03_falta_trigger.ghost
echo
echo "--- erro semantico ---"
roda tests/suite/erros_semanticos/03_tipo_incompativel.ghost
echo
echo "--- erro de execucao (divisao por zero) ---"
roda tests/suite/erros_semanticos/12_divisao_por_zero.ghost
