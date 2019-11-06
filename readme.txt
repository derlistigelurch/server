Um das Programm zu starten werden folgende Pakete benötigt:
- libldap2-dev
- uuid-dev

Usage:
make all

Server starten
server portnr dir
(zB. ./server 5678 saved_mails)

Client starten
client serveraddress portnr
(zB. ./client 127.0.0.1 5678)
