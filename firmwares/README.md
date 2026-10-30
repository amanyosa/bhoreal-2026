# Firmwares del Bhoreal

| Fitxer | Què és |
|---|---|
| `bhoreal_original.hex` | Firmware original del Bhoreal SLIM, llegit de la placa abans de tocar res |
| `bhoreal_seq_v1/` | Seqüenciador per passos v1: bateria + melodia (pentatònica de Do), 3 modes, 8 memòries a l'EEPROM. Inclou el codi font (`src/`) i la web app (`webapp/`) |
| `bhoreal_apps_v1/` | Menú d'inici amb 3 apps: seqüenciador, pica el talp i Simon. Per tornar al menú, mantenir premudes les dues cantonades de dalt 1,5 s. Inclou el codi font (`src/`) i la web app (`webapp/`) |
| `bhoreal_apps_v2/` | Com la v1 més el Memory (parelles) amb 5 nivells, de 2×4 a 6×8. Inclou el codi font (`src/`) i la web app (`webapp/`) |
| `bhoreal_apps_v3/` | Com la v2 més el seqüenciador generatiu de boles (rebots a les parets = notes; mantenir una bola 2 s l'esborra). Inclou el codi font (`src/`) i la web app (`webapp/`) |
| `bhoreal_apps_v4/` | Com la v3 més el Frogger (5 nivells, el 5 amb riu i troncs), el menú reordenat i la correcció dels botons fantasma (v1-v3 la tenen). Inclou el codi font (`src/`, amb `rescue.py`) i la web app (`webapp/`) |

## Tornar a pujar un firmware

Amb el Bhoreal connectat, des de la carpeta `arduino`:

```bash
bhoreal_seq/upload.sh firmwares/bhoreal_seq_v1/bhoreal_seq.hex
```

o, per al menú amb les sis apps (la versió més recent):

```bash
bhoreal_seq/upload.sh firmwares/bhoreal_apps_v4/bhoreal_apps.hex
```

o, per tornar a l'original:

```bash
bhoreal_seq/upload.sh firmwares/bhoreal_original.hex
```

Les memòries guardades a l'EEPROM no s'esborren en pujar un firmware nou.

## Si una pujada queda a mitges

No deixis cap programa amb el port sèrie obert mentre puges un firmware. Si una pujada s'interromp, el Bhoreal pot quedar arrencant però sense respondre ni aparèixer al USB. Per recuperar-lo:

```bash
python3 bhoreal_apps/rescue.py firmwares/bhoreal_apps_v4/bhoreal_apps.hex
```

i, mentre s'executa, desconnecta i torna a connectar el Bhoreal unes quantes vegades. El script esborra el programa trencat en el moment que el bootloader apareix i després hi carrega el firmware.
