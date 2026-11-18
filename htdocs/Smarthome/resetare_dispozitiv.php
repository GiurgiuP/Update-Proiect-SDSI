<?php
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';

$userImplicit   = "admin";
$parolaHashuita = password_hash("1234", PASSWORD_DEFAULT);

$stmtUser = $conn->prepare("INSERT INTO setari (nume_setare, valoare) VALUES ('global_user', ?) ON DUPLICATE KEY UPDATE valoare=?");
$stmtUser->bind_param("ss", $userImplicit, $userImplicit);
$okUser = $stmtUser->execute();
$stmtUser->close();

$stmtParola = $conn->prepare("INSERT INTO setari (nume_setare, valoare) VALUES ('global_pass', ?) ON DUPLICATE KEY UPDATE valoare=?");
$stmtParola->bind_param("ss", $parolaHashuita, $parolaHashuita);
$okParola = $stmtParola->execute();
$stmtParola->close();

if ($okUser && $okParola) {
    echo json_encode(["status" => "success", "user" => "admin", "parola" => "1234", "mesaj" => "Dispozitiv resetat la valori implicite"]);
} else {
    echo json_encode(["status" => "error", "mesaj" => "Eroare la resetare in baza de date"]);
}

$conn->close();
?>