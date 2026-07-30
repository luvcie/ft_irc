#!/bin/bash
# ============================================================================
#  Bateria de tests para ft_irc  --  uso:  ./test_irc.sh [puerto]
#
#  Cada escenario levanta un servidor limpio, abre hasta 3 clientes nc a traves
#  de FIFOs (fd 3, 4 y 5) y compara lo que recibe cada uno con lo esperado.
#
#  Los tests marcados PENDIENTE no son fallos: son cosas que el subject pide y
#  todavia no estan implementadas. Sirven de lista de tareas.
# ============================================================================

PORT=${1:-6767}
PASSWORD=test
DIR=$(mktemp -d)
SRV_PID=""

OK=0
KO=0
PEND=0

R=$'\e[31m'; G=$'\e[32m'; Y=$'\e[33m'; B=$'\e[1m'; N=$'\e[0m'

cleanup() {
	[ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null
	exec 3>&- 4>&- 5>&- 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup EXIT

# ---------------------------------------------------------------- infra ----

start_server() {
	./ircserv "$PORT" "$PASSWORD" > "$DIR/server.log" 2>&1 &
	SRV_PID=$!
	sleep 0.4
}

stop_server() {
	exec 3>&- 4>&- 5>&- 2>/dev/null
	sleep 0.2
	kill "$SRV_PID" 2>/dev/null
	wait "$SRV_PID" 2>/dev/null
	SRV_PID=""
	rm -f "$DIR"/in* "$DIR"/out*
}

open_clients() {           # $1 = cuantos clientes (1..3)
	local i
	# de uno en uno y con pausa: asi el orden de los fd en el servidor es el
	# mismo que el orden de los clientes, y las listas de NAMES son predecibles
	for i in $(seq 1 "$1"); do
		mkfifo "$DIR/in$i"
		: > "$DIR/out$i"
		timeout 30 nc localhost "$PORT" < "$DIR/in$i" > "$DIR/out$i" 2>/dev/null &
		case $i in
			1) exec 3> "$DIR/in1" ;;
			2) exec 4> "$DIR/in2" ;;
			3) exec 5> "$DIR/in3" ;;
		esac
		sleep 0.25
	done
}

s1() { printf '%s\r\n' "$1" >&3; sleep "${2:-0.15}"; }
s2() { printf '%s\r\n' "$1" >&4; sleep "${2:-0.15}"; }
s3() { printf '%s\r\n' "$1" >&5; sleep "${2:-0.15}"; }
raw1() { printf '%s' "$1" >&3; sleep "${2:-0.2}"; }   # sin \r\n, para envios partidos

register() {               # $1 = fd (1|2|3), $2 = nick, $3 = user
	local f=s$1
	$f "PASS $PASSWORD"
	$f "NICK $2"
	$f "USER $3 0 * :$2"
}

pass_t()  { OK=$((OK+1));   printf "  ${G}ok${N}   %s\n" "$1"; }
fail_t()  { KO=$((KO+1));   printf "  ${R}FALLA${N} %s\n" "$1"; printf "         esperaba: %s\n" "$2"; }
pend_t()  { PEND=$((PEND+1)); printf "  ${Y}PEND${N} %s\n" "$1"; }

has()     { grep -qF -- "$2" "$DIR/out$1"; }

# el cliente $1 DEBE haber recibido $2
check()   { if has "$1" "$2"; then pass_t "$3"; else fail_t "$3" "$2"; fi; }
# el cliente $1 NO debe haber recibido $2
nocheck() { if has "$1" "$2"; then fail_t "$3" "NO recibir: $2"; else pass_t "$3"; fi; }
# lo mismo, pero es una carencia conocida: se informa, no cuenta como fallo
todo()    { if has "$1" "$2"; then pass_t "$3"; else pend_t "$3  (falta: $2)"; fi; }

title()   { printf "\n${B}%s${N}\n" "$1"; }

