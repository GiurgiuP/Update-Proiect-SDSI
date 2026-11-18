<?php
/**
 * update_settings.php
 * Salveaza sau actualizeaza o setare in tabela `setari`.
 * Daca setarea exista => UPDATE, daca nu => INSERT.
 * Caz special: daca numele setarii este 'global_pass', parola este hash-uita automat.
 *
 * Parametri GET:
 *   ?nume=X     — numele setarii (ex: "vent_mod", "oras_vreme")
 *   ?valoare=Y  — valoarea de salvat
 *
 * Raspuns: { "status": "success" } sau { "status": "error", "mesaj": "..." }
 */

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';

$numeSetare  = isset($_GET['nume'])    ? trim($_GET['nume'])    : '';
$valoareNoua = isset($_GET['valoare']) ? trim($_GET['valoare']) : '';

if ($numeSetare === '' || $valoareNoua === '') {
    echo json_encode(["status" => "error", "mesaj" => "Date incomplete!"]);
    $conn->close();
    exit();
}

// Parola globala se salveaza intotdeauna hash-uita pentru securitate
if ($numeSetare === 'global_pass') {
    $valoareNoua = password_hash($valoareNoua, PASSWORD_DEFAULT);
}

// Verificam daca setarea exista deja in baza de date
$verificare = $conn->prepare("SELECT id FROM setari WHERE nume_setare = ?");
$verificare->bind_param("s", $numeSetare);
$verificare->execute();
$verificare->store_result();
$exista = $verificare->num_rows > 0;
$verificare->close();

// UPDATE daca exista, INSERT daca e noua
if ($exista) {
    $actiune = $conn->prepare("UPDATE setari SET valoare = ? WHERE nume_setare = ?");
    $actiune->bind_param("ss", $valoareNoua, $numeSetare);
} else {
    $actiune = $conn->prepare("INSERT INTO setari (nume_setare, valoare) VALUES (?, ?)");
    $actiune->bind_param("ss", $numeSetare, $valoareNoua);
}

if ($actiune->execute()) {
    echo json_encode(["status" => "success", "mesaj" => "Salvat cu succes!"]);
} else {
    echo json_encode(["status" => "error", "mesaj" => "Eroare la salvare in baza de date!"]);
}

$actiune->close();
$conn->close();
?>