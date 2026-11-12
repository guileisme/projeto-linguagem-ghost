#!/usr/bin/env bash
# =====================================================================
#  rodar_testes.sh — Suite de testes da linguagem Ghost
#  (suite escrita por Deborah, adaptada ao interpretador unico ./ghost)
#
#  tests/suite/validos/            devem ser ACEITOS (ghost --check)
#  tests/suite/erros_lexicos/      devem ser REJEITADOS na fase lexica (ghost --tokens)
#  tests/suite/erros_sintaticos/   lexico aceita, parser REJEITA (ghost --check)
#  tests/suite/erros_semanticos/   sintaxe correta, erro de tipo/escopo/execucao.
#                                  Se o erro ainda nao for detectado: PENDENTE.
#
#  Cada arquivo invalido comeca com:  // ESPERADO: linha N, coluna C: <mensagem>
#  O teste passa se o erro for da categoria certa e, quando o ESPERADO
#  informa a linha, se a linha do erro for a mesma.
# =====================================================================
cd "$(dirname "$0")"

GHOST=./ghost
SUITE=tests/suite

VERDE=$'\e[32m'; VERMELHO=$'\e[31m'; AMARELO=$'\e[33m'; NORMAL=$'\e[0m'
[ -t 1 ] || { VERDE=""; VERMELHO=""; AMARELO=""; NORMAL=""; }

ok=0; falhas=0; pendentes=0
ERR=$(mktemp)

esperado()        { sed -n '1s|^// ESPERADO: ||p' "$1" | tr -d '\r'; }
linha_esperada()  { esperado "$1" | grep -o 'linha [0-9]*' | head -1; }

passou()   { echo "  ${VERDE}[PASSOU]${NORMAL}   $1"; ok=$((ok+1)); }
falhou()   { echo "  ${VERMELHO}[FALHOU]${NORMAL}   $1 — $2"; falhas=$((falhas+1)); }
pendente() { echo "  ${AMARELO}[PENDENTE]${NORMAL} $1"; pendentes=$((pendentes+1)); }
mostra()   { sed 's/^/             /' "$ERR"; }

# confere ARQUIVO CATEGORIA: o erro emitido e da categoria e (se o ESPERADO
# indica a linha) aponta a mesma linha.
confere() {
    local l; l=$(linha_esperada "$1")
    grep -qF -- "erro $2" "$ERR" || return 1
    [ -z "$l" ] || grep -qF -- "na $l," "$ERR"
}

# ---------------------------------------------------------------------
echo "== Programas validos (devem ser aceitos) =="
for f in "$SUITE"/validos/*.ghost; do
    if timeout 10 $GHOST --check "$f" >/dev/null 2>"$ERR"; then
        passou "$f"
    else
        falhou "$f" "deveria ser aceito"; mostra
    fi
done

# ---------------------------------------------------------------------
echo; echo "== Erros lexicos (devem ser rejeitados pelo lexico) =="
for f in "$SUITE"/erros_lexicos/*.ghost; do
    if timeout 10 $GHOST --tokens "$f" >/dev/null 2>"$ERR"; then
        falhou "$f" "deveria gerar erro lexico"
    elif ! confere "$f" lexico; then
        falhou "$f" "categoria ou linha diferente da esperada"
        echo "             esperado: $(esperado "$f")"; mostra
    else
        passou "$f"; mostra
    fi
done

# ---------------------------------------------------------------------
echo; echo "== Erros sintaticos (lexico aceita, parser rejeita) =="
for f in "$SUITE"/erros_sintaticos/*.ghost; do
    if ! timeout 10 $GHOST --tokens "$f" >/dev/null 2>"$ERR"; then
        falhou "$f" "tem erro lexico, nao isola a fase sintatica"; mostra
    elif timeout 10 $GHOST --check "$f" >/dev/null 2>"$ERR"; then
        falhou "$f" "deveria gerar erro sintatico"
    elif ! confere "$f" sintatico; then
        falhou "$f" "categoria ou linha diferente da esperada"
        echo "             esperado: $(esperado "$f")"; mostra
    else
        passou "$f"; mostra
    fi
done

# ---------------------------------------------------------------------
echo; echo "== Erros semanticos / de execucao =="
for f in "$SUITE"/erros_semanticos/*.ghost; do
    esp=$(esperado "$f")
    if [[ "$esp" == *"divisão aritmética"* ]]; then
        cat=execucao;  cmd=("$GHOST" "$f")            # este so aparece ao executar
    else
        cat=semantico; cmd=("$GHOST" --check "$f")
    fi
    if timeout 10 "${cmd[@]}" >/dev/null 2>"$ERR"; then
        pendente "$f"; echo "             esperado: $esp"
    elif confere "$f" "$cat"; then
        passou "$f"; mostra
    else
        falhou "$f" "erro de categoria ou linha diferente da esperada"
        echo "             esperado: $esp"; mostra
    fi
done

rm -f "$ERR"
echo
echo "Resumo: $ok passou, $falhas falhou, $pendentes pendente(s)."
[ $falhas -eq 0 ]