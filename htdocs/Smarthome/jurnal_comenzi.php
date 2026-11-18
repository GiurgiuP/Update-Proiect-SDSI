<?php
/**
 * jurnal_comenzi.php
 * Citeste istoricul comenzilor de diagnostic (ultimele 10), pentru butonul
 * "Deschide Istoric Comenzi" din pagina "Stare Sistem".
 *
 * Doar admin (verificare pe server). Nu exista actiune de scriere aici: inregistrarile
 * sunt create exclusiv de diag.php, dupa ce comanda a fost executata.
 *
 * Raspuns: [ { "ip_utilizator": "...", "rol": "...", "comanda": "...",
 *              "rezultat": "...", "status": "...", "data_ora": "..." }, ... ]
 * La eroare: { "status": "error", "mesaj": "..." }
 */

header("Content-Type: application/json; charset=UTF-8");
header("Cache-Control: no-store");
include 'conectare.php';
include 'verifica_admin.php';

cere_admin($conn);

$rezultat = @$conn->query(
    "SELECT ip_utilizator, rol, comanda, rezultat, status, data_ora
     FROM log_comenzi ORDER BY id DESC LIMIT 10"
);

if ($rezultat === false) {
    echo json_encode([
        "status" => "error",
        "mesaj"  => "Tabela log_comenzi lipsește. Rulează log_comenzi.sql în phpMyAdmin."
    ]);
    $conn->close();
    exit();
}

$intrari = [];
while ($rand = $rezultat->fetch_assoc()) {
    $intrari[] = $rand;
}
echo json_encode($intrari);
$conn->close();
?>