# ================================================================ 1. REGISTRO
escenario_registro() {
	title "1. REGISTRO  (PASS / NICK / USER / PING)"
	start_server; open_clients 2

	s1 "JOIN #early"                                  # comando antes de registrarse
	check 1 "451" "comando antes de registrarse -> 451"

	s1 "PASS malapass"
	s1 "NICK temprano"
	s1 "USER u 0 * :U"
	check 1 "464" "password incorrecta -> 464"

	stop_server; start_server; open_clients 2

	register 1 alice alu
	check 1 ":ircserv 001 alice" "registro correcto -> 001 de bienvenida"

	s1 "PASS $PASSWORD"
	check 1 "462" "PASS despues de registrado -> 462"

	s1 "USER otro 0 * :Otro"
	check 1 "462" "USER despues de registrado -> 462"

	register 2 ALICE alu2                              # mismo nick en mayusculas
	check 2 "433" "nick duplicado ignorando mayusculas -> 433 (ircLower)"

	s2 "NICK ali:ce"
	check 2 "432" "nick con caracter invalido -> 432"

	s2 "USER"
	check 2 "461" "USER sin parametros -> 461"

	s1 "PING token123"
	check 1 "PONG" "PING -> PONG"

	stop_server
}

# ============================================================ 2. JOIN / PART
escenario_join_part() {
	title "2. CANALES  (JOIN / PART)"
	start_server; open_clients 2

	register 1 alice alu
	register 2 bob bou

	s1 "JOIN #sala"
	check 1 ":alice!alu@127.0.0.1 JOIN #sala" "JOIN difunde el eco al que entra"
	check 1 "331 alice #sala" "canal nuevo sin topic -> 331"
	check 1 "353 alice = #sala :@alice" "el creador sale como @alice en el 353"
	check 1 "366" "fin de la lista de nombres -> 366"

	s2 "JOIN #sala"
	check 1 ":bob!bou@127.0.0.1 JOIN #sala" "los de dentro ven entrar al nuevo"
	# el orden del 353 lo marca el fd, no el nick: se comprueba quien lleva la @
	check 2 "@alice" "el creador aparece con @ en la lista del segundo"
	nocheck 2 "@bob" "el segundo NO es operador"

	s2 "JOIN sin_almohadilla"
	check 2 "403 bob sin_almohadilla" "nombre de canal sin # -> 403"

	s2 "PART #noexiste"
	check 2 "403 bob #noexiste" "PART de un canal inexistente -> 403"

	s2 "PART #sala :me piro"
	check 1 ":bob!bou@127.0.0.1 PART #sala :me piro" "PART difunde con el motivo"

	s2 "PART #sala"
	check 2 "442 bob #sala" "PART sin estar dentro -> 442"

	s1 "PART #sala"
	s1 "JOIN #sala"
	check 1 "353 alice = #sala :@alice" "canal vaciado y recreado: vuelve a ser operador"

	stop_server
}

# ====================================================== 3. PRIVMSG / NOTICE
escenario_mensajes() {
	title "3. MENSAJES  (PRIVMSG / NOTICE)"
	start_server; open_clients 3

	register 1 alice alu
	register 2 bob bou
	register 3 carol cau
	s1 "JOIN #sala"; s2 "JOIN #sala"

	s1 "PRIVMSG #sala :hola a todos"
	check 2 ":alice!alu@127.0.0.1 PRIVMSG #sala :hola a todos" "PRIVMSG a canal llega a los demas"
	nocheck 1 "PRIVMSG #sala :hola a todos" "el emisor NO recibe su propio PRIVMSG"

	s1 "PRIVMSG bob :en privado"
	check 2 ":alice!alu@127.0.0.1 PRIVMSG bob :en privado" "PRIVMSG a un nick llega"
	nocheck 3 "en privado" "un tercero NO ve el privado"

	s1 "PRIVMSG BOB :mayusculas"
	check 2 "PRIVMSG BOB :mayusculas" "PRIVMSG a nick en mayusculas llega (ircLower)"

	s1 "PRIVMSG"
	check 1 "411" "PRIVMSG sin destinatario -> 411"
	s1 "PRIVMSG bob"
	check 1 "412" "PRIVMSG sin texto -> 412"
	s1 "PRIVMSG #nada :hey"
	check 1 "403 alice #nada" "PRIVMSG a canal inexistente -> 403"
	s3 "PRIVMSG #sala :desde fuera"
	check 3 "404 carol #sala" "PRIVMSG a un canal donde no estas -> 404"
	s1 "PRIVMSG fantasma :hey"
	check 1 "401 alice fantasma" "PRIVMSG a nick inexistente -> 401"

	# NOTICE entrega igual pero NUNCA contesta con error
	: > "$DIR/out1"
	s1 "NOTICE #sala :aviso"
	check 2 ":alice!alu@127.0.0.1 NOTICE #sala :aviso" "NOTICE a canal se entrega igual"
	s1 "NOTICE fantasma :hey"
	s1 "NOTICE"
	s1 "NOTICE #nada :hey"
	nocheck 1 ":ircserv 4" "NOTICE nunca devuelve numericos de error"

	stop_server
}

