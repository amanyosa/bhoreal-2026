# Bhoreal 2026

Firmwares nous per al Bhoreal SLIM (graella de 8×8 botons amb LEDs RGB, ATmega32U4): jocs per a nens i eines de producció musical per MIDI.

## Contingut

| Carpeta | Què hi ha |
|---|---|
| `bhoreal_apps/` | Codi actual. Un menú d'inici amb apps, que es compila en dos firmwares: **jocs** i **música** |
| `bhoreal_seq/` | Primer seqüenciador per passos (versió independent) i la **web app** (`webapp/`) que fa sonar el MIDI al navegador |
| `bhoreal_talps/` | Primera versió del joc de pica el talp (independent) |
| `firmwares/` | Versions guardades (`.hex` llestos per pujar) i instruccions de pujada i rescat |
| `backup_bhoreal/` | Còpia del firmware original llegit de la placa abans de tocar res |

### Firmware de jocs
Pica el talp, Simon, Memory (parelles), Frogger, Serp, Connecta 4, Duel de reflexos (2 jugadors) i Pong (2 jugadors).

### Firmware de música
Seqüenciador per passos (bateria + melodia, 8 memòries) i seqüenciador generatiu de boles. Envien MIDI per USB, i la web app els fa sonar a Chrome.

Des de qualsevol app, mantenir premudes les dues cantonades de dalt 1,5 s torna al menú.

## Compilar i pujar

Fa servir el toolchain AVR que instal·la l'Arduino IDE (`~/Library/Arduino15`), sense arduino-cli.

```bash
cd bhoreal_apps
./build.sh            # compila els dos firmwares a build/
./upload.sh games     # o: ./upload.sh music
```

No tinguis cap monitor sèrie obert mentre puges. Si una pujada queda a mitges, consulta la secció de rescat de `firmwares/README.md`.

## Web app

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory bhoreal_seq/webapp
```

Obre http://127.0.0.1:8765 a Chrome (necessita Web MIDI).
