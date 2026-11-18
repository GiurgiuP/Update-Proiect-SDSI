<?php
/**
 * diag.php
 * Proxy sigur intre consola de comenzi din site ("Stare Sistem") si ESP32.
 *
 * Browserul NU vorbeste direct cu ESP32 pentru aceste comenzi. Fluxul este:
 *   browser --POST c=29h--> diag.php --verifica sesiunea de admin--> ESP32 /cmd --> raspuns
 *   diag.php scrie comanda in log_comenzi (istoric) si trimite raspunsul inapoi in browser.
 *
 * Doar adminii pot folosi acest fisier (verificare pe server, vezi verifica_admin.php).
 *
 * Comenzi acceptate (lista fixa; orice altceva este respins aici, inainte sa ajunga la ESP32):
 *   Ecran ILI9341 (SPI): 04h, 0Ah, 0Ch, 28h, 29h   (se pot scrie ca 29h, 0x29 sau 29)
 *   Senzor DHT11:        dht.temp, dht.hum, dht.all, dht.raw, dht.test
 *   Resetare:            reset (ecran + senzor la starea initiala)
 *
 * Cerere:   POST  c=<comanda>
 * Raspuns:  { "status": "OK|EROARE|NEVERIFICAT", "comanda": "29h", "descriere": "...",
 *             "linii": ["..."], "motiv": "...", "ms": 12, "logat": true }
 */

header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store");
ini_set('display_errors', '0');   // avertismentele PHP nu au voie sa se amestece in JSON
ob_start();                        // prindem orice text "ratacit" (warning, spatii) de la include-uri
include 'conectare.php';
include 'verifica_admin.php';

// ---------------------------------------------------------------------------
// CONFIGURARE - se modifica manual daca se schimba adresele (ca in COD_ARDUINO.ino)
// ---------------------------------------------------------------------------
$ESP_IP    = "10.176.199.36";               // IP-ul ESP32 (acelasi ca ESP_IP din script.js)
$CHEIE_ESP = "SmartHome2026-diag";          // TREBUIE sa fie identica cu DIAG_KEY din COD_ARDUINO.ino
$NR_ISTORIC = 10;                           // cate comenzi se pastreaza in log_comenzi

// ---------------------------------------------------------------------------
// 1. Doar admin, doar POST
// ---------------------------------------------------------------------------
$rol = cere_admin($conn);

/** Trimite raspunsul JSON curat: arunca orice text ratacit si inlocuieste caracterele UTF-8 invalide. */
function trimite_json($date) {
    while (ob_get_level() > 0) ob_end_clean();
    echo json_encode($date, JSON_INVALID_UTF8_SUBSTITUTE | JSON_PARTIAL_OUTPUT_ON_ERROR);
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    trimite_json(["status" => "EROARE", "motiv" => "Metoda invalida (se foloseste POST)."]);
    $conn->close();
    exit();
}

// ---------------------------------------------------------------------------
// 2. Normalizarea comenzii (aceeasi comanda se poate scrie in mai multe feluri)
// ---------------------------------------------------------------------------
/**
 * Returneaza forma standard a comenzii ("29h", "0Ah", "dht.temp", "reset") sau null daca
 * nu este una dintre cele 11 comenzi permise.
 */
function normalizeaza_comanda($text) {
    $s = strtolower(trim($text));
    if ($s === '' || strlen($s) > 20) return null;

    $nume = ['dht.temp', 'dht.hum', 'dht.all', 'dht.raw', 'dht.test', 'reset'];
    if (in_array($s, $nume, true)) return $s;

    // cod hexadecimal: 29h / 0x29 / 29 / 9h  =>  "29h" / "09h"
    if (preg_match('/^(?:0x)?([0-9a-f]{1,2})h?$/', $s, $m)) {
        $hex = strtoupper(str_pad($m[1], 2, '0', STR_PAD_LEFT)) . 'h';
        if (in_array($hex, ['04h', '0Ah', '0Ch', '28h', '29h'], true)) return $hex;
    }
    return null;
}

$brut = isset($_POST['c']) ? (string) $_POST['c'] : '';
$cmd  = normalizeaza_comanda($brut);

if ($cmd === null) {
    trimite_json([
        "status"    => "EROARE",
        "comanda"   => mb_substr(trim($brut), 0, 20),
        "descriere" => "comandă necunoscută",
        "linii"     => [],
        "motiv"     => "Comandă necunoscută. Scrie help pentru lista comenzilor permise.",
        "ms"        => 0,
        "logat"     => false
    ]);
    $conn->close();
    exit();
}