# ==================================================================== 4. TOPIC
escenario_topic() {
	title "4. TOPIC"
	start_server; open_clients 2

	register 1 alice alu
	register 2 bob bou
	s1 "JOIN #sala"; s2 "JOIN #sala"

	s1 "TOPIC"
	check 1 "461" "TOPIC sin parametros -> 461"
	s1 "TOPIC #nada"
	check 1 "403 alice #nada" "TOPIC de canal inexistente -> 403"

	s1 "TOPIC #sala"
	check 1 "331 alice #sala" "consultar topic vacio -> 331"

	s1 "TOPIC #sala :bienvenidos al canal"
	check 2 ":alice!alu@127.0.0.1 TOPIC #sala :bienvenidos al canal" "cambiar topic se difunde"
	s2 "TOPIC #sala"
	check 2 "332 bob #sala :bienvenidos al canal" "consultar topic puesto -> 332"

	s2 "TOPIC #sala :lo cambio yo"
	check 1 ":bob!bou@127.0.0.1 TOPIC #sala :lo cambio yo" "sin +t cualquiera cambia el topic"

	s1 "MODE #sala +t"
	s2 "TOPIC #sala :otra vez yo"
	check 2 "482 bob #sala" "con +t un no-operador no puede -> 482"

	stop_server
}

# ===================================================================== 5. KICK
escenario_kick() {
	title "5. KICK"
	start_server; open_clients 3

	register 1 alice alu
	register 2 bob bou
	register 3 carol cau
	s1 "JOIN #sala"; s2 "JOIN #sala"; s3 "JOIN #sala"

	s1 "KICK #sala"
	check 1 "461" "KICK con un solo parametro -> 461"
	s1 "KICK #nada bob"
	check 1 "403 alice #nada" "KICK en canal inexistente -> 403"
	s1 "KICK #sala fantasma"
	check 1 "401 alice fantasma" "KICK a un nick que no existe -> 401"

	s2 "KICK #sala carol"
	check 2 "482 bob #sala" "KICK siendo no-operador -> 482"

	s1 "KICK #sala bob :fuera"
	check 2 ":alice!alu@127.0.0.1 KICK #sala bob :fuera" "la VICTIMA recibe el KICK (difundir antes de borrar)"
	check 3 ":alice!alu@127.0.0.1 KICK #sala bob :fuera" "un TESTIGO recibe el KICK"
	check 1 ":alice!alu@127.0.0.1 KICK #sala bob :fuera" "el OPERADOR recibe su eco"

	s2 "TOPIC #sala"
	check 2 "442 bob #sala" "tras el KICK, la victima ya no esta en el canal"

	s1 "KICK #sala carol"
	s1 "KICK #sala carol"
	check 1 "441" "KICK a alguien que ya no esta en el canal -> 441"

	stop_server
}

