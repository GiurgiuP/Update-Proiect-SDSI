<?php
/**
 * get_settings.php
 * Citeste toate setarile din tabela `setari` si le returneaza ca obiect JSON.
 * Folosit de interfata web la incarcare si de ESP32 pentru sincronizare periodica.
 * 
 * Raspuns: { "nume_setare": "valoare", ... }
 * Exemplu: { "vent_mod": "AUTO", "vent_prag": "26", "oras_vreme": "Timisoara" }
 */

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';

$setari = [];

$rezultat = $conn->query("SELECT nume_setare, valoare FROM setari");

if ($rezultat && $rezultat->num_rows > 0) {
    while ($rand = $rezultat->fetch_assoc()) {
        $setari[$rand["nume_setare"]] = $rand["valoare"];
    }
}

echo json_encode($setari);
$conn->close();
?>