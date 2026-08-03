#!/usr/bin/env bash
# ============================================================================
#  Bateria de fugas para ft_irc  --  uso:  ./test_leaks.sh [puerto]
#
#  PARTE A  memoria y descriptores al salir, bajo valgrind
#  PARTE B  descriptores en caliente, contando /proc/<pid>/fd entre tandas
#
#  La parte B es la que pilla un fd que se escapa durante la ejecucion aunque
#  el cierre final lo tape: un servidor que filtra un fd por conexion aguanta
#  un rato y luego deja de aceptar a nadie.
# ============================================================================

PORT=${1:-6877}
PASSWORD=test
DIR=$(mktemp -d)

R=$'\e[31m'; G=$'\e[32m'; Y=$'\e[33m'; B=$'\e[1m'; N=$'\e[0m'

SRV_PID=""
cleanup() {
	[ -n "$SRV_PID" ] && kill -9 "$SRV_PID" 2>/dev/null
	pkill -f "ircserv $PORT" 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup EXIT

title() { printf "\n${B}%s${N}\n" "$1"; }
info()  { printf "  %s\n" "$1"; }
ok()    { printf "  ${G}ok${N}    %s\n" "$1"; }
bad()   { printf "  ${R}FUGA${N}  %s\n" "$1"; }
warn()  { printf "  ${Y}aviso${N} %s\n" "$1"; }

# ------------------------------------------------------------ tandas de uso --
# manda un guion de comandos y espera a que el servidor cierre (QUIT) o a que
# venza el timeout (desconexion abrupta)
session() {           # $1 = segundos de timeout, resto por stdin
	timeout "$1" nc localhost "$PORT" > /dev/null 2>&1
}

# ejercita todos los caminos que crean o destruyen estado
workout() {
	local tag=$1

	# --- registro completo, canal, modos, y salida limpia con QUIT
	{ printf 'PASS %s\r\nNICK a_%s\r\nUSER au 0 * :A\r\n' "$PASSWORD" "$tag"
	  printf 'JOIN #c_%s\r\nTOPIC #c_%s :hola\r\n' "$tag" "$tag"
	  printf 'MODE #c_%s +itkl clave 5\r\n' "$tag"
	  printf 'MODE #c_%s\r\nWHO #c_%s\r\n' "$tag" "$tag"
	  printf 'MODE #c_%s -ikl\r\n' "$tag"
	  printf 'PART #c_%s\r\nQUIT :adios\r\n' "$tag"
	  sleep 0.4
	} | session 3

	# --- dos clientes: kick, invite y canal que se queda vacio
	{ printf 'PASS %s\r\nNICK b_%s\r\nUSER bu 0 * :B\r\nJOIN #k_%s\r\n' "$PASSWORD" "$tag" "$tag"
	  sleep 0.8
	  printf 'MODE #k_%s +i\r\nINVITE c_%s #k_%s\r\n' "$tag" "$tag" "$tag"
	  sleep 0.6
	  printf 'KICK #k_%s c_%s :fuera\r\n' "$tag" "$tag"
	  printf 'QUIT :adios\r\n'
	  sleep 0.3
	} | session 4 &
	local bg=$!
	sleep 0.5
	{ printf 'PASS %s\r\nNICK c_%s\r\nUSER cu 0 * :C\r\n' "$PASSWORD" "$tag"
	  sleep 0.6
	  printf 'JOIN #k_%s\r\nPRIVMSG #k_%s :hola\r\nNOTICE #k_%s :aviso\r\n' "$tag" "$tag" "$tag"
	  sleep 1.2
	} | session 4
	# wait sobre ESE trabajo, no sobre todos: el servidor tambien esta en segundo
	# plano y un wait sin argumentos se quedaria esperandolo para siempre
	wait "$bg" 2>/dev/null

	# --- desconexion abrupta: se registra, entra y le cortan el socket
	{ printf 'PASS %s\r\nNICK d_%s\r\nUSER du 0 * :D\r\nJOIN #c_%s\r\n' "$PASSWORD" "$tag" "$tag"
	  sleep 5
	} | session 1

	# --- conecta y se va sin registrarse siquiera
	printf 'x' | session 1

	# --- password incorrecta
	{ printf 'PASS mala\r\nNICK e_%s\r\nUSER eu 0 * :E\r\n' "$tag"; sleep 0.3; } | session 2

	# --- comandos invalidos y entradas que rompen
	{ printf 'PASS %s\r\nNICK f_%s\r\nUSER fu 0 * :F\r\n' "$PASSWORD" "$tag"
	  printf 'MODE :\r\nMODE #nada\r\nINVENTADO x\r\nJOIN\r\nKICK\r\nMODE #c_%s +l abc\r\n' "$tag"
	  printf 'QUIT\r\n'; sleep 0.3
	} | session 2

	# --- inundacion sin \n: dispara el corte por MAX_PENDING_LINE
	{ printf 'PASS %s\r\nNICK g_%s\r\nUSER gu 0 * :G\r\n' "$PASSWORD" "$tag"
	  sleep 0.2
	  head -c 8000 < /dev/zero | tr '\0' 'x'
	  sleep 0.5
	} | session 3
}

# ============================================================ PARTE A: valgrind
parte_a() {
	title "PARTE A  --  memoria y descriptores al salir (valgrind)"

	valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
	         --errors-for-leak-kinds=definite,indirect \
	         --log-file="$DIR/valgrind.log" \
	         ./ircserv "$PORT" "$PASSWORD" > "$DIR/srv.log" 2>&1 &
	SRV_PID=$!
	info "arrancando bajo valgrind (tarda, va ~20x mas lento)..."
	sleep 4

	if ! kill -0 "$SRV_PID" 2>/dev/null; then
		bad "el servidor no arranco bajo valgrind"
		cat "$DIR/valgrind.log"
		return 1
	fi

	workout va1
	workout va2

	info "cerrando con SIGINT..."
	kill -INT "$SRV_PID" 2>/dev/null
	for _ in $(seq 1 40); do
		kill -0 "$SRV_PID" 2>/dev/null || break
		sleep 0.5
	done
	if kill -0 "$SRV_PID" 2>/dev/null; then
		warn "no salio con SIGINT, se fuerza (el informe de fugas puede salir incompleto)"
		kill -9 "$SRV_PID" 2>/dev/null
	fi
	SRV_PID=""
	sleep 1

	printf "\n${B}  --- resumen de memoria ---${N}\n"
	sed -n '/HEAP SUMMARY/,/suppressed/p' "$DIR/valgrind.log" | sed 's/^==[0-9]*== */  /'

	printf "\n${B}  --- descriptores abiertos al salir ---${N}\n"
	if grep -q "FILE DESCRIPTORS" "$DIR/valgrind.log"; then
		sed -n '/FILE DESCRIPTORS/,/^==[0-9]*== *$/p' "$DIR/valgrind.log" | sed 's/^==[0-9]*== */  /' | head -40
	else
		info "valgrind no reporto la seccion de descriptores"
	fi

	printf "\n${B}  --- errores ---${N}\n"
	grep -E "ERROR SUMMARY|definitely lost|indirectly lost|possibly lost|still reachable" \
	     "$DIR/valgrind.log" | sed 's/^==[0-9]*== */  /'

	# fuera del repo a proposito, para no dejar basura sin trackear
	cp "$DIR/valgrind.log" /tmp/ft_irc_valgrind.log 2>/dev/null
	info ""
	info "log completo guardado en /tmp/ft_irc_valgrind.log"
}

# ================================================= PARTE B: fds en caliente
fdcount() { ls /proc/"$SRV_PID"/fd 2>/dev/null | wc -l; }

parte_b() {
	title "PARTE B  --  descriptores en caliente (sin valgrind)"

	./ircserv "$PORT" "$PASSWORD" > "$DIR/srv2.log" 2>&1 &
	SRV_PID=$!
	sleep 0.6
	if ! kill -0 "$SRV_PID" 2>/dev/null; then
		bad "el servidor no arranco"
		return 1
	fi

	local base
	base=$(fdcount)
	info "descriptores en reposo tras arrancar: $base"
	info ""

	local prev=$base
	for round in 1 2 3; do
		workout "fd$round" > /dev/null 2>&1
		sleep 1.2
		local now
		now=$(fdcount)
		local delta=$((now - prev))
		if [ "$delta" -eq 0 ]; then
			ok "tanda $round: $now descriptores (sin cambio)"
		else
			bad "tanda $round: $now descriptores (${delta:+$delta} respecto a la anterior)"
		fi
		prev=$now
	done

	info ""
	local final
	final=$(fdcount)
	if [ "$final" -eq "$base" ]; then
		ok "vuelta al valor de reposo: $base -> $final"
	else
		bad "no vuelve al reposo: $base -> $final  (se quedan $((final - base)) colgando)"
		info "  descriptores que sobran:"
		ls -l /proc/"$SRV_PID"/fd 2>/dev/null | tail -n +2 | head -20 | sed 's/^/    /'
	fi

	# --- prueba de resistencia: muchas conexiones cortas seguidas
	info ""
	info "60 conexiones cortas seguidas..."
	for i in $(seq 1 60); do
		printf 'PASS %s\r\nNICK s%d\r\nUSER su 0 * :S\r\nJOIN #stress\r\nQUIT\r\n' "$PASSWORD" "$i" \
			| timeout 1 nc localhost "$PORT" > /dev/null 2>&1
	done
	sleep 1
	local after_stress
	after_stress=$(fdcount)
	if [ "$after_stress" -le "$((base + 1))" ]; then
		ok "tras 60 conexiones: $after_stress descriptores (reposo $base)"
	else
		bad "tras 60 conexiones: $after_stress descriptores (reposo $base) -> $((after_stress - base)) colgando"
		ls -l /proc/"$SRV_PID"/fd 2>/dev/null | tail -n +2 | head -20 | sed 's/^/    /'
	fi

	kill -INT "$SRV_PID" 2>/dev/null
	sleep 1
	kill -9 "$SRV_PID" 2>/dev/null
	SRV_PID=""
}

# ===================================================================== MAIN
if [ ! -x ./ircserv ]; then
	echo "${R}No existe ./ircserv. Lanza 'make' primero.${N}"
	exit 1
fi
if ! command -v valgrind > /dev/null; then
	echo "${R}valgrind no esta instalado: sudo nala install valgrind${N}"
	exit 1
fi

printf "${B}Bateria de fugas ft_irc${N}  (puerto %s)\n" "$PORT"
parte_b
parte_a
printf "\n"