# ===================================================================== 6. MODE
escenario_mode() {
	title "6. MODE"
	start_server; open_clients 2

	register 1 alice alu
	register 2 bob bou
	s1 "JOIN #sala"; s2 "JOIN #sala"

	s1 "MODE"
	check 1 "461" "MODE sin parametros -> 461"
	s1 "MODE alice +i"
	check 1 "502" "MODE sobre un nick (no es canal) -> 502"
	s1 "MODE :"
	check 1 "502" "MODE con parametro vacio -> 502, y el servidor sigue vivo"
	s1 "MODE #nada"
	check 1 "403 alice #nada" "MODE de canal inexistente -> 403"

	s1 "MODE #sala"
	check 1 "324 alice #sala +" "consulta de un canal sin modos -> 324 +"

	s2 "MODE #sala +t"
	check 2 "482 bob #sala" "cambiar modos siendo no-operador -> 482"

	s1 "MODE #sala +i"
	check 2 ":alice!alu@127.0.0.1 MODE #sala +i" "+i se difunde al canal"
	s1 "MODE #sala +k secreto"
	check 1 "MODE #sala +k secreto" "+k con clave"
	s1 "MODE #sala +l 5"
	check 1 "MODE #sala +l 5" "+l con limite"
	s1 "MODE #sala"
	check 1 "324 alice #sala +ikl secreto 5" "la consulta muestra modos Y valores"

	: > "$DIR/out1"
	s1 "MODE #sala +l abc"
	nocheck 1 "+l" "+l con texto se ignora en silencio"
	s1 "MODE #sala +l -5"
	nocheck 1 "+l" "+l negativo se ignora en silencio"
	s1 "MODE #sala +o"
	nocheck 1 "+o" "+o sin nick se salta sin error"

	s1 "MODE #sala +z"
	check 1 "472 alice z" "letra de modo desconocida -> 472"
	s1 "MODE #sala +o fantasma"
	check 1 "401 alice fantasma" "+o a un nick inexistente -> 401"

	s1 "MODE #sala -ikl"
	check 1 "MODE #sala -i-k-l" "quitar tres modos de golpe, sin consumir parametros"
	s1 "MODE #sala"
	check 1 "324 alice #sala +" "tras quitarlos, la consulta vuelve a +"

	s1 "MODE #sala +ot BOB"
	check 2 "MODE #sala +o+t BOB" "+ot encadenado: el nick va a la o, en mayusculas (ircLower)"
	s2 "MODE #sala -t"
	check 1 ":bob!bou@127.0.0.1 MODE #sala -t" "bob ya es operador y puede cambiar modos"

	stop_server
}

# ===================================================================== 7. QUIT
escenario_quit() {
	title "7. QUIT  y supervivencia del servidor"
	start_server; open_clients 3

	register 1 alice alu
	register 2 bob bou
	register 3 carol cau
	s1 "JOIN #sala"; s2 "JOIN #sala"; s3 "JOIN #sala"

	s2 "QUIT :me voy"
	check 1 ":bob!bou@127.0.0.1 QUIT :me voy" "el QUIT llega a los del canal"
	check 3 ":bob!bou@127.0.0.1 QUIT :me voy" "y a todos, una sola vez"

	s1 "PRIVMSG #sala :sigo aqui"
	check 3 "PRIVMSG #sala :sigo aqui" "el servidor sigue funcionando tras un QUIT"

	# el arreglo de recvFromClient: QUIT y mas lineas en el MISMO paquete.
	# sin el guard, el bucle seguiria usando una referencia a un Client destruido
	: > "$DIR/out1"
	printf 'QUIT :adios\r\nPRIVMSG #sala :zombie\r\nMODE #sala +t\r\n' >&5
	sleep 0.4
	s1 "MODE #sala"
	check 1 "324 alice #sala" "el servidor responde tras un QUIT + mas lineas en el mismo recv"
	nocheck 1 "zombie" "las lineas posteriores al QUIT no se ejecutan"

	if kill -0 "$SRV_PID" 2>/dev/null; then
		pass_t "el proceso del servidor sigue vivo al final"
	else
		fail_t "el proceso del servidor sigue vivo al final" "proceso vivo"
	fi

	stop_server
}

# ================================================================ 8. ROBUSTEZ
escenario_robustez() {
	title "8. ROBUSTEZ  (datos partidos, lineas raras)"
	start_server; open_clients 1

	register 1 alice alu
	s1 "JOIN #sala"

	# el test del subject: un comando enviado en varios trozos
	raw1 "MOD"
	raw1 "E #sa"
	raw1 "la"
	s1 ""
	check 1 "324 alice #sala" "comando partido en 3 paquetes se reconstruye (test del subject)"

	# dos comandos en un solo paquete
	: > "$DIR/out1"
	printf 'MODE #sala +t\r\nMODE #sala\r\n' >&3
	sleep 0.4
	check 1 "324 alice #sala +t" "dos comandos en un solo paquete se procesan los dos"

	# linea vacia, solo espacios, y \n a secas sin \r
	s1 ""
	s1 "   "
	printf 'MODE #sala\n' >&3
	sleep 0.3
	check 1 "324" "lineas vacias y \\n suelto no rompen nada"

	# comando larguisimo
	s1 "TOPIC #sala :$(head -c 400 < /dev/zero | tr '\0' 'x')"
	if kill -0 "$SRV_PID" 2>/dev/null; then
		pass_t "un topic de 400 caracteres no tumba el servidor"
	else
		fail_t "un topic de 400 caracteres no tumba el servidor" "proceso vivo"
	fi

	stop_server
}

