# Update-Proiect-SDSI

---

Extensia adaugă aplicației Smart Home o secțiune de **diagnostic**: comenzi trimise din interfața web către ESP32, prin care se testează individual **ecranul tactil ILI9341 (SPI)** și **senzorul DHT11**. Rezultatul fiecărei comenzi apare într-o consolă din pagină, împreună cu un status de execuție, iar ultimele 10 comenzi sunt păstrate într-un istoric.

Restul proiectului (ventilație, iluminat Tapo, acces, utilizatori) rămâne neschimbat.

---

## Ce s-a adăugat

- Pagina **„Stare ESP”** a fost redenumită **„Stare Sistem”**. Cele 4 carduri cu informații despre ESP32 (WiFi, IP, uptime, RAM) au rămas.
- Sub carduri: secțiunea **Comenzi Diagnostic**, cu o casetă de comandă, o consolă de răspuns și butoane pentru **Trimite**, **Reset la valori implicite** și **Curăță consola**.
- Butonul **Deschide Istoric Comenzi** afișează ultimele 10 comenzi (data, IP, rol, comandă, rezultat, status), după același principiu ca jurnalul de ventilație.
- Comenzile pot fi folosite **doar de utilizatorii cu rol de admin**. Verificarea se face pe server, nu doar în browser.

---

## Comenzi disponibile

Scrie comanda în casetă și apasă **Enter** sau **Trimite**. Comanda `help` afișează lista direct în consolă.

### Ecran ILI9341 (SPI)

Codurile se pot scrie ca `29h`, `0x29` sau `29`.

| Comandă | Registru | Ce face |
|---|---|---|
| `04h` | RDDID / D3h | Citește ID-ul afișajului. Așteptat: `00 93 41` |
| `0Ah` | RDDPM | Citește modul de alimentare (booster, sleep, display etc.) |
| `0Ch` | RDDCOLMOD | Citește formatul pixelilor (16 biți/pixel pe SPI) |
| `28h` | DISPOFF | Oprește afișajul |
| `29h` | DISPON | Pornește afișajul |

Comenzile de scriere (`28h`, `29h`) nu au confirmare pe SPI, deci efectul este verificat citind înapoi registrul `0Ah` (bitul D2 = display pornit).

### Senzor DHT11

| Comandă | Ce face |
|---|---|
| `dht.temp` | Citește temperatura |
| `dht.hum` | Citește umiditatea |
| `dht.all` | Temperatură, umiditate și indice de căldură dintr-o singură măsurătoare |
| `dht.raw` | Citește direct cadrul brut de 40 de biți (5 octeți) și verifică checksum-ul |
| `dht.test` | Autotest: 3 citiri consecutive, cu minim și maxim (durează ~3 s) |

DHT11 nu este I2C: folosește un protocol propriu pe un singur fir de date (pinul 13). Senzorul poate fi citit cel mult o dată pe secundă, iar codul impune o pauză de minim 1,1 s între citiri.

### Altele

| Comandă | Ce face |
|---|---|
| `reset` | Readuce ecranul și senzorul la starea inițială (la fel ca butonul **Reset**) |
| `help` | Afișează lista de comenzi (rezolvată în browser, nu ajunge la ESP32) |

---

## Formatul răspunsului

Fiecare comandă afișează în consolă descrierea, valorile citite și un status:

```
> 29h
Comanda 29h – Display ON (pornește afișajul)
  Verificare: 0Ah = 0x9C → bit D2 (display) = 1 (ON)
Status execuție: OK
```

| Status | Semnificație |
|---|---|
| `OK` | Comanda s-a executat și rezultatul a fost verificat |
| `EROARE` | Comanda a eșuat sau valoarea citită nu este cea așteptată (cu motiv) |
| `NEVERIFICAT` | Comanda a fost trimisă, dar starea nu a putut fi citită înapoi |

---

## Cum funcționează

Browserul **nu comunică direct** cu ESP32 pentru aceste comenzi. Drumul unei comenzi:

```
Browser  --POST c=29h-->  diag.php  --GET /cmd?c=29h&k=cheie-->  ESP32
   ^                          |                                     |
   |                          +-- verifică sesiunea și rolul admin  |
   |                          +-- validează comanda (listă fixă)    |
   |                          +-- scrie comanda în log_comenzi      |
   +-------- JSON <----------+<---------------- JSON <-------------+
```

1. **Browserul** (`script.js`) trimite comanda prin POST la `diag.php`, împreună cu cookie-ul de sesiune.
2. **`diag.php`** verifică pe server sesiunea și rolul admin (`verifica_admin.php`), normalizează comanda (`0x29`, `29` și `29h` devin `29h`) și o compară cu lista permisă. Ce nu este în listă nu ajunge la ESP32. Apoi o trimite la ESP32 prin HTTP.
3. **ESP32** (`/cmd` în `.ino`) acceptă cererea doar dacă vine de la IP-ul XAMPP și cheia este corectă, execută comanda pe ecran sau pe senzor și răspunde în JSON (descriere, linii cu valori, status, motiv, durată).
4. **`diag.php`** salvează comanda în tabelul `log_comenzi` (ultimele 10) și trimite răspunsul înapoi în browser.
5. **Browserul** afișează răspunsul în consolă.

Datele **nu** trec prin Serial Monitor: acesta rămâne doar pentru depanare. Valorile citite sunt aceleași ca în Serial Monitor, pentru că vin din aceleași funcții de pe ESP32; site-ul doar transportă comanda și răspunsul.

### Comunicarea componentelor

| Componentă | Tip de comunicare |
|---|---|
| Ecran ILI9341 | SPI (SCK, MOSI, MISO, CS, DC, RESET); ESP32 este master, ecranul este slave |
| Touch XPT2046 | SPI, pe aceeași magistrală, cu CS propriu (`T_CS`) |
| DHT11 | Protocol propriu pe un singur fir (cadru de 40 de biți cu checksum) |
| ESP32 ↔ aplicație | WiFi, HTTP, JSON |

### Securitate

- Doar adminii pot trimite comenzi. Rolul este citit din baza de date la fiecare cerere, pe baza sesiunii PHP create la login.
- ESP32 acceptă `/cmd` doar de la IP-ul XAMPP și cu o cheie comună (`DIAG_KEY` în `.ino` = `$CHEIE_ESP` în `diag.php`).
- Istoricul este scris de server, nu de browser, deci nu poate fi falsificat din pagină.
- `gestiune_utilizatori.php` verifică acum și el rolul de admin pe server.