// ---------------------------------------------------------------------------
// 3. Trimiterea comenzii la ESP32 (timeout generos: dht.test dureaza ~3-4 secunde)
// ---------------------------------------------------------------------------
$url = "http://" . $ESP_IP . "/cmd?c=" . urlencode($cmd) . "&k=" . urlencode($CHEIE_ESP);

$start      = microtime(true);
$raspuns    = false;
$codHttp    = 0;
$eroareCone = '';

if (function_exists('curl_init')) {
    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_CONNECTTIMEOUT => 2,
        CURLOPT_TIMEOUT        => 12
    ]);
    $raspuns    = curl_exec($ch);
    $codHttp    = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $eroareCone = curl_error($ch);
    curl_close($ch);
} else {
    // varianta de rezerva daca extensia cURL nu este activata in XAMPP
    $context = stream_context_create(['http' => ['timeout' => 12, 'ignore_errors' => true]]);
    $raspuns = @file_get_contents($url, false, $context);
    if (isset($http_response_header[0]) && preg_match('/\s(\d{3})\s/', $http_response_header[0], $mm)) {
        $codHttp = (int) $mm[1];
    }
    if ($raspuns === false) $eroareCone = 'conexiune esuata';
}
$ms = (int) round((microtime(true) - $start) * 1000);

// ---------------------------------------------------------------------------
// 4. Interpretarea raspunsului
// ---------------------------------------------------------------------------
$iesire = [
    "status"    => "EROARE",
    "comanda"   => $cmd,
    "descriere" => "",
    "linii"     => [],
    "motiv"     => "",
    "ms"        => $ms,
    "logat"     => false
];

if ($raspuns === false || $raspuns === '') {
    $iesire["motiv"] = "ESP32 nu răspunde (" . ($eroareCone !== '' ? $eroareCone : 'fără răspuns') . "). Verifică alimentarea și rețeaua.";
} elseif ($codHttp === 403) {
    $iesire["motiv"] = "ESP32 a refuzat comanda (cheie sau IP-ul XAMPP diferit de cel din COD_ARDUINO.ino).";
} else {
    $date = json_decode($raspuns, true);
    if (!is_array($date) || !isset($date['status'])) {
        $iesire["motiv"] = "Răspuns invalid de la ESP32.";
    } else {
        $stari = ['OK', 'EROARE', 'NEVERIFICAT'];
        $iesire["status"]    = in_array($date['status'], $stari, true) ? $date['status'] : 'EROARE';
        $iesire["descriere"] = isset($date['descriere']) ? mb_substr((string) $date['descriere'], 0, 200) : '';
        $iesire["motiv"]     = isset($date['motiv']) ? mb_substr((string) $date['motiv'], 0, 300) : '';
        if (isset($date['linii']) && is_array($date['linii'])) {
            foreach (array_slice($date['linii'], 0, 12) as $linie) {
                $iesire["linii"][] = mb_substr((string) $linie, 0, 200);
            }
        }
        if (isset($date['ms'])) $iesire["ms"] = (int) $date['ms'];   // durata masurata de ESP32
    }
}

// ---------------------------------------------------------------------------
// 5. Istoric: se scrie AICI, pe server (nu de browser), ca sa nu poata fi falsificat
// ---------------------------------------------------------------------------
$rezumat = implode(' | ', $iesire["linii"]);
if ($iesire["motiv"] !== '') $rezumat .= ($rezumat !== '' ? ' | ' : '') . $iesire["motiv"];
$rezumat = mb_substr($rezumat, 0, 250);
$ipClient = $_SERVER['REMOTE_ADDR'];

$inserare = @$conn->prepare(
    "INSERT INTO log_comenzi (ip_utilizator, rol, comanda, rezultat, status) VALUES (?, ?, ?, ?, ?)"
);
if ($inserare) {
    $inserare->bind_param("sssss", $ipClient, $rol, $cmd, $rezumat, $iesire["status"]);
    if ($inserare->execute()) {
        $iesire["logat"] = true;
        // pastram doar ultimele $NR_ISTORIC inregistrari (aceeasi metoda ca la jurnal_ventilatie.php)
        $limita = (int) $NR_ISTORIC;
        $conn->query(
            "DELETE FROM log_comenzi WHERE id NOT IN
             (SELECT id FROM (SELECT id FROM log_comenzi ORDER BY id DESC LIMIT $limita) AS t)"
        );
    }
    $inserare->close();
}

trimite_json($iesire);
$conn->close();
?>