# =================================================================== 9. INVITE
escenario_invite() {
	title "9. INVITE"
	start_server; open_clients 3

	register 1 alice alu
	register 2 bob bou
	register 3 carol cau
	s1 "JOIN #sala"
	s3 "JOIN #sala"

	s1 "INVITE bob"
	check 1 "461" "INVITE con un solo parametro -> 461"
	s1 "INVITE bob #nada"
	check 1 "403 alice #nada" "INVITE a un canal inexistente -> 403"
	s2 "INVITE carol #sala"
	check 2 "442 bob #sala" "invitar sin estar en el canal -> 442"
	s1 "INVITE fantasma #sala"
	check 1 "401 alice fantasma" "INVITE a un nick inexistente -> 401"
	s1 "INVITE carol #sala"
	check 1 "443 alice carol #sala" "invitar a alguien que ya esta dentro -> 443"

	# canal abierto: cualquier miembro puede invitar, no hace falta ser operador
	s3 "INVITE bob #sala"
	check 3 "341 carol bob #sala" "sin +i un no-operador puede invitar -> 341"
	check 2 ":carol!cau@127.0.0.1 INVITE bob :#sala" "el invitado recibe la invitacion"
	nocheck 1 "INVITE bob" "el resto del canal NO se entera de la invitacion"

	# con +i la restriccion aparece, igual que +t en TOPIC
	s1 "MODE #sala +i"
	s3 "INVITE bob #sala"
	check 3 "482 carol #sala" "con +i un no-operador ya no puede invitar -> 482"

	# y la invitacion abre la puerta del 473
	s1 "INVITE bob #sala"
	check 1 "341 alice bob #sala" "el operador invita en un canal +i"
	s2 "JOIN #sala"
	check 2 "353 bob" "el invitado entra en un canal +i"
	nocheck 2 "473" "y no recibe el 473"

	# la invitacion es de un solo uso
	s2 "PART #sala"
	s2 "JOIN #sala"
	check 2 "473 bob #sala" "la invitacion se consume: al volver ya no vale -> 473"

	stop_server
}

# ============================================= 10. LO QUE FALTA (informativo)
escenario_pendientes() {
	title "10. PENDIENTE  (lo pide el subject y aun no esta)"
	start_server; open_clients 2

	register 1 alice alu
	register 2 bob bou
	s1 "JOIN #sala"

	s1 "MODE #sala +i"
	s2 "JOIN #sala"
	todo 2 "473" "canal +i deberia rechazar el JOIN -> 473  [falta en cmd_join.cpp]"

	s1 "MODE #sala -i"
	s1 "MODE #sala +k clave"
	s2 "PART #sala"
	s2 "JOIN #sala"
	todo 2 "475" "canal +k deberia pedir la clave -> 475  [falta en cmd_join.cpp]"

	s1 "MODE #sala -k"
	s1 "MODE #sala +l 1"
	s2 "PART #sala"
	s2 "JOIN #sala"
	todo 2 "471" "canal +l lleno deberia rechazar -> 471  [falta en cmd_join.cpp]"

	s1 "COMANDOINVENTADO algo"
	todo 1 "421" "comando desconocido deberia dar -> 421  [falta en dispatch()]"

	# JOIN normaliza el nombre con ircLower, MODE todavia no: MODE #General no
	# encuentra el canal que JOIN guardo como #general
	s1 "JOIN #General"
	s1 "MODE #General"
	todo 1 "324 alice #general" "MODE deberia encontrar #General  [falta ircLower en cmd_mode.cpp]"

	stop_server
}

# ===================================================================== MAIN
if [ ! -x ./ircserv ]; then
	echo "${R}No existe ./ircserv. Lanza 'make' primero.${N}"
	exit 1
fi

printf "${B}Bateria de tests ft_irc${N}  (puerto %s)\n" "$PORT"

escenario_registro
escenario_join_part
escenario_mensajes
escenario_topic
escenario_kick
escenario_mode
escenario_quit
escenario_robustez
escenario_invite
escenario_pendientes

printf "\n${B}RESUMEN${N}\n"
printf "  ${G}ok${N}       %d\n" "$OK"
printf "  ${R}fallos${N}   %d\n" "$KO"
printf "  ${Y}pendientes${N} %d   (no son fallos: features que faltan)\n" "$PEND"
printf "\n  Transcripciones de la ultima ejecucion en el log del servidor.\n"

[ "$KO" -eq 0 ]
