<?php
/**
 * conectare.php
 * Stabileste conexiunea cu baza de date MySQL prin extensia MySQLi.
 * Acest fisier este inclus de toate celelalte fisiere PHP prin include 'conectare.php'.
 * Nu returneaza nimic direct — expune variabila $conn folosita peste tot.
 */

$gazda     = "localhost";
$utilizator = "root";
$parola     = "";
$bazaDate   = "smarthome_db";

$conn = new mysqli($gazda, $utilizator, $parola, $bazaDate);

if ($conn->connect_error) {
    die("Eroare critica la conectarea cu Baza de Date: " . $conn->connect_error);
}

// Setam UTF-8 extins pentru suport diacritice (ă, î, ș, ț, â)
$conn->set_charset("utf8mb4");
?>